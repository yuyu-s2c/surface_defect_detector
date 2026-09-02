# EfficientAD 训练命令

过程说明（为什么、日志、调参记录）见 [TRAINING_NOTES.md](TRAINING_NOTES.md)。

先 `cd` 到仓库根目录。下面每一块都是**一整条命令**，请整段复制一次回车，不要拆行。

venv、imagenette、teacher 权重已就绪。步数 70000，不要让系统休眠，也不要同时占用 GPU。

Phase 2 已收住：metal_nut `k=3`/面积 1000，screw `k=1`/面积 300。下面命令用于复现或重训。

## 训 screw

PowerShell（推荐：跑脚本，避免命令被拆行）：

```powershell
cd D:\C++\QtProject\surface_defect_detector
powershell -File tools\training\train_screw.ps1
```

或手动一条命令（必须含 `screw`，从 `tools\` 一直复制到 `train.log`）：

```powershell
$env:PYTHONUNBUFFERED = "1"; tools\training\.venv\Scripts\python.exe -u tools\training\train_efficientad.py screw --steps 70000 2>&1 | Tee-Object -FilePath models\screw\train.log
```

Git Bash：

```bash
cd /d/C++/QtProject/surface_defect_detector && PYTHONUNBUFFERED=1 tools/training/.venv/Scripts/python.exe -u tools/training/train_efficientad.py screw --steps 70000 2>&1 | tee models/screw/train.log
```

## 训完看这些

- 终端末尾应有 `image_AUROC` / `pixel_AUROC` / F1 表
- `ONNX exported: ...\models\screw\weights\onnx\screw.onnx`
- `models/screw/test_metrics.txt`
- `models/screw/train.log`

## 训完后的 C++ 批处理

```powershell
.\build\surface_defect_detector.exe --batch screw --engine dl
```

screw 的 DL：阈值 `均值+1.0σ`，图像级面积门 300（metal_nut 仍是 3σ / 1000）。改后必须重新编译再跑批处理。

## 只测已有 checkpoint（不训练）

```powershell
$env:PYTHONUNBUFFERED = "1"; tools\training\.venv\Scripts\python.exe -u tools\training\train_efficientad.py screw --test-only
```

## 只从 checkpoint 导出 ONNX（不训练）

test 跑完后直接 export 会报 `Inference tensors do not track version counter`。训练已成功时只补导出：

```powershell
$env:PYTHONUNBUFFERED = "1"; tools\training\.venv\Scripts\python.exe -u tools\training\train_efficientad.py screw --export-only
```

metal_nut 把 `screw` 换成 `metal_nut` 即可。
