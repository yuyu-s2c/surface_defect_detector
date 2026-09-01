# EfficientAD 训练 + ONNX 导出脚本（Phase 2 训练侧）
#
# 用法（在仓库根目录）：
#   tools/training/.venv/Scripts/python.exe tools/training/train_efficientad.py metal_nut
#   tools/training/.venv/Scripts/python.exe tools/training/train_efficientad.py screw --steps 70000
#
# 产物：models/<category>/weights/onnx/<category>.onnx（models/ 已 gitignore，不入库）。
# 说明：
# - 数据集用 anomalib 内置 MVTecAD datamodule，root 指向仓库根（其下 metal_nut/、screw/
#   目录结构天然匹配），数据集只读。
# - EfficientAD 要求 train_batch_size=1（模型内部硬约束）；输入 256×256，
#   ImageNet 归一化在模型 forward 内部，ONNX 输入即 0~1 浮点 RGB。
# - 4GB 显存实测可跑 batch=1 @ 256×256；max_steps 默认 70000（论文值）。
import argparse
import sys
from pathlib import Path

from anomalib.data import MVTecAD
from anomalib.deploy import ExportType
from anomalib.engine import Engine
from anomalib.models import EfficientAd

PROJECT_ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description="Train EfficientAD on one MVTec AD category and export ONNX.")
    parser.add_argument("category", choices=["metal_nut", "screw"])
    parser.add_argument("--steps", type=int, default=70000, help="max_steps（默认 70000，论文值）")
    parser.add_argument("--image-size", type=int, default=256)
    args = parser.parse_args()

    datamodule = MVTecAD(
        root=PROJECT_ROOT,
        category=args.category,
        train_batch_size=1,   # EfficientAD 内部硬约束
        eval_batch_size=1,
        num_workers=0,        # Windows 下多进程 dataloader 易卡死，保守取 0
    )
    model = EfficientAd(
        # imagenette 惩罚项数据集默认下在 ./datasets/，改到工具缓存目录避免污染仓库根
        imagenet_dir=Path(__file__).resolve().parent / ".cache" / "imagenette",
    )

    # 训练中间产物（ckpt 等）放 models/<category>/ 下，与最终 ONNX 同目录管理
    engine = Engine(
        max_steps=args.steps,
        accelerator="gpu",
        devices=1,
        default_root_dir=str(PROJECT_ROOT / "models" / args.category),
    )
    engine.fit(model=model, datamodule=datamodule)

    onnx_path = engine.export(
        model=model,
        export_type=ExportType.ONNX,
        export_root=PROJECT_ROOT / "models" / args.category,
        model_file_name=args.category,
        input_size=(args.image_size, args.image_size),
    )
    print(f"ONNX exported: {onnx_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
