# surface_defect_detector

Desktop tool for industrial surface defect detection (Qt6 Quick / QML + OpenCV + EfficientAD ONNX).

Status, architecture, build commands and measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md).
Agent conventions: [AGENTS.md](AGENTS.md). Training: [tools/training/TRAIN.md](tools/training/TRAIN.md).

Phases 1–3.5 are done (CV baseline / EfficientAD / engineering / QML MVVM).
Phase 3.6 work items 1–3 (DirectML, image-level score, zero-code new-category onboarding)
are done. Next: work item 4 (folder frame source). Phase 4 (camera/PLC) waits on hardware.

```bash
./build/surface_defect_detector.exe
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
```

GUI overlay: red = ground-truth mask, green = detection. DL defaults to DirectML (CPU fallback).
Switching to DL calibrates on `train/good` once per execution provider (DirectML is tens of seconds; that is not training) and writes `<model>.calib.json` (v3, keyed by EP) next to the ONNX file.
`--batch` image-level defaults to score-over-threshold and still prints the area-gate column for comparison; it does not read GUI settings. `--provider cpu|dml|auto`.

New category, no code change: drop `<cat>/train/good` and `<cat>/test/...` at the dataset root;
for DL also place `models/<cat>/weights/onnx/<cat>.onnx` (calibration needs at least 3 good images).
Unknown categories use k=3 / area gate 1000. Details: [DEVELOPMENT.md](DEVELOPMENT.md) work item 3.

1.  xxxx
2.  xxxx
3.  xxxx

#### Contribution

1.  Fork the repository
2.  Create Feat_xxx branch
3.  Commit your code
4.  Create Pull Request


#### Gitee Feature

1.  You can use Readme\_XXX.md to support different languages, such as Readme\_en.md, Readme\_zh.md
2.  Gitee blog [blog.gitee.com](https://blog.gitee.com)
3.  Explore open source project [https://gitee.com/explore](https://gitee.com/explore)
4.  The most valuable open source project [GVP](https://gitee.com/gvp)
5.  The manual of Gitee [https://gitee.com/help](https://gitee.com/help)
6.  The most popular members  [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
