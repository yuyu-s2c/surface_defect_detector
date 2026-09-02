# AGENTS.md

给 AI 代理的项目工作指南。阶段规划与实测指标见 [DEVELOPMENT.md](DEVELOPMENT.md)。

## 项目概况

工业产品表面缺陷检测工具：Qt6 Quick（QML + MVVM）桌面应用 + OpenCV 传统检测（v0.1）
+ EfficientAD ONNX（Phase 2，已收住）+ Phase 3 工程化（参数/导出/双引擎对比/GUI 异步）
+ Phase 3.5 QML/MVVM 界面重做。
数据集为 MVTec AD（metal_nut、screw，无监督设定：train/ 只有良品）。
训练说明见 tools/training/TRAINING_NOTES.md。Phase 3.6 工作项 1（DirectML）与
工作项 2（图像级分数过线）已完成；接下来是新类别接入。Phase 4（相机/PLC）等实机。

## 构建与运行

```bash
# 配置（Ninja 不在 PATH，必须显式传 CMAKE_MAKE_PROGRAM）
D:/Qt/Tools/CMake_64/bin/cmake.exe -S . -B build -G Ninja \
  -DCMAKE_PREFIX_PATH=D:/Qt/6.11.2/mingw_64 \
  -DCMAKE_MAKE_PROGRAM=D:/Qt/Tools/Ninja/ninja.exe \
  -DCMAKE_CXX_COMPILER=D:/Qt/Tools/mingw1310_64/bin/g++.exe \
  -DCMAKE_C_COMPILER=D:/Qt/Tools/mingw1310_64/bin/gcc.exe
D:/Qt/Tools/CMake_64/bin/cmake.exe --build build

# 验证（改完代码必跑）
./build/surface_defect_detector.exe --batch metal_nut           # 传统，115 张无崩溃
./build/surface_defect_detector.exe --batch metal_nut --engine dl  # DL，默认 DirectML
```

## 环境事实（已核实，勿再探测）

- 工具链均不在 PATH：Qt `D:/Qt/6.11.2/mingw_64`，GCC 13.1 `D:/Qt/Tools/mingw1310_64`，
  CMake `D:/Qt/Tools/CMake_64/bin`，Ninja `D:/Qt/Tools/Ninja`
- OpenCV 4.5.5 MinGW 预编译包在 `third_party/opencv/`（gitignore，不在仓库里；
  缺失时从 huihut/OpenCV-MinGW-Build 的 tag OpenCV-4.5.5-x64 重新下载）
- ONNX Runtime C++：`third_party/onnxruntime/` 为 DirectML 1.24.4（gitignore）。
  缺失时 `powershell -ExecutionPolicy Bypass -File tools/fetch_onnxruntime_dml.ps1`。
  不要下 CPU zip 或 CUDA EP（MinGW）。Python 训练仍是 onnxruntime==1.29.0。
- 硬件：RTX 3050 Ti 4GB 显存，Python 3.12.10
- Windows + Git Bash 环境，用 Unix 语法和正斜杠路径

## 硬性约定

- **接口契约**：所有检测引擎实现 `IDetectionEngine` 并输出 `DetectionResult`
  （src/IDetectionEngine.h：defectMask / boxes / areas / imageScore / imageThreshold /
  detected() 为分数过线）。改此结构需同步改 DetectionController、ResultEvaluator、
  main.cpp 批处理、MainViewModel 四处。
- **优先用现成的，不要手搓**：Qt / OpenCV / 已接入的成熟库能覆盖的，直接用官方 API
  或第三方成熟实现，不要自己重写一套。自定义只做主题、布局和本项目没有现成控件的
  领域能力（例如 InspectionCanvas 的 GT/检测叠加）。不要重做官方控件已经提供的
  交互（选中、展开、缩进、事件路由）。
  反例：数据集树曾手搓 `Item` + `TapHandler`，点击无响应，名称和展开箭头重叠；
  应使用 `TreeView` + `TreeViewDelegate`，选中走 `selectionModel`，不要覆盖
  `leftPadding`（模板按 depth 和指示器宽度给 contentItem 让位）。
- **分层**：QML View（qml/）只绑属性/命令；MainViewModel 是 GUI 状态层（选图、参数、
  QImage 叠加、QSettings）；DetectionController 是应用服务层（数据集 + 引擎缓存 +
  批量编排，GUI 与 --batch 共用）。不要把 Controller / cv::Mat 暴露给 QML。
  GUI 耗时路径走 Controller 工作线程 + 画布蒙层/底栏进度；`--batch` 仍同步，不读 QSettings。
  左栏数据集树用官方 `TreeViewDelegate`，点叶子节点经 `selectFromModelIndex` 加载。
- **不要动数据集**：metal_nut/、screw/ 只读（顶层 train/、test/、ground_truth/）。
  曾因解压套一层出现 metal_nut/metal_nut、screw/screw，已删除；若再出现则忽略。
- **不入库**：third_party/、build*/、models/（见 .gitignore）。
- **不执行 git 提交/推送等变更操作**，除非用户明确要求。
- 代码注释用中文，风格对齐现有文件（解释"为什么"，关键实测依据写入注释）。

## 验证标准

- 构建零错误；改动的代码无新警告
- 两个类别的 `--batch` 模式跑完不崩溃
- 指标口径固定用 ResultEvaluator（像素级 P/R/F1/IoU + 图像级检出率）。
  图像级默认分数过线，`--batch` 另打面积门对照列。像素级与 v0.1 基线
  （DEVELOPMENT.md 第 4 节表格）同口径对比
