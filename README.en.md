# surface_defect_detector

Desktop tool for industrial surface defect detection (Qt6 + OpenCV + EfficientAD ONNX).

Status, architecture, build commands and measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md).
Agent conventions: [AGENTS.md](AGENTS.md). Training: [tools/training/TRAIN.md](tools/training/TRAIN.md).

Phases 1–3 are done (CV baseline / EfficientAD / engineering). Phase 4 (camera/PLC) is later.

```bash
./build/surface_defect_detector.exe
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
```

GUI overlay: red = ground-truth mask, green = detection. The first switch to DL calibrates on `train/good` (about 1–2 minutes); that is not training.

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
