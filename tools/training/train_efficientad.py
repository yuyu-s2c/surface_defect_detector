# EfficientAD 训练 + 测试 + ONNX 导出脚本（Phase 2 训练侧）
#
# 用法（在仓库根目录）：
#   python tools/training/train_efficientad.py metal_nut
#   python tools/training/train_efficientad.py screw --steps 70000
#   python tools/training/train_efficientad.py screw --model-size medium --steps 70000
#   python tools/training/train_efficientad.py screw --model-size medium --test-only
#
# 产物（models/ 已 gitignore，不入库）：
#   S：models/<category>/weights/onnx/<category>.onnx   （现网，不要覆盖）
#   M：models/<category>_m/weights/onnx/<category>.onnx （对照实验）
# 说明：
# - 数据集用 anomalib 内置 MVTecAD datamodule，root 默认仓库根（其下 metal_nut/、screw/
#   目录结构天然匹配），数据集只读。AutoDL 可用 --data-root 指到数据盘。
# - EfficientAD 要求 train_batch_size=1（模型内部硬约束）；输入默认 256×256，
#   ImageNet 归一化在模型 forward 内部，ONNX 输入即 0~1 浮点 RGB。
# - S @ 256、batch=1 在 4GB 上能训；M 走云卡。max_steps 默认 70000（论文值）。
# - anomalib 默认只在 engine.test() 打图像/像素 AUROC、F1；fit 日志只有 train_* loss。
#   EfficientAD 的分位数标定在 on_validation_start，test 不会自动跑，所以先 validate 再 test。
import argparse
import sys
from pathlib import Path

import torch
from anomalib.data import MVTecAD
from anomalib.deploy import ExportType
from anomalib.engine import Engine
from anomalib.models import EfficientAd

PROJECT_ROOT = Path(__file__).resolve().parents[2]

# anomalib EfficientAdModelSize 的值是 small / medium，短写 s / m 在这里归一。
_SIZE_ALIAS = {"s": "small", "small": "small", "m": "medium", "medium": "medium"}


def parse_model_size(raw: str) -> str:
    key = raw.strip().lower()
    if key not in _SIZE_ALIAS:
        raise argparse.ArgumentTypeError("model-size 只认 s/small/m/medium")
    return _SIZE_ALIAS[key]


def default_num_workers() -> int:
    # Windows 多进程 dataloader 易卡死；Linux / AutoDL 用 4。
    return 0 if sys.platform.startswith("win") else 4


def run_dir(category: str, model_size: str, data_root: Path) -> Path:
    # M 不写进 models/screw/，避免覆盖现网 S 的 ckpt / ONNX。
    name = f"{category}_m" if model_size == "medium" else category
    return data_root / "models" / name


def default_ckpt(root: Path) -> Path:
    # 子类化 datamodule 后 lightning 目录名是 MvtecTopLevel 而非 MVTecAD，按文件搜更稳。
    latest = list(root.glob("**/latest/weights/lightning/model.ckpt"))
    if latest:
        return latest[0].resolve()
    ckpts = list(root.glob("**/weights/lightning/model.ckpt"))
    if ckpts:
        return max(ckpts, key=lambda p: p.stat().st_mtime)
    return root / "EfficientAd" / "MVTecAD" / "latest" / "weights" / "lightning" / "model.ckpt"


def export_onnx(
    engine: Engine,
    model: EfficientAd,
    category: str,
    image_size: int,
    ckpt_path: Path | None,
    out_root: Path,
) -> Path:
    # test()/validate() 之后部分 buffer 是 inference tensor，torch.onnx 去重 initializer 会报
    # RuntimeError: Inference tensors do not track version counter。
    # 从磁盘 ckpt 重新 load 再导出（与第一回 metal_nut「fit 后立刻 export」等价）。
    if ckpt_path is None or not ckpt_path.exists():
        raise FileNotFoundError(f"没有可导出的 checkpoint: {ckpt_path}")
    onnx_path = engine.export(
        model=model,
        export_type=ExportType.ONNX,
        export_root=out_root,
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


def make_datamodule(category: str, data_root: Path, num_workers: int) -> MVTecAD:
    return MvtecTopLevel(
        root=data_root,
        category=category,
        train_batch_size=1,  # EfficientAD 内部硬约束
        eval_batch_size=1,
        num_workers=num_workers,
    )


def make_model(model_size: str, imagenet_dir: Path) -> EfficientAd:
    return EfficientAd(
        # imagenette 惩罚项默认下在 ./datasets/，改到工具缓存目录避免污染仓库根
        imagenet_dir=imagenet_dir,
        model_size=model_size,  # "small" / "medium"，与 anomalib 枚举值一致
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


def print_gpu() -> int:
    if not torch.cuda.is_available():
        print("ERROR: 没有 CUDA，拒绝在 CPU 上跑 70k step", file=sys.stderr)
        return 2
    props = torch.cuda.get_device_properties(0)
    print(
        f"GPU: {torch.cuda.get_device_name(0)}  "
        f"VRAM={props.total_memory / (1024 ** 3):.1f} GB  "
        f"torch={torch.__version__}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Train EfficientAD on one MVTec AD category and export ONNX.")
    parser.add_argument("category", choices=["metal_nut", "screw"])
    parser.add_argument("--steps", type=int, default=70000, help="max_steps（默认 70000，论文值）")
    parser.add_argument("--image-size", type=int, default=256)
    parser.add_argument(
        "--model-size",
        type=parse_model_size,
        default="small",
        help="s/small 或 m/medium。M 写到 models/<类>_m/，不覆盖现网 S",
    )
    parser.add_argument(
        "--data-root",
        type=Path,
        default=PROJECT_ROOT,
        help="含 <类>/train/good 的根目录，默认仓库根；AutoDL 可指到 /root/autodl-tmp/sdd",
    )
    parser.add_argument(
        "--num-workers",
        type=int,
        default=default_num_workers(),
        help="DataLoader workers。Windows 默认 0，Linux 默认 4",
    )
    parser.add_argument(
        "--imagenet-dir",
        type=Path,
        default=Path(__file__).resolve().parent / ".cache" / "imagenette",
        help="imagenette 惩罚项缓存。没有则训练开始时下载（约 1.5GB）",
    )
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
        "--resume",
        action="store_true",
        help="从该次运行目录里最新的 ckpt 接着训（AutoDL 断线后用）",
    )
    parser.add_argument(
        "--ckpt",
        type=Path,
        default=None,
        help="--test-only / --export-only / --resume 用的 checkpoint；默认搜本次 run 目录",
    )
    args = parser.parse_args()
    data_root = args.data_root.resolve()
    out_root = run_dir(args.category, args.model_size, data_root)
    out_root.mkdir(parents=True, exist_ok=True)

    cat_dir = data_root / args.category
    if not (cat_dir / "train" / "good").is_dir():
        print(f"ERROR: 找不到数据集 {cat_dir / 'train' / 'good'}", file=sys.stderr)
        return 2

    print(
        f"category={args.category}  model_size={args.model_size}  "
        f"image_size={args.image_size}  steps={args.steps}  "
        f"num_workers={args.num_workers}"
    )
    print(f"data_root={data_root}")
    print(f"out_root={out_root}")

    if print_gpu() != 0:
        return 2

    datamodule = make_datamodule(args.category, data_root, args.num_workers)
    model = make_model(args.model_size, args.imagenet_dir)
    engine = Engine(
        max_steps=args.steps,
        accelerator="gpu",
        devices=1,
        default_root_dir=str(out_root),
    )

    metrics_path = out_root / "test_metrics.txt"

    if args.test_only and args.export_only:
        print("ERROR: --test-only 与 --export-only 不能同时用", file=sys.stderr)
        return 2

    if args.test_only:
        ckpt = args.ckpt or default_ckpt(out_root)
        if not ckpt.exists():
            print(f"ERROR: checkpoint 不存在: {ckpt}", file=sys.stderr)
            return 2
        print(f"Test-only from checkpoint: {ckpt}")
        results = run_test(engine, model, datamodule, ckpt)
        write_test_metrics(results, metrics_path)
        return 0

    if args.export_only:
        ckpt = args.ckpt or default_ckpt(out_root)
        if not ckpt.exists():
            print(f"ERROR: checkpoint 不存在: {ckpt}", file=sys.stderr)
            return 2
        print(f"Export-only from checkpoint: {ckpt}")
        export_onnx(engine, model, args.category, args.image_size, ckpt, out_root)
        return 0

    resume_ckpt = None
    if args.resume or args.ckpt is not None:
        resume_ckpt = args.ckpt or default_ckpt(out_root)
        if not resume_ckpt.exists():
            print(f"ERROR: --resume 找不到 checkpoint: {resume_ckpt}", file=sys.stderr)
            return 2
        print(f"Resume from checkpoint: {resume_ckpt}")

    engine.fit(model=model, datamodule=datamodule, ckpt_path=resume_ckpt)
    # fit 刚结束时内存里已有权重；validate 不再传 ckpt，避免重载覆盖当前分位数。
    results = run_test(engine, model, datamodule, ckpt_path=None)
    write_test_metrics(results, metrics_path)
    # 必须从 ckpt 重新 load 再导出，不能拿 test 后的内存模型直接 to_onnx。
    export_onnx(engine, model, args.category, args.image_size, default_ckpt(out_root), out_root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
