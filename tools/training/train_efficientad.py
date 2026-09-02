# EfficientAD 训练 + 测试 + ONNX 导出脚本（Phase 2 训练侧）
#
# 用法（在仓库根目录）：
#   tools/training/.venv/Scripts/python.exe tools/training/train_efficientad.py metal_nut
#   tools/training/.venv/Scripts/python.exe tools/training/train_efficientad.py screw --steps 70000
#   tools/training/.venv/Scripts/python.exe tools/training/train_efficientad.py metal_nut --test-only
#
# 产物：models/<category>/weights/onnx/<category>.onnx（models/ 已 gitignore，不入库）。
# 说明：
# - 数据集用 anomalib 内置 MVTecAD datamodule，root 指向仓库根（其下 metal_nut/、screw/
#   目录结构天然匹配），数据集只读。
# - EfficientAD 要求 train_batch_size=1（模型内部硬约束）；输入 256×256，
#   ImageNet 归一化在模型 forward 内部，ONNX 输入即 0~1 浮点 RGB。
# - 4GB 显存实测可跑 batch=1 @ 256×256；max_steps 默认 70000（论文值）。
# - anomalib 默认只在 engine.test() 打图像/像素 AUROC、F1；fit 日志只有 train_* loss。
#   EfficientAD 的分位数标定在 on_validation_start，test 不会自动跑，所以先 validate 再 test。
import argparse
import sys
from pathlib import Path

from anomalib.data import MVTecAD
from anomalib.deploy import ExportType
from anomalib.engine import Engine
from anomalib.models import EfficientAd

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def default_ckpt(category: str) -> Path:
    # 子类化 datamodule 后 lightning 目录名是 MvtecTopLevel 而非 MVTecAD，按文件搜更稳。
    root = PROJECT_ROOT / "models" / category
    latest = list(root.glob("**/latest/weights/lightning/model.ckpt"))
    if latest:
        return latest[0].resolve()
    ckpts = list(root.glob("**/weights/lightning/model.ckpt"))
    if ckpts:
        return max(ckpts, key=lambda p: p.stat().st_mtime)
    return (
        root
        / "EfficientAd"
        / "MVTecAD"
        / category
        / "latest"
        / "weights"
        / "lightning"
        / "model.ckpt"
    )


def export_onnx(engine: Engine, model: EfficientAd, category: str, image_size: int, ckpt_path: Path | None) -> Path:
    # test()/validate() 之后部分 buffer 是 inference tensor，torch.onnx 去重 initializer 会报
    # RuntimeError: Inference tensors do not track version counter。
    # 从磁盘 ckpt 重新 load 再导出（与第一回 metal_nut「fit 后立刻 export」等价）。
    if ckpt_path is None or not ckpt_path.exists():
        raise FileNotFoundError(f"没有可导出的 checkpoint: {ckpt_path}")
    onnx_path = engine.export(
        model=model,
        export_type=ExportType.ONNX,
        export_root=PROJECT_ROOT / "models" / category,
        model_file_name=category,
        input_size=(image_size, image_size),
        ckpt_path=ckpt_path,
    )
    print(f"ONNX exported: {onnx_path}")
    return onnx_path


class MvtecTopLevel(MVTecAD):
    """按 image_path 去重，防止解压套层（metal_nut/metal_nut）把样本数翻倍。

    anomalib 的 MVTecAD 用 glob('**/*') 扫图。仓库里那层套目录已删掉；
    若再次解压套进去，这里仍会丢掉重复路径，避免 train/test 数量翻倍。
    """

    def _setup(self, stage: str | None = None) -> None:
        super()._setup(stage)
        for attr in ("train_data", "test_data"):
            ds = getattr(self, attr, None)
            if ds is None:
                continue
            before = len(ds.samples)
            ds.samples = ds.samples.drop_duplicates(subset=["image_path"]).reset_index(drop=True)
            if len(ds.samples) != before:
                print(f"{attr}: dropped nested duplicates {before} -> {len(ds.samples)}")


def make_datamodule(category: str) -> MVTecAD:
    return MvtecTopLevel(
        root=PROJECT_ROOT,
        category=category,
        train_batch_size=1,  # EfficientAD 内部硬约束
        eval_batch_size=1,
        num_workers=0,  # Windows 下多进程 dataloader 易卡死，保守取 0
    )


def make_model() -> EfficientAd:
    return EfficientAd(
        # imagenette 惩罚项数据集默认下在 ./datasets/，改到工具缓存目录避免污染仓库根
        imagenet_dir=Path(__file__).resolve().parent / ".cache" / "imagenette",
        visualizer=False,  # 评估只打分，不写 anomalib 可视化图
    )


def write_test_metrics(results: list, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = []
    if results:
        for key, value in results[0].items():
            lines.append(f"{key}: {value}")
    else:
        lines.append("(empty — anomalib/Lightning 未返回指标字典)")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(f"Test metrics written: {path}")


def run_test(engine: Engine, model: EfficientAd, datamodule: MVTecAD, ckpt_path: Path | str | None) -> list:
    # 先 validate：EfficientAD.on_validation_start 会用良品图算分位数，写入 model.quantiles。
    # 再 test 且不再 reload ckpt，否则刚算的分位数会被 checkpoint 里的旧值覆盖。
    engine.validate(model=model, datamodule=datamodule, ckpt_path=ckpt_path)
    results = engine.test(model=model, datamodule=datamodule)
    print("Test results:")
    if results:
        for key, value in results[0].items():
            print(f"  {key}: {value}")
    else:
        print("  (empty — anomalib/Lightning 未返回指标字典)")
    return results


def main() -> int:
    parser = argparse.ArgumentParser(description="Train EfficientAD on one MVTec AD category and export ONNX.")
    parser.add_argument("category", choices=["metal_nut", "screw"])
    parser.add_argument("--steps", type=int, default=70000, help="max_steps（默认 70000，论文值）")
    parser.add_argument("--image-size", type=int, default=256)
    parser.add_argument(
        "--test-only",
        action="store_true",
        help="跳过训练/导出，只对已有 checkpoint 跑 anomalib 测试指标",
    )
    parser.add_argument(
        "--export-only",
        action="store_true",
        help="跳过训练/测试，只从已有 checkpoint 导出 ONNX（test 后直接 export 会因 inference tensor 失败）",
    )
    parser.add_argument(
        "--ckpt",
        type=Path,
        default=None,
        help="--test-only / --export-only 用的 checkpoint；默认搜索 models/<cat>/**/model.ckpt",
    )
    args = parser.parse_args()

    datamodule = make_datamodule(args.category)
    model = make_model()
    engine = Engine(
        max_steps=args.steps,
        accelerator="gpu",
        devices=1,
        default_root_dir=str(PROJECT_ROOT / "models" / args.category),
    )

    metrics_path = PROJECT_ROOT / "models" / args.category / "test_metrics.txt"

    if args.test_only and args.export_only:
        print("ERROR: --test-only 与 --export-only 不能同时用", file=sys.stderr)
        return 2

    if args.test_only:
        ckpt = args.ckpt or default_ckpt(args.category)
        if not ckpt.exists():
            print(f"ERROR: checkpoint 不存在: {ckpt}", file=sys.stderr)
            return 2
        print(f"Test-only from checkpoint: {ckpt}")
        results = run_test(engine, model, datamodule, ckpt)
        write_test_metrics(results, metrics_path)
        return 0

    if args.export_only:
        ckpt = args.ckpt or default_ckpt(args.category)
        if not ckpt.exists():
            print(f"ERROR: checkpoint 不存在: {ckpt}", file=sys.stderr)
            return 2
        print(f"Export-only from checkpoint: {ckpt}")
        export_onnx(engine, model, args.category, args.image_size, ckpt)
        return 0

    engine.fit(model=model, datamodule=datamodule)
    # fit 刚结束时内存里已有权重；validate 不再传 ckpt，避免重载覆盖当前分位数。
    results = run_test(engine, model, datamodule, ckpt_path=None)
    write_test_metrics(results, metrics_path)
    # 必须从 ckpt 重新 load 再导出，不能拿 test 后的内存模型直接 to_onnx。
    export_onnx(engine, model, args.category, args.image_size, default_ckpt(args.category))
    return 0


if __name__ == "__main__":
    sys.exit(main())
