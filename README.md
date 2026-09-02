# surface_defect_detector

工业产品表面缺陷检测桌面工具（Qt6 Quick / QML + OpenCV + EfficientAD ONNX）。

阶段状态、架构、构建命令和实测指标见 [DEVELOPMENT.md](DEVELOPMENT.md)。
给 AI 代理的约定见 [AGENTS.md](AGENTS.md)。训练见 [tools/training/TRAIN.md](tools/training/TRAIN.md)。

当前进度：Phase 1～3.6 已完成（DirectML / 图像级分数过线 / 新类别零改代码接入 /
模拟取流）。Phase 4 产线对接等实机，只换 `CameraSource`。

```bash
# GUI
./build/surface_defect_detector.exe

# 批处理（与 DEVELOPMENT.md 第 4 节同口径，不读 GUI 设置）
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
```

GUI 为暗色质检台（QML）：红=GT 标注，绿=检测结果。顶栏可按设定 FPS 模拟取流（文件夹源吐 test/），
画布叠延迟/队列/合格·不合格；NG 打 `[DO] REJECT` 日志。DL 默认 DirectML（失败回 CPU）。
切深度学习时若尚无该 EP 的标定缓存，会跑一遍 `train/good`（DML 约十几秒，不是训练）；
之后启动复用模型旁的 `.calib.json`（v3，键含 EP）。`--batch` 图像级默认分数过线，
另打面积门对照列；不读 GUI 设置。`--provider cpu|dml|auto`。

新类别不改代码：在数据集根放入 `<类>/train/good` + `<类>/test/...`，DL 再放
`models/<类>/weights/onnx/<类>.onnx`（标定至少 3 张良品）。无专表工作点时用 k=3 /
面积门 1000。步骤与验收见 [DEVELOPMENT.md](DEVELOPMENT.md) 工作项 3。

#### 参与贡献

1.  Fork 本仓库
2.  新建 Feat_xxx 分支
3.  提交代码
4.  新建 Pull Request


#### 特技

1.  使用 Readme\_XXX.md 来支持不同的语言，例如 Readme\_en.md, Readme\_zh.md
2.  Gitee 官方博客 [blog.gitee.com](https://blog.gitee.com)
3.  你可以 [https://gitee.com/explore](https://gitee.com/explore) 这个地址来了解 Gitee 上的优秀开源项目
4.  [GVP](https://gitee.com/gvp) 全称是 Gitee 最有价值开源项目，是综合评定出的优秀开源项目
5.  Gitee 官方提供的使用手册 [https://gitee.com/help](https://gitee.com/help)
6.  Gitee 封面人物是一档用来展示 Gitee 会员风采的栏目 [https://gitee.com/gitee-stars/](https://gitee.com/gitee-stars/)
