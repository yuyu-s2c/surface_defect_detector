# 开发文档 — 表面缺陷检测工具

> 本文档是项目的阶段迭代依据：记录当前状态、架构约定、构建运行方式和后续阶段规划。
> 每完成一个阶段，更新对应章节的状态与实测数据。

## 1. 项目概述

工业产品表面缺陷检测桌面工具。Qt6 Widgets 桌面应用，离线图片批量检测，
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
```

注意：Ninja 不在 PATH，配置时必须显式传 `CMAKE_MAKE_PROGRAM`。
构建后 OpenCV dll 自动复制到 exe 旁；Qt dll 已用 windeployqt 部署。

## 3. 架构与目录

```
surface_defect_detector/
├── CMakeLists.txt
├── src/
│   ├── main.cpp              # 入口；--batch 批处理模式（自动化验证用）
│   ├── MainWindow.h/.cpp     # 主窗口：左数据集树 / 中查看器 / 右结果面板
│   ├── DatasetManager.h/.cpp # 数据集加载：类别→缺陷类型→图片，配对 GT 掩码
│   ├── ImageViewWidget.h/.cpp# QGraphicsView 看图：缩放平移，GT/检测结果叠加
│   ├── DetectionEngine.h/.cpp# 传统 CV 检测引擎（v0.1 基线）
│   └── ResultEvaluator.h/.cpp# 像素级 P/R/F1/IoU + 图像级检出评估
├── third_party/opencv/       # OpenCV 预编译包（gitignore）
├── metal_nut/  screw/        # MVTec AD 数据集（嵌套重复目录忽略不用）
└── build/                    # 构建产物（gitignore）
```

### 关键接口契约（迭代时保持兼容）

`DetectionResult`（src/DetectionEngine.h:10）是所有检测实现的统一输出：

- `cv::Mat defectMask`：8UC1，0/255 缺陷掩码
- `std::vector<cv::Rect> boxes` / `std::vector<double> areas`：缺陷框与面积
- `bool detected(int minTotalArea)`：图像级检出判定（缺陷总面积 ≥ 阈值）

后续深度学习引擎只要产出同一结构，UI、评估器、批处理模式均无需改动。

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

### Phase 2 🚩 深度学习异常检测（下一阶段）

**选型：EfficientAD**（W. Batzner et al., WACV 2024）。

理由：MVTec AD 上图像级 AUROC ≈99%、像素级 ≈97%（论文值），毫秒级推理；
小模型，4GB 显存可训练（输入 256×256、小 batch）；anomalib 官方实现并支持 ONNX 导出。
备选：PatchCore（无需训练，但 k-NN coreset 难以 ONNX 化，C++ 部署成本高）。

**技术流程**：

1. **训练侧（Python）**：项目内建 `tools/training/`，创建 venv（Python 3.12），
   安装 anomalib；数据集按 anomalib Folder 格式组织（现有目录结构直接兼容，
   写配置指向 `metal_nut/`、`screw/` 即可）；每类产品训一个模型，导出 ONNX。
   - 模型文件放 `models/<类别>.onnx`，**不入库**（gitignore），文档记录产出方式。
   - 训练显存 4GB 偏紧：batch size 1、输入 256×256，必要时梯度累积。
2. **推理侧（C++）**：新增 `DLDetectionEngine`，加载 ONNX 模型推理出异常热图，
   阈值化 + 连通域得到 `DetectionResult`（复用现有契约与阈值后处理）。
   - 推理后端首选 **ONNX Runtime C API**（官方 Windows 包为 MSVC 构建，但 C ABI 与
     MinGW 兼容，ld 可直接链接 onnxruntime.dll；验证不行再改用运行时动态加载）。
   - 备选后端：OpenCV DNN（third_party 包带 dnn 模块，但对 EfficientAD 算子兼容性待验证）。
   - UI 增加引擎选择（传统 / 深度学习），批处理模式支持 `--engine dl`。
3. **验收口径**：与 v0.1 完全相同的评估器与命令（`--batch metal_nut`），对比指标。
   - 目标：metal_nut 图像级准确率 ≥ 90%，good 误报率 ≤ 10%，scratch F1 显著提升。

**风险与备注**：anomalib 版本迭代快，锁定安装版本并写入 `tools/training/requirements.txt`；
ONNX Runtime 的 MinGW 链接是主要不确定点，先用最小 demo（加载模型跑通一张图）验证再集成。

### Phase 3 工程化（之后）

- 检测参数/阈值在 GUI 可调并持久化（QSettings）
- 检测结果导出（标注图 + CSV 报告）
- 双引擎指标对比视图

### Phase 4 产线对接（远期）

- 接海康 MVS / MVD 相机 SDK 实时取流检测（本机已装运行时）
- 与 PLC/剔除机构联动的接口预留

## 5. 工程规范

- 提交规范：参考 README 参与贡献节（Feat_xxx 分支 + PR）
- 不入库的内容：`third_party/`、`build*/`、`models/`、数据集目录不动
- 检测引擎接口（DetectionResult 契约）变更需同步改 MainWindow、ResultEvaluator、batch 模式三处
- 每阶段完成：更新本文档状态表与实测指标；跑通两个类别的 `--batch` 无崩溃
