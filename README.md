# surface_defect_detector

工业产品表面缺陷检测工作站（Qt6 Quick / QML + OpenCV + EfficientAD ONNX）。

- 阶段、架构、实测指标：[DEVELOPMENT.md](DEVELOPMENT.md)
- 阶段实施日记：[DEVELOPMENT-HISTORY.md](DEVELOPMENT-HISTORY.md)
- AI 代理约定：[AGENTS.md](AGENTS.md)
- 训练：[tools/training/TRAIN.md](tools/training/TRAIN.md)

P1～P3.8、离线工位闭环、P4.0 本机摄像头、工位可读性已完成。完整 Phase 4（海康 + 真实 DO）等实机：只换 `CameraSource`，再加一个真实 DO 的 `IRejectSink`。不要把 `CameraSource` 当假直播。

识别数字以 `--batch` / `--live-smoke` 为准。本机摄像头相对 `metal_nut` / `screw` 是分布外，整班 NG 是预期。

## 无相机 5 分钟演示

Windows 本机已构建，数据集在仓库根（含 `metal_nut/`）。`models/metal_nut/.../metal_nut.onnx` 可选，没有就用传统 CV。

1. 启动 `./build/surface_defect_detector.exe`，本机登录（首次：`admin` / `engineer` / `operator`，密码均为 `123456`）。操作员只跑检测台；改参数请用工艺员或管理员。
2. 打开数据集根。顶栏保持 **检测**，左侧点 `metal_nut`。引擎在右侧配方上方切 **传统 CV / EfficientAD**（分析态改在顶栏切；操作员不能切）。
3. 身份条应为 **相机离线 / 模拟取流 · FolderSource / 模拟 PLC · DO0.0**，不要出现「相机已连接」。
   顶栏可切 **文件夹 / 本机摄像头**（默认文件夹）。切到摄像头后画布立即预览（无检测），身份条改为 **本机摄像头 · OpenCV / 预览中 · WebcamSource**，并出现分布外横幅。开线才检测。
4. 右侧工单默认同日 `WO-YYYYMMDD`；操作员名取登录显示名，不能手填。连续 NG（默认 8）、帧率（1–15）在同一配方区（工艺员/管理员可改）。
5. 点 **模拟开线**（或空格）→ 开线自检 → **确认开线**。不会启动 `CameraSource`。
   摄像头模式下按钮为 **开线**；预览已出帧则不再探活，否则自检会短开短关。
6. 结果轨看合格/不合格、直通率、节拍、连续 NG。约 8 张连续不合格后 **联锁停线**。
7. 停线后打开 `_sessions/<班次>/`：`session.csv`、`rejects.csv`、`do_map.csv`、`do_pulses.csv`。
8. 要跑完整一类 `test/`：切回 **文件夹**，连续 NG 调到 **0** 再开线（与 `--live-smoke` 相同，联锁关闭）。

完整 Phase 4 到货后只换两处：实现 `CameraSource`（`overflowPolicy()` 已是丢最旧帧），再实现一个真实 DO 的 `IRejectSink` 加进 `CompositeRejectSink`。不要改 `SimulatedDoSink` 当现场总线。

## 命令行

```bash
# GUI
./build/surface_defect_detector.exe

# 批处理（与 DEVELOPMENT.md 第 4 节同口径，不读 GUI 设置 / 工单 / 联锁）
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 15 --seconds 8
```

- `--batch` 图像级默认分数过线，另打面积门对照列。DL 默认 DirectML（失败回 CPU）；`--provider cpu|dml|auto`。
- `--overflow block|drop` 只影响取流队列（Folder 默认 block，保证跑完一类 test）。
- `--live-smoke` 关闭连续 NG 联锁，应跑完一类 test 后退出 0。
- `--webcam-smoke` 限时打开本机摄像头（默认 8 秒）后退出 0；打不开设备退出 2，不崩。停线若堵在 DirectShow `read()`，超时返回，不挂死进程。

构建、标定缓存、新类别接入见 [DEVELOPMENT.md](DEVELOPMENT.md) 第 2 / 5 节。新类别不改代码：数据集根放入 `<类>/train/good` + `<类>/test/...`，DL 再放 `models/<类>/weights/onnx/<类>.onnx`（标定至少 3 张良品）。无专表工作点时用 k=3 / 面积门 1000。
