# surface_defect_detector

Desktop tool for industrial surface defect detection (Qt6 Quick / QML + OpenCV + EfficientAD ONNX).

Status, architecture, build commands and measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md).
Agent conventions: [AGENTS.md](AGENTS.md). Training: [tools/training/TRAIN.md](tools/training/TRAIN.md).

Phases 1–3.7 are done (DirectML, image-level score, zero-code new-category onboarding,
simulated folder streaming, shift/reject archive, inspect/analyze layout).
An offline station loop was added on top (pre-run self-check, simulated PLC/DO point table,
takt/yield, consecutive-NG interlock), plus **P4.0 laptop webcam** (`WebcamSource`, header switch).
Full Phase 4 (Hikvision/PLC) still waits on hardware: swap in `CameraSource` and add a real-DO
`IRejectSink`. Do not treat `CameraSource` as a fake live feed.

## 5-minute demo with no camera

On the Windows machine, with the repo root as the dataset root (`metal_nut/` present).
ONNX under `models/metal_nut/...` is optional (traditional CV works without it).

1. Run `./build/surface_defect_detector.exe` and open the dataset root.
2. Stay in **Inspect** mode, select `metal_nut`.
3. The header must show **camera offline / FolderSource simulation / simulated PLC·DO0.0** (folder mode). Switching to **本机摄像头** shows **laptop camera · OpenCV / WebcamSource**, never “camera connected”.
4. Fill a work order (default `WO-YYYYMMDD`). Consecutive-NG limit defaults to 8.
5. Click **模拟开线** (or Space) → self-check → confirm. `CameraSource` is not started. Webcam mode uses **开线** and probes the device in self-check.
6. Watch OK/NG, yield, takt, the consecutive-NG bar, and DO0.0 pulses. After 8 consecutive NG the line **interlocks**. Webcam frames vs metal_nut/screw are out-of-distribution; all-NG is expected.
7. Open `_sessions/<shift>/`: `session.csv`, `rejects.csv`, `do_map.csv`, `do_pulses.csv`.
8. To run a full category: switch back to **文件夹**, set consecutive NG to **0** (same as `--live-smoke`).

Phase 4 is still only: implement `CameraSource` (`overflowPolicy()` already returns drop-oldest) and add a real DO sink to `CompositeRejectSink`. Do not turn `SimulatedDoSink` into a fieldbus.

```bash
./build/surface_defect_detector.exe
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 5
```

GUI overlay: red = ground-truth mask, green = detection. Header switches Inspect / Analyze;
the inspect rail shows OK/NG, yield/takt and the shift reject / simulated DO list.
Streaming plays `test/` at a set FPS (folder source); rejects log `[DO] REJECT` / `[PLC-SIM]`
and write `_sessions/`. `--batch` does not read GUI settings, work orders, or the interlock.
`--live-smoke` keeps consecutive-NG interlock off so a category finishes.

New category, no code change: drop `<cat>/train/good` and `<cat>/test/...` at the dataset root;
for DL also place `models/<cat>/weights/onnx/<cat>.onnx` (calibration needs at least 3 good images).
Unknown categories use k=3 / area gate 1000. Details: [DEVELOPMENT.md](DEVELOPMENT.md) work item 3.
