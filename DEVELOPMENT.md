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
./build/surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu   # 强制 CPU
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5     # 取流冒烟（无窗）
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
```

注意：Ninja 不在 PATH，配置时必须显式传 `CMAKE_MAKE_PROGRAM`。
构建后 OpenCV / ONNX Runtime / DirectML dll 复制到 exe 旁；Qt dll 已用 windeployqt 部署。
缺 DirectML 包时：`powershell -ExecutionPolicy Bypass -File tools/fetch_onnxruntime_dml.ps1`
（C++ 推理用 Microsoft.ML.OnnxRuntime.DirectML **1.24.4**，这是目前最新的 DML 原生包，没有 1.29 的 DML zip；
Python 训练仍锁 onnxruntime==1.29.0。）

## 3. 架构与目录

```
surface_defect_detector/
├── CMakeLists.txt
├── qml/                      # View：检测/分析两态（顶栏 / 左树 / 中画布 / 右结果轨或分析侧栏）
├── src/
│   ├── main.cpp                # 入口；--batch / --live-smoke 走 QCoreApplication，GUI 走 QGuiApplication
│   ├── viewmodels/             # ViewModel：MainViewModel + 树/框/指标/对比/NG 列表
│   ├── items/InspectionCanvas  # QQuickPaintedItem 看图：缩放平移，GT/检测叠加
│   ├── DetectionController.h/.cpp # 应用服务层：数据集 + 引擎缓存 + 批量编排（GUI/CLI 共用）
│   ├── InspectionSession.h/.cpp   # 取流：有界队列 + 取流/检测双线程 + 班次摘要
│   ├── LiveSessionTypes.h         # 班次摘要 / 工件记录（session.csv）
│   ├── sources/                # IFrameSource / FolderSource / CameraSource（空）
│   │                           # IRejectSink / LogRejectSink / FileRejectSink / CompositeRejectSink
│   ├── IDetectionEngine.h      # 检测引擎抽象接口 + DetectionResult 输出契约
│   ├── DetectionEngine.h/.cpp  # 传统 CV 检测引擎（v0.1 基线，IDetectionEngine 实现）
│   ├── DLDetectionEngine.h/.cpp # EfficientAD ONNX（P2；3.6 DirectML + 图像级分数过线）
│   ├── EngineParams.h          # 传统/DL 可调参数；新类走 defaults()，仅 screw 保留 P2 工作点
│   ├── OverlayColors.h         # GT 红 / 检测绿（画布与导出共用）
│   ├── ResultExporter.h/.cpp   # 标注图 + 批量 CSV + 班次 CSV（Phase 3 / 3.7）
│   ├── DatasetManager.h/.cpp   # 数据集加载：类别→缺陷类型→图片，配对 GT 掩码
│   └── ResultEvaluator.h/.cpp  # 像素级 P/R/F1/IoU + 图像级检出评估
├── tools/training/           # Python 训练侧（anomalib / EfficientAD）
├── tools/fetch_onnxruntime_dml.ps1  # 拉 DirectML ORT + DirectML.dll 到 third_party/
├── models/                   # ONNX / ckpt（gitignore，不入库）
├── third_party/opencv/       # OpenCV 预编译包（gitignore）
├── third_party/onnxruntime/  # DirectML ORT 1.24.4（gitignore）
├── metal_nut/  screw/        # MVTec AD 数据集（顶层 train/test/ground_truth）
└── build/                    # 构建产物（gitignore）
```

分层：`QML View → MainViewModel → DetectionController / InspectionSession → IDetectionEngine`。
`DatasetManager` / `ResultEvaluator` 为无 UI 依赖的领域服务，由 Controller 使用。
QML 不接触 `cv::Mat` 与引擎实例。GUI 的引擎加载 / 标定 / 单张推理 / 批量走
DetectionController 工作线程，进度在画布蒙层与底栏；`--batch` 仍同步，口径不变。
取流走 `InspectionSession`（有界队列 + 同步 `detect()`），NG 走 `IRejectSink`
（日志 + `_sessions/` 落盘）。GUI 分检测 / 分析两态：检测态右侧是结果轨（判定 + 班次），
分析态是参数 / 指标 / 对比。

### 关键接口契约（迭代时保持兼容）

`IDetectionEngine` + `DetectionResult`（src/IDetectionEngine.h）是所有检测实现的统一约定：

- `cv::Mat defectMask`：8UC1，0/255 缺陷掩码
- `std::vector<cv::Rect> boxes` / `std::vector<double> areas`：缺陷框与面积
- `double imageScore` / `imageThreshold`：图像级分数与判定阈值
- `bool detected()`：分数过线（`imageScore >= imageThreshold`）。P3.5 及以前是
  `totalArea >= minImageArea`。面积门只影响掩码/框与 `--batch` 对照列。
  改此结构须同步 DetectionController、ResultEvaluator、main.cpp 批处理、MainViewModel 四处。

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

推理在 Phase 3.6 改为默认 DirectML（见第 4 节工作项 1）；P2 收口时仍是 CPU ONNX、整批约十几分钟。
不继续在同一小模型上堆 step。

### Phase 3 ✅ 工程化（已完成）

检测算法与 P2 工作点不变。GUI 改参写入 QSettings；`--batch` 不读设置，口径与上表一致。
回归（收口时实测，与 P2 表相同）：metal_nut CV 图像级 58/115、F1 0.2926；DL 106/115、F1 0.2703；
screw DL 120/160、F1 0.4782。

- **参数**：右侧「检测参数」（CV：聚合阈值/闭运算核/面积门；DL：kσ/闭运算核/叠加面积门），按引擎×类别持久化。恢复默认 = 该类别 P2 工作点。DL 的 k 在 detect 时现算（标定只存 mean/std），改 k 不重跑 train/good。Phase 3.6 起 DL 面积门只影响绿叠加，图像级走分数过线。
- **导出**：当前图 PNG；批量 `images/<defect>/*.png` + `per_image.csv` + `summary.csv`（需先跑过该引擎该类别批量）。
- **对比**：同一类别 CV vs DL 的 P/R/F1/图像级 + ΔF1；缺哪侧批量补跑哪侧。两类引擎分缓存。
- **叠加**：红 = GT 标注（`ground_truth/`），绿 = 当前引擎检出；`good` 无红。
- **GUI 线程**：加载 / 标定 / 单张 / 批量在工作线程，底部状态栏进度条。切 DL 时若模型旁没有有效 `.calib.json`（v3 还键 EP 与每图 max），才对全部 `train/good` 跑 ONNX 标定阈值（metal_nut 220 张；DML 约十几秒，CPU 仍要数分钟，不是训练），结果写到 `models/<类>/weights/onnx/<类>.calib.json`；模型、良品图指纹与 EP 未变则下次启动跳过该循环。同进程再切走内存缓存。`--batch` 仍同步，共用该文件缓存；图像级默认分数过线，另打面积门对照列。

### Phase 3.5 ✅ QML + MVVM 界面重做

Widgets `MainWindow` / `ImageViewWidget` 已删除。GUI 改为 Qt Quick：自定义暗色设计系统、
顶栏引擎分段开关与主操作、画布为视觉中心（判定徽章 / 浮层 GT·检测开关）、右侧 Tab
（当前 / 参数 / 指标 / 对比）。算法、P2 工作点、`--batch` 口径不变。
回归：`--batch metal_nut` 仍为图像级 58/115、F1 0.2926。

左栏数据集用官方 `TreeView` + `TreeViewDelegate`，选中走 `ItemSelectionModel`（不要手搓
点击与缩进）。看图是项目特有叠加，用 `InspectionCanvas`（QQuickPaintedItem）。

### Phase 3.6 部署闭环（无硬件）— ✅ 已完成

P4 相机/PLC 暂不做：没有实机，接 SDK 只能写成空壳。也不再训 EfficientAD-S、不再重做 GUI。
网络排序已到小模型上限（图像 AUROC 0.97+）；界面刚在 3.5 收口。本阶段把现有离线 demo
收成可独立用、以后能直接挂相机的质检工作站。

**不做**（写进规划以免回潮）：

- 再训 EfficientAD-S / 加 step / 换更大模型或更高分辨率（4GB 显存，P2 已判定不改网络）
- 假海康 SDK / 假 PLC 报文（没有设备，协议层测不了）
- 再重做 GUI
- 传统 CV 给 screw 做配准（DL 已覆盖旋转，别回到 P1 短板）
- 为刷表再训 hazelnut / bottle 等类（新类别只用来验证「零改代码接入」，见工作项 3）

实施顺序：1 → 2 → 3 → 4；模拟取流放在 GPU 和图像级分数之后，没有它们它只是幻灯片。

#### 工作项 1：GPU 推理 ✅

C++ 推理默认 DirectML（独显），失败回 CPU。`--provider auto|cpu|dml`（默认 auto；`dml` 失败不静默回退）。
标定缓存 `.calib.json` v2 含 `provider`，cpu/dml 阈值不混用。

包：`Microsoft.ML.OnnxRuntime.DirectML` 1.24.4 + `Microsoft.AI.DirectML` 1.15.4
（NuGet 无 1.29 DML 包；不 include `dml_provider_factory.h`，走 `GetExecutionProviderApi("DML")`，避免 MinGW 缺 DirectML.h）。
笔记本用 DXGI 选最大专用显存适配器（本机 device_id=1，3050 Ti，不是核显）。ORT 会把 shape 类算子留在 CPU，属预期警告。

实测（2026-09-02，metal_nut，含 220 张标定 + 115 张 test，P2 工作点）：

| | 墙钟 | 图像级 | 像素 F1 | good 误报 |
|---|---|---|---|---|
| DML（RTX 3050 Ti Laptop, 3962 MB） | **13.3 s** | 106/115（0.9217） | 0.2703 | 1/22（4.5%） |
| CPU（`--provider cpu`） | 580.4 s | 106/115（0.9217） | 0.2703 | 1/22（4.5%） |

两边指标与 P2 表一致，未因 EP 浮点差跳档。DML 约 44×。

#### 工作项 2：图像级 OK/NG 与面积门解耦 ✅

产线剔除看「这张图有没有问题」，不是掩码贴得多齐。P3.5 及以前 `detected()` =
形态学后 `totalArea >= 面积门`，和绿叠加绑在一起。

契约：`DetectionResult` 增加 `imageScore` / `imageThreshold`；`detected()` 改为分数过线。
像素掩码 / 框仍走热图→kσ→形态学→连通域，只给绿叠加。`--batch` 默认打分数口径，
并保留面积门对照列。P2/P3 表仍是面积门基线，不动。

分数：DL = 256 热图 max（上采样前）；CV = `totalArea`（故 `--batch metal_nut` 传统仍是
58/115、F1 0.2926）。阈值：DL = 良品 max 的 `mean + kσ`（与像素阈同一 k，改 k 不必重标定）；
CV = 面积门。标定缓存升到 **v3**（写入每图 max 便于对照；读到 v2 会重跑一轮 DML 标定）。

试过 train/good 的 95% 分位当图像阈：screw 的良品 max 有饱和到 1.0 的长尾，分位被抬到 0.84，
图像级掉到 0.675；metal_nut 分布极紧（std≈0.004），同一 95% 测试误报 13.6%。改回 mean+kσ。

实测（2026-09-02，DML，P2 像素工作点：metal_nut k=3 / 面积 1000；screw k=1 / 面积 300）：

metal_nut，115 张：

| 类别 | 像素 P | R | F1 | 图像级（分数） | 图像级（面积对照） |
|---|---|---|---|---|---|
| bent | 0.87 | 0.57 | 0.69 | 1.00（25/25） | 1.00（25/25） |
| color | 0.65 | 0.85 | 0.74 | 0.95（21/22） | 0.91（20/22） |
| flip | 0.88 | 0.09 | 0.17 | 1.00（23/23） | 1.00（23/23） |
| scratch | 0.94 | 0.30 | 0.45 | 0.83（19/23） | 0.74（17/23） |
| good（误报率） | — | — | — | 0.091（2/22） | 0.045（1/22） |
| **汇总** | 0.83 | 0.16 | 0.269 | **0.939**（108/115） | 0.922（106/115） |

screw，160 张：

| 类别 | 像素 P | R | F1 | 图像级（分数） | 图像级（面积对照） |
|---|---|---|---|---|---|
| scratch_head | 0.32 | 0.60 | 0.41 | 0.79（19/24） | 0.71（17/24） |
| scratch_neck | 0.48 | 0.84 | 0.61 | 0.96（24/25） | 0.92（23/25） |
| thread_side | 0.09 | 0.05 | 0.06 | 0.43（10/23） | 0.35（8/23） |
| thread_top | 0.49 | 0.78 | 0.60 | 1.00（23/23） | 1.00（23/23） |
| manipulated_front | 0.67 | 0.21 | 0.32 | 0.42（10/24） | 0.38（9/24） |
| good（误报率） | — | — | — | 0.049（2/41） | 0.024（1/41） |
| **汇总** | 0.43 | 0.54 | 0.478 | **0.781**（125/160） | 0.750（120/160） |

验收：metal_nut 图像级 0.939（不掉到 0.90 下）、good 误报 9.1% / 4.9% 均 ≤10%；
screw 从 0.75 提到 0.781。未到 0.90：剩下的漏检是 `thread_side` / `manipulated_front`
的 max 落在良品分数带里（已知模型短板），再降 k 会先打穿误报预算。anomalib 图像 F1 0.95
用的是带标签的 F1 自适应阈，和无监督 mean+kσ 不是同一口径。像素 F1 非本项标准
（metal_nut 0.269，相对 P2 的 0.270 差在 256 max 与上采样 max 的第 4 位）。

#### 工作项 3：新类别零改代码接入 ✅

`DatasetManager` 按 MVTec 布局扫根下直接子目录，没有类别白名单。树、QSettings、
`--batch <名|目录>` 都按名字走。`DLParams::defaultsFor` 只保留 screw 的 P2 工作点
（k=1 / 面积门 300，CLI 不读 QSettings）；**新类禁止再加 `if`**，走 `defaults()`
（k=3 / 面积门 1000）+ 图像级分数标定。

接入（不改 C++、不改 QML）：

```
<root>/<新类>/train/good/*.png          # DL 标定至少 3 张
<root>/<新类>/test/<缺陷类型>/*.png
<root>/<新类>/ground_truth/..._mask.png # 可选
<root>/models/<新类>/weights/onnx/<新类>.onnx
# 回退：<root>/models/<新类>/<新类>.onnx
```

无 ONNX 时传统引擎可跑；切 DL / `--engine dl` 失败并打出约定路径，不崩溃。
不提供 `--model`：标定缓存写在模型旁，指到别类 onnx 会污染那份 `.calib.json`。

探针（临时 `_onboard/`，不入库、不刷 AUROC；验证后已删）：从 metal_nut 拷 3 张
train/good + 各 1 张 test/good、test/scratch 及 mask，onnx 硬链到约定路径。

实测（2026-09-02）：`--batch _onboard` 2 张退出 0；`--batch _onboard --engine dl`
接到 `models/_onboard/weights/onnx/_onboard.onnx`，DML 标定 3 张 + 推理 2 张，0.8 s，
退出 0（指标无意义）。回归未漂：metal_nut CV 图像级 58/115、F1 0.2926；metal_nut DL
分数口径 108/115、F1 0.269；screw DL 125/160、F1 0.478（阈值仍是 k=1 的 0.714，
没有掉到通用 k=3）。

#### 工作项 4：模拟取流 ✅

薄接口，不接假 SDK。真相机来了只换 Source：

```
IFrameSource
  ├── FolderSource   # 按设定 FPS 吐当前类别 test/（含 good），顺序与 runBatch 一致
  └── CameraSource   # P4 填海康；本阶段 start() 失败「未接相机」，不接假 SDK
InspectionSession    # 有界队列 8 + 取流/检测双线程；NG → LogRejectSink（[DO] REJECT）
```

队列满时 Folder **阻塞取帧**（不丢图）；P4 相机应丢最旧帧（写在接口注释，本阶段 stub 不实现）。
Live 走同步 `detect()`，不走 `prepareAndDetectAsync`（忙碌会丢帧）。GUI 开始前 `prepareEngineAsync`，
取流中不盖 BusyOverlay。顶栏「开始/停止取流」+ FPS 1–15（默认 5）；画布 LiveHud 延迟/队列/fps，
徽章改为合格/不合格；底栏 `取流 x fps | 延迟 | 队列 | OK·NG | 路径`。停源后树可再选。

无头冒烟（开发用，不是产品开关）：`--live-smoke <类> [--engine dl] [--fps N]`。

实测（2026-09-02，metal_nut，DL DirectML，引擎已标定）：

| 目标 FPS | 墙钟 | 有效 fps | 末帧实际 fps | 最大队列 | 最大延迟 | 跑完 |
|---|---|---|---|---|---|---|
| 5 | 25.2 s | 4.57 | 4.5 | 0 | 227 ms（首帧） | **115/115** |
| 15 | 9.7 s | 11.86 | 11.1 | 5 | 509 ms（首帧） | **115/115** |

5 fps 时检测跟得上，队列保持 0，延迟稳态 ~90 ms。15 fps 超过 DML 吞吐，队列涨到 5 后仍不丢帧。
NG 90 张 = 分数过线张数（88 TP + 2 good FP，与图像级 108/115 一致）。进程退出码 0，`stop()` 等两线程。

回归未漂：`--batch metal_nut` 图像级 58/115、F1 0.2926；`--batch metal_nut --engine dl` 分数口径 108/115、F1 0.269。

### 生产向抛光（3.6 之后，无新阶段号）

在 3.6 工作站骨架上补夜班能看见的状态，**不改算法、不改 P2 工作点、不重做 GUI、不接相机/PLC、不训练**。本云环境是 Linux，没有 MinGW/Qt/DirectML，也没有 gitignore 的 `models/` 权重，未在此跑 Windows `--batch`，指标表不更新。

做了：

- 缺 ONNX / `train/good` 不足：画布横幅给出约定路径，不进工作线程空转，不静默回退 CV；切 DL 时批量/取流按钮灰掉
- 标定蒙层写明「扫描 train/good，不是训练」；顶栏芯片显示 CV vs EfficientAD、EP、是否已有 `.calib.json`
- 画布徽章统一为合格/不合格 + 分数过线；取流 HUD/底栏打 OK·NG，NG 闪红边（对应已有 `[DO] REJECT` 日志）
- 取流中禁止选图/切引擎/批量，画布有锁条；参数夹紧避免脏 QSettings 崩 OpenCV
- 打开数据集、关于、快捷键、导出成败 toast；空数据集树可恢复

随后又做了一轮产线体验优化（仍不改算法 / 工作点 / `--batch` 口径 / CSV）：

- 判定成为第一视觉：画布大号合格/不合格、外框随判定着色；右侧「当前」顶部放大号判定 + 检出处数，分数收成「判定依据」一行
- GUI 表头与计数改为中文（精确率/召回率/综合分/交并比/图像检出；合格/不合格；真值）。`--batch` 与 CSV 仍用英文列名
- 已知缺陷类型/类别显示中文（`good`→良品 等），未知文件夹名原样；内部路径与 key 不变
- 真值叠加默认关闭，取流期间强制关掉（停流恢复）；顶栏去掉与右侧 Tab 重复的批量/对比，帧率 + 取流让位

明确没做：Phase 4 `CameraSource` 仍是空壳；未重训 EfficientAD、未改分辨率/类别/工作点；未把工程改成 Linux 构建。

### Phase 3.7 班次闭环 + 质检台布局 — ✅ 已完成

P4 仍等实机。本阶段把取流从「日志计数、停了就没了」收成可回看的班次，并把 IDE 三栏改成检测/分析两态工位。不改算法、P2 工作点、`--batch` 口径。

**布局：** 顶栏只留检测|分析、引擎、取流；数据集/导出/关于进「更多」。检测态右侧约 280px 结果轨（合格/不合格只出现这里 + 本班 OK/NG/丢帧/迟剔除 + NG 列表）。分析态右侧是参数 / 指标 / 对比。画布去掉判定徽章和 LiveHud，保留真值/检测开关与 NG 闪边。`TreeView` / `InspectionCanvas` 不变。

**剔除落地：** `InspectionSession` 注入 `IRejectSink`。`CompositeRejectSink` = `LogRejectSink`（`[DO] REJECT`）+ `FileRejectSink`（叠图 + `rejects.csv`）。结束时写 `session.csv` / `summary.csv`。目录：数据集根 `_sessions/<时间>_<类>_<引擎>/`（gitignore）。

**实时策略：** `IFrameSource::overflowPolicy()`。Folder 默认 Block（跑完一类 test）；`--overflow drop` 或未来相机关 `DropOldest`。剔除窗口 = `1000 / targetFps` ms，超时 NG 仍落盘但标迟剔除。

实测（2026-09-03，metal_nut，DL DirectML，引擎已标定）：

| | 墙钟 | 有效 fps | 丢帧 | 迟剔除 | 最大队列 | 最大延迟 | 跑完 |
|---|---|---|---|---|---|---|---|
| 5 fps · block | 24.9 s | 4.61 | 0 | 1（首帧 259 ms > 200 ms 窗） | 0 | 259 ms | **115/115** |
| 15 fps · drop | 9.3 s | 12.32 | 0 | 5（窗 66 ms） | 2 | 250 ms | **115/115** |

5 fps 检测跟得上，队列 0，NG 90 与图像级 108/115 一致（88 TP + 2 good FP）。15 fps 吞吐仍高于节拍，队列最高 2、未打满 8，故 DropOldest 未触发；迟剔除能看见。真相机跟不上时才会丢最旧帧。班次目录含 `rejects.csv`、`session.csv`、不合格叠图。退出码 0。

回归未漂：`--batch metal_nut` 图像级 58/115、F1 0.2926；`--batch metal_nut --engine dl` 分数口径 108/115、F1 0.269。

### Phase 4 产线对接（远期，等实机）

依赖 3.6 的 DirectML、图像级分数、`IFrameSource`，以及 3.7 的 Sink / 丢最旧帧。到货后再填，不在本阶段预写协议：

- `CameraSource`：海康 MVS / MVD 实时取流（本机已装运行时）；`overflowPolicy()` 已返回 `DropOldest`
- 与 PLC / 剔除机构联动（再实现一个 `IRejectSink`，加进 `CompositeRejectSink`）

## 5. 工程规范

- 提交规范：参考 README 参与贡献节（Feat_xxx 分支 + PR）
- 不入库的内容：`third_party/`、`build*/`、`models/`、`_onboard/`、`_sessions/`、数据集目录不动
- 检测引擎接口（IDetectionEngine / DetectionResult 契约）变更需同步改
  DetectionController、ResultEvaluator、main.cpp 批处理、MainViewModel 四处。
  `detected()` 为分数过线；面积门只影响掩码/框与 `--batch` 对照列
- 新类别：按 MVTec 布局丢目录 + `models/<类>/weights/onnx/<类>.onnx`，不改 C++ / QML，
  禁止再加 `if (category == ...)`。无专表工作点走 `DLParams::defaults()`
- 优先用 Qt / OpenCV / 已接入的成熟库，不要手搓官方已有的控件与交互（详见 AGENTS.md）
- 每阶段完成：更新本文档状态表与实测指标；跑通两个类别的 `--batch` 无崩溃
