# EfficientAD 训练命令

过程说明（为什么、日志、调参记录）见 [TRAINING_NOTES.md](TRAINING_NOTES.md)。
云卡训 **EfficientAD-M**（screw @ 256，不覆盖现网 S）见 [AUTODL.md](AUTODL.md)。

先 `cd` 到仓库根目录。下面每一块都是**一整条命令**，请整段复制一次回车，不要拆行。

venv、imagenette、teacher 权重已就绪。步数 70000，不要让系统休眠，也不要同时占用 GPU。

Phase 2 已收住像素工作点：metal_nut `k=3`/叠加面积 1000，screw `k=1`/叠加面积 300。
Phase 3.6 起图像级判定是 `max(热图) >= mean+kσ`，面积门只影响绿叠加。
现网权重是 EfficientAD-**S**。M 的产物在 `models/<类>_m/`，对照完再决定要不要换工位 ONNX。

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

## 训 EfficientAD-M（screw，不覆盖现网 S）

本机 4GB 不要跑这一条。AutoDL 步骤抄 [AUTODL.md](AUTODL.md)。Linux 一条：

```bash
bash tools/training/train_screw_m.sh
```

产物：`models/screw_m/weights/onnx/screw.onnx`、`models/screw_m/test_metrics.txt`。
C++ 仍读 `models/screw/weights/onnx/screw.onnx`，对照时先备份 S 再复制，见 AUTODL.md 第 6 节。

`--model-size medium` 的 `--test-only` / `--export-only` / `--resume` 都在 `models/screw_m/` 里找 ckpt，不会误用现网 S。

## 训完看这些

- 终端末尾应有 `image_AUROC` / `pixel_AUROC` / F1 表
- `ONNX exported: ...\models\screw\weights\onnx\screw.onnx`（S）或 `...\models\screw_m\...`（M）
- `models/screw/test_metrics.txt` 或 `models/screw_m/test_metrics.txt`
- 对应目录的 `train.log`

## 训完后的 C++ 批处理

默认 DirectML（失败回 CPU）。C++ ORT 是 DirectML 1.24.4，缺包时先跑 `tools/fetch_onnxruntime_dml.ps1`。

```powershell
.\build\surface_defect_detector.exe --batch screw --engine dl
.\build\surface_defect_detector.exe --batch screw --engine dl --provider cpu
```

screw 的 DL：像素/图像阈都是 `均值+1.0σ`，叠加面积门 300（metal_nut 仍是 3σ / 1000）。
`--batch` 图像级默认分数过线，另打面积门对照列。改 k 后必须重新编译再跑批处理（不必重训）。

## 只测已有 checkpoint（不训练）

```powershell
$env:PYTHONUNBUFFERED = "1"; tools\training\.venv\Scripts\python.exe -u tools\training\train_efficientad.py screw --test-only
# M：末尾加 --model-size medium（在 models/screw_m/ 里找 ckpt）
```

## 只从 checkpoint 导出 ONNX（不训练）

test 跑完后直接 export 会报 `Inference tensors do not track version counter`。训练已成功时只补导出：

```powershell
$env:PYTHONUNBUFFERED = "1"; tools\training\.venv\Scripts\python.exe -u tools\training\train_efficientad.py screw --export-only
```

metal_nut 把 `screw` 换成 `metal_nut` 即可。

本脚本 `category` 只认 `metal_nut` / `screw`（anomalib `MVTecAD` 官方类名）。C++ 侧加一类
产品不改此脚本：按 MVTec 布局丢目录 + 约定 ONNX 即可，见 DEVELOPMENT.md 第 5 节。
不要为刷表再训 hazelnut / bottle。
