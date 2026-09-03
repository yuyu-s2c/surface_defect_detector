# surface_defect_detector

Desktop tool for industrial surface defect detection (Qt6 Quick / QML + OpenCV + EfficientAD ONNX).

Status, architecture, build commands and measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md).
Agent conventions: [AGENTS.md](AGENTS.md). Training: [tools/training/TRAIN.md](tools/training/TRAIN.md).

Phases 1–3.7 are done (DirectML, image-level score, zero-code new-category onboarding,
simulated folder streaming, shift/reject archive, inspect/analyze layout).
Phase 4 (camera/PLC) waits on hardware; swap in `CameraSource` and add an `IRejectSink`.

```bash
./build/surface_defect_detector.exe
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
```

GUI overlay: red = ground-truth mask, green = detection. Header switches Inspect / Analyze;
the inspect rail shows OK/NG and the shift reject list. Streaming plays `test/` at a set FPS
(folder source); rejects log `[DO] REJECT` and write `_sessions/` (annotated PNGs + CSV).
DL defaults to DirectML (CPU fallback).
Switching to DL calibrates on `train/good` once per execution provider (DirectML is tens of seconds; that is not training) and writes `<model>.calib.json` (v3, keyed by EP) next to the ONNX file.
`--batch` image-level defaults to score-over-threshold and still prints the area-gate column for comparison; it does not read GUI settings. `--provider cpu|dml|auto`. `--overflow block|drop` only affects the live queue (Folder defaults to block so a category finishes without dropping images).

New category, no code change: drop `<cat>/train/good` and `<cat>/test/...` at the dataset root;
for DL also place `models/<cat>/weights/onnx/<cat>.onnx` (calibration needs at least 3 good images).
Unknown categories use k=3 / area gate 1000. Details: [DEVELOPMENT.md](DEVELOPMENT.md) work item 3.
