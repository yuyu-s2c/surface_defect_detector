# surface_defect_detector

Industrial surface-defect inspection workstation (Qt6 Quick / QML + OpenCV + EfficientAD ONNX).

- Phases, architecture, measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md)
- Phase diary: [DEVELOPMENT-HISTORY.md](DEVELOPMENT-HISTORY.md)
- Agent conventions: [AGENTS.md](AGENTS.md)
- Training: [tools/training/TRAIN.md](tools/training/TRAIN.md)

Phases 1–3.8, the offline station loop, P4.0 laptop webcam, and the workstation-readability pass are done. Full Phase 4 (Hikvision + real DO) still waits on hardware: implement `CameraSource` and add a real-DO `IRejectSink`. Do not treat `CameraSource` as a fake live feed.

Metrics come from `--batch` / `--live-smoke`. Laptop-webcam frames are out-of-distribution vs `metal_nut` / `screw`; an all-NG shift is expected.

## 5-minute demo with no industrial camera

Windows box, repo root as the dataset root (`metal_nut/` present). ONNX under `models/metal_nut/...` is optional (traditional CV works without it). The GUI is Chinese; quoted labels below match the running app.

1. Run `./build/surface_defect_detector.exe` and sign in locally (first run: `admin` / `engineer` / `operator`, password equals username). Operators stay on Inspect; change parameters as engineer or admin.
2. Open the dataset root. Stay in **检测** (Inspect). Select `metal_nut`. Switch **传统 CV / EfficientAD** above the recipe on the right rail (in **分析** / Analyze the engine switch is in the header; operators cannot switch).
3. The identity bar should read **相机离线 / 模拟取流 · FolderSource / 模拟 PLC · DO0.0**, never “camera connected”. The header switches **文件夹 / 本机摄像头** (folder is default). Webcam shows a no-detect preview at once, the bar becomes **本机摄像头 · OpenCV / 预览中 · WebcamSource**, and the canvas shows an out-of-distribution banner. Detection starts only after 开线.
4. Work order defaults to `WO-YYYYMMDD`. Operator name comes from the signed-in display name (not free text). Consecutive-NG defaults to 8; target FPS (1–15) is in the same recipe block (engineer/admin can edit).
5. Click **模拟开线** (or Space) → self-check → confirm. `CameraSource` is not started. Webcam mode uses **开线**; if preview already has a frame, self-check does not probe again.
6. Watch OK/NG, yield, takt, and the consecutive-NG bar. After 8 consecutive NG the line **interlocks**.
7. Open `_sessions/<shift>/`: `session.csv`, `rejects.csv`, `do_map.csv`, `do_pulses.csv`.
8. To run a full `test/` category: switch back to **文件夹**, set consecutive NG to **0** (same as `--live-smoke`).

Phase 4 is still only two swaps: implement `CameraSource` (`overflowPolicy()` already returns drop-oldest) and add a real DO sink to `CompositeRejectSink`. Do not turn `SimulatedDoSink` into a fieldbus.

## CLI

```bash
./build/surface_defect_detector.exe
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 15 --seconds 8
```

`--batch` uses the same metrics as DEVELOPMENT.md §4 and does not read GUI settings, work orders, or the interlock. Image-level defaults to score-over-threshold (area gate is a comparison column). `--live-smoke` keeps the consecutive-NG interlock off so a category finishes. `--webcam-smoke` opens the laptop camera for a timed session (default 8 s) then exits 0; no device → exit 2, no crash. If DirectShow `read()` blocks on stop, the process times out instead of hanging.

New category, no code change: drop `<cat>/train/good` and `<cat>/test/...` at the dataset root; for DL also place `models/<cat>/weights/onnx/<cat>.onnx` (calibration needs at least 3 good images). Unknown categories use k=3 / area gate 1000. Layout: [DEVELOPMENT.md](DEVELOPMENT.md) §5.
