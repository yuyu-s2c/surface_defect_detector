# surface_defect_detector

工业产品表面缺陷检测桌面工具（Qt6 Quick / QML + OpenCV + EfficientAD ONNX）。

阶段状态、架构、构建命令和实测指标见 [DEVELOPMENT.md](DEVELOPMENT.md)。
给 AI 代理的约定见 [AGENTS.md](AGENTS.md)。训练见 [tools/training/TRAIN.md](tools/training/TRAIN.md)。

当前进度：Phase 1～3.7 已完成（DirectML / 图像级分数过线 / 新类别零改代码接入 /
模拟取流 / 班次落盘 / 检测·分析两态布局）。另补离线工位闭环（开线自检 / 模拟 PLC·DO /
节拍与直通率 / 连续 NG 联锁）。Phase 4 产线对接等实机，只换 `CameraSource`
并加一个真实 DO 的 `IRejectSink`。**本机相机保持离线，不要接假直播。**

## 无相机 5 分钟演示

Windows 本机已构建、数据集在仓库根、`models/metal_nut/.../metal_nut.onnx` 可选（无 ONNX 用传统 CV）。

1. 启动 `./build/surface_defect_detector.exe`，打开数据集根（含 `metal_nut/`）。
2. 顶栏保持 **检测** 态，左侧点 `metal_nut`。引擎可选 EfficientAD 或传统 CV。
3. 顶栏应看到 **相机离线 / 模拟取流 FolderSource / 模拟 PLC·DO0.0**，没有「相机已连接」。
4. 右侧填工单（默认同日 `WO-YYYYMMDD`），连续 NG 默认 8。
5. 点 **模拟开线**（或空格）→ 看开线自检 → **确认开线**。不会启动 `CameraSource`。
6. 结果轨：合格/不合格、直通率、节拍、连续 NG 条、模拟 DO0.0 脉冲。约 8 张连续不合格后 **联锁停线**。
7. 停线后打开 `_sessions/<班次>/`：`session.csv`、`rejects.csv`、`do_map.csv`、`do_pulses.csv`。
8. 想跑完整一类 test：把连续 NG 调到 **0** 再开线（与 `--live-smoke` 相同，联锁关闭）。

Phase 4 到货后只换两处：实现 `CameraSource`（`overflowPolicy()` 已是丢最旧帧），再实现一个真实 DO 的 `IRejectSink` 加进 `CompositeRejectSink`。不要改 `SimulatedDoSink` 当现场总线。

```bash
# GUI
./build/surface_defect_detector.exe

# 批处理（与 DEVELOPMENT.md 第 4 节同口径，不读 GUI 设置 / 工单 / 联锁）
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
```

GUI 为暗色质检台（QML）：红=GT 标注，绿=检测结果。顶栏分检测/分析两态；检测态右侧结果轨
给出合格/不合格、直通率/节拍与本班 NG / 模拟 DO 列表。取流按设定 FPS 用文件夹源吐 `test/`，
不合格打 `[DO] REJECT` / `[PLC-SIM] DO0.0` 并写入数据集根 `_sessions/`。DL 默认 DirectML（失败回 CPU）。
切深度学习时若尚无该 EP 的标定缓存，会跑一遍 `train/good`（DML 约十几秒，不是训练）；
之后启动复用模型旁的 `.calib.json`（v3，键含 EP）。`--batch` 图像级默认分数过线，
另打面积门对照列；不读 GUI 设置。`--provider cpu|dml|auto`。`--overflow block|drop`
只影响取流队列（Folder 默认 block，保证跑完一类 test）。`--live-smoke` 关闭连续 NG 联锁。

新类别不改代码：在数据集根放入 `<类>/train/good` + `<类>/test/...`，DL 再放
`models/<类>/weights/onnx/<类>.onnx`（标定至少 3 张良品）。无专表工作点时用 k=3 /
面积门 1000。步骤与验收见 [DEVELOPMENT.md](DEVELOPMENT.md) 工作项 3。
