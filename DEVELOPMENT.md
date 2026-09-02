# 开发文档 — 表面缺陷检测工具

> 本文档是项目的阶段迭代依据：记录当前状态、架构约定、构建运行方式和后续阶段规划。
> 每完成一个阶段，更新对应章节的状态与实测数据。

## 1. 项目概述

工业产品表面缺陷检测桌面工具。Qt6 Quick（QML + MVVM）桌面应用，离线图片批量检测，
缺陷区域可视化叠加，并与 ground truth 对比输出评估指标。

- 数据集：MVTec AD（metal_nut、screw，700×700 PNG）。无监督设定——`train/` 只有良品图，
  `test/` 含各类缺陷图，`ground_truth/` 为像素级标注掩码。
- 工具链（均不在 PATH，用绝对路径）：
  - Qt 6.11.2 MinGW：`D:/Qt/6.11.2/mingw_64`
  - 编译器 GCC 13.1：`D:/Qt/Tools/mingw1310_64/bin/g++.exe`
  - CMake：`D:/Qt/Tools/CMake_64/bin/cmake.exe`，Ninja：`D:/Qt/Tools/Ninja/ninja.exe`
- OpenCV 4.5.5 MinGW 预编译包：`third_party/opencv/`（已 gitignore，不入库）
- 硬件：RTX 3050 Ti Laptop（4GB 显存），Python 3.12.10

## 2. 构建与运行

```bash
# 配置 + 构建
D:/Qt/Tools/CMake_64/bin/cmake.exe -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64 \
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe \
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe
D:/Qt/Tools/CMake_64/bin/cmake.exe --build build

# GUI
./build/surface_defect_detector.exe

# 命令行批处理（不弹窗，跑完输出指标后退出）
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch screw
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --batch screw --engine dl
```

注意：Ninja 不在 PATH，配置时必须显式传 `CMAKE_MAKE_PROGRAM`。
构建后 OpenCV dll 自动复制到 exe 旁；Qt dll 已用 windeployqt 部署。

## 3. 架构与目录

```
surface_defect_detector/
├── CMakeLists.txt
├── qml/                      # View：暗色质检台（顶栏 / 左树 / 中画布 / 右 Tab）
├── src/
│   ├── main.cpp                # 入口；--batch 走 QCoreApplication，GUI 走 QGuiApplication
│   ├── viewmodels/             # ViewModel：MainViewModel + 树/框/指标/对比模型
│   ├── items/InspectionCanvas  # QQuickPaintedItem 看图：缩放平移，GT/检测叠加
│   ├── DetectionController.h/.cpp # 应用服务层：数据集 + 引擎缓存 + 批量编排（GUI/CLI 共用）
│   ├── IDetectionEngine.h      # 检测引擎抽象接口 + DetectionResult 输出契约
│   ├── DetectionEngine.h/.cpp  # 传统 CV 检测引擎（v0.1 基线，IDetectionEngine 实现）
│   ├── DLDetectionEngine.h/.cpp # EfficientAD ONNX 推理（Phase 2）
│   ├── EngineParams.h          # 传统/DL 可调参数默认值（P2 工作点）
│   ├── OverlayColors.h         # GT 红 / 检测绿（画布与导出共用）
│   ├── ResultExporter.h/.cpp   # 标注图 + CSV 导出（Phase 3）
│   ├── DatasetManager.h/.cpp   # 数据集加载：类别→缺陷类型→图片，配对 GT 掩码
│   └── ResultEvaluator.h/.cpp  # 像素级 P/R/F1/IoU + 图像级检出评估
├── tools/training/           # Python 训练侧（anomalib / EfficientAD）
├── models/                   # ONNX / ckpt（gitignore，不入库）
├── third_party/opencv/       # OpenCV 预编译包（gitignore）
├── metal_nut/  screw/        # MVTec AD 数据集（顶层 train/test/ground_truth）
└── build/                    # 构建产物（gitignore）
```

分层：`QML View → MainViewModel → DetectionController → IDetectionEngine`。
`DatasetManager` / `ResultEvaluator` 为无 UI 依赖的领域服务，由 Controller 使用。
QML 不接触 `cv::Mat` 与引擎实例。GUI 的引擎加载 / 标定 / 单张推理 / 批量走
DetectionController 工作线程，进度在画布蒙层与底栏；`--batch` 仍同步，口径不变。

### 关键接口契约（迭代时保持兼容）

`IDetectionEngine` + `DetectionResult`（src/IDetectionEngine.h）是所有检测实现的统一约定：

- `cv::Mat defectMask`：8UC1，0/255 缺陷掩码
- `std::vector<cv::Rect> boxes` / `std::vector<double> areas`：缺陷框与面积
- `bool detected()`：图像级检出判定（`totalArea >= minImageArea`；DL/传统由引擎写入面积门）

后续深度学习引擎只要实现该接口产出同一结构，ViewModel、编排、评估器、批处理模式均无需改动。

## 4. 阶段规划与状态

### Phase 1 ✅ 骨架 + 传统 CV 基线（v0.1，已完成）

传统管线：灰度 → 高斯 → 对良品均值图+逐像素标准差图算 z-score → 51×51 空间聚合
→ 阈值 1.4 → 闭运算 → 连通域面积过滤。
（为什么不是 absdiff+Otsu：良品纹理随机，实测约 20% 像素 |diff|>30，Otsu 必然全图误报，
详见 src/DetectionEngine.h:25 注释。）

**v0.1 实测基线**（`--batch metal_nut`，115 张）：

| 类别 | 像素级 P | R | F1 | 图像级准确率 |
|---|---|---|---|---|
| bent | 0.55 | 0.14 | 0.22 | 0.28 |
| color | 0.31 | 0.12 | 0.17 | 0.64 |
| flip | 0.90 | 0.21 | 0.33 | 1.00 |
| scratch | 0.22 | 0.01 | 0.02 | 0.17 |
| good（误报率） | — | — | — | 0.55 |
| **汇总** | 0.79 | 0.18 | **0.29** | **0.50** |

screw 类因旋转差异无配准，基本无效（good 误报率 100%），为已知短板。
结论：朴素像素差分在此数据集已到天花板（scratch 像素值与良品波动重叠），
不继续在传统路线上调参，直接进入深度学习阶段。

### Phase 2 ✅ 深度学习异常检测（已收住）

**选型：EfficientAD**（W. Batzner et al., WACV 2024）+ anomalib 2.6.0 训练，
ONNX Runtime C++ 推理。训练过程说明见 [tools/training/TRAINING_NOTES.md](tools/training/TRAINING_NOTES.md)，
命令见 [tools/training/TRAIN.md](tools/training/TRAIN.md)。

每类一个模型，导出 `models/<类别>/weights/onnx/<类别>.onnx`（不入库）。
C++ `DLDetectionEngine` 用 `train/good` 标定像素阈值（`mean + kσ`），再形态学 + 连通域，
输出与 v0.1 相同的 `DetectionResult`。GUI 可选引擎，CLI：`--engine dl`。

**工作点**（`DetectionController`）：metal_nut `k=3`、面积门 1000；screw `k=1`、面积门 300。

**anomalib 测试**（自适应阈，和 C++ 不是同一口径）：

| | image_AUROC | image_F1 | pixel_AUROC | pixel_F1 |
|---|---|---|---|---|
| metal_nut | 0.986 | 0.973 | 0.974 | 0.806 |
| screw | 0.975 | 0.948 | 0.985 | 0.511 |

**C++ `--batch --engine dl`（与 v0.1 同口径）**

metal_nut，115 张：

| 类别 | 像素级 P | R | F1 | 图像级准确率 |
|---|---|---|---|---|
| bent | 0.87 | 0.57 | 0.69 | 1.00（25/25） |
| color | 0.65 | 0.85 | 0.74 | 0.91（20/22） |
| flip | 0.88 | 0.09 | 0.17 | 1.00（23/23） |
| scratch | 0.94 | 0.30 | 0.45 | 0.74（17/23） |
| good（误报率） | — | — | — | 0.045（1/22） |
| **汇总** | 0.83 | 0.16 | 0.27 | **0.92**（106/115） |

对照目标：图像级 ≥90%、good 误报 ≤10%、scratch F1 相对 v0.1 的 0.02 显著提升，均达到。
汇总像素 F1 0.27 略低于 v0.1 的 0.29，因为 flip 热图只打局部、GT 是整颗，图像级已全中。

screw，160 张（v0.1 因无配准基本无效）：

| 类别 | 像素级 P | R | F1 | 图像级准确率 |
|---|---|---|---|---|
| scratch_head | 0.31 | 0.60 | 0.41 | 0.71（17/24） |
| scratch_neck | 0.48 | 0.84 | 0.61 | 0.92（23/25） |
| thread_side | 0.09 | 0.05 | 0.06 | 0.35（8/23） |
| thread_top | 0.49 | 0.78 | 0.60 | 1.00（23/23） |
| manipulated_front | 0.67 | 0.21 | 0.32 | 0.38（9/24） |
| good（误报率） | — | — | — | 0.024（1/41） |
| **汇总** | 0.43 | 0.54 | 0.48 | **0.75**（120/160） |

缺陷检出 80/119（67%）。`thread_side` / `manipulated_front` 为已知短板；k 与面积门已扫过，再拧收益有限。
网络排序（AUROC）已接近 EfficientAD 小模型上限，C++ 硬判决与 anomalib 的 0.95 图像 F1 仍有缝。

推理目前 CPU ONNX，整批约十几分钟。不继续在同一小模型上堆 step。

### Phase 3 ✅ 工程化（已完成）

检测算法与 P2 工作点不变。GUI 改参写入 QSettings；`--batch` 不读设置，口径与上表一致。
回归（收口时实测，与 P2 表相同）：metal_nut CV 图像级 58/115、F1 0.2926；DL 106/115、F1 0.2703；
screw DL 120/160、F1 0.4782。

- **参数**：右侧「检测参数」（CV：聚合阈值/闭运算核/面积门；DL：kσ/闭运算核/面积门），按引擎×类别持久化。恢复默认 = 该类别 P2 工作点。DL 的 k 在 detect 时现算（标定只存 mean/std），改 k 不重跑 train/good。
- **导出**：当前图 PNG；批量 `images/<defect>/*.png` + `per_image.csv` + `summary.csv`（需先跑过该引擎该类别批量）。
- **对比**：同一类别 CV vs DL 的 P/R/F1/图像级 + ΔF1；缺哪侧批量补跑哪侧。两类引擎分缓存。
- **叠加**：红 = GT 标注（`ground_truth/`），绿 = 当前引擎检出；`good` 无红。
- **GUI 线程**：加载 / 标定 / 单张 / 批量在工作线程，底部状态栏进度条。切 DL 时若模型旁没有有效 `.calib.json`，才对全部 `train/good` 跑 ONNX 标定阈值（metal_nut 220 张、screw 320 张，约 1～2 分钟，不是训练），结果写到 `models/<类>/weights/onnx/<类>.calib.json`；模型或良品图指纹未变则下次启动跳过该循环。同进程再切走内存缓存。`--batch` 仍同步，共用该文件缓存。

### Phase 3.5 ✅ QML + MVVM 界面重做

Widgets `MainWindow` / `ImageViewWidget` 已删除。GUI 改为 Qt Quick：自定义暗色设计系统、
顶栏引擎分段开关与主操作、画布为视觉中心（判定徽章 / 浮层 GT·检测开关）、右侧 Tab
（当前 / 参数 / 指标 / 对比）。算法、P2 工作点、`--batch` 口径不变。
回归：`--batch metal_nut` 仍为图像级 58/115、F1 0.2926。

左栏数据集用官方 `TreeView` + `TreeViewDelegate`，选中走 `ItemSelectionModel`（不要手搓
点击与缩进）。看图是项目特有叠加，用 `InspectionCanvas`（QQuickPaintedItem）。

### Phase 4 产线对接（远期）

- 接海康 MVS / MVD 相机 SDK 实时取流检测（本机已装运行时）
- 与 PLC/剔除机构联动的接口预留

## 5. 工程规范

- 提交规范：参考 README 参与贡献节（Feat_xxx 分支 + PR）
- 不入库的内容：`third_party/`、`build*/`、`models/`、数据集目录不动
- 检测引擎接口（IDetectionEngine / DetectionResult 契约）变更需同步改
  DetectionController、ResultEvaluator、main.cpp 批处理、MainViewModel 四处
- 优先用 Qt / OpenCV / 已接入的成熟库，不要手搓官方已有的控件与交互（详见 AGENTS.md）
- 每阶段完成：更新本文档状态表与实测指标；跑通两个类别的 `--batch` 无崩溃
