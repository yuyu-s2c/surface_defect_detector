# AGENTS.md

给 AI 代理的项目工作指南。阶段规划与实测指标见 [DEVELOPMENT.md](DEVELOPMENT.md)。

## 项目概况

工业产品表面缺陷检测工具：Qt6 Widgets 桌面应用 + OpenCV 传统检测（v0.1）
+ EfficientAD ONNX（Phase 2，已收住）+ Phase 3 工程化（已完成：参数/导出/双引擎对比/GUI 异步）。
数据集为 MVTec AD（metal_nut、screw，无监督设定：train/ 只有良品）。
训练说明见 tools/training/TRAINING_NOTES.md。下一阶段是 Phase 4（相机/PLC，远期）。

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
./build/surface_defect_detector.exe --batch metal_nut --engine dl  # DL，需 models/.../onnx
```

## 环境事实（已核实，勿再探测）

- 工具链均不在 PATH：Qt `D:/Qt/6.11.2/mingw_64`，GCC 13.1 `D:/Qt/Tools/mingw1310_64`，
  CMake `D:/Qt/Tools/CMake_64/bin`，Ninja `D:/Qt/Tools/Ninja`
- OpenCV 4.5.5 MinGW 预编译包在 `third_party/opencv/`（gitignore，不在仓库里；
  缺失时从 huihut/OpenCV-MinGW-Build 的 tag OpenCV-4.5.5-x64 重新下载）
- 硬件：RTX 3050 Ti 4GB 显存，Python 3.12.10
- Windows + Git Bash 环境，用 Unix 语法和正斜杠路径

## 硬性约定

- **接口契约**：所有检测引擎实现 `IDetectionEngine` 并输出 `DetectionResult`
  （src/IDetectionEngine.h：defectMask / boxes / areas / detected()）。改此结构需同步改
  DetectionController、ResultEvaluator、main.cpp 批处理三处。
- **分层**：MainWindow 纯视图（只做展示与转发）；DetectionController 是应用服务层
  （数据集 + 引擎缓存 + 批量编排，GUI 与 --batch 共用）；新引擎实现 IDetectionEngine 即可接入。
  GUI 耗时路径走 Controller 工作线程 + 底部进度条；`--batch` 仍同步，不读 QSettings。
- **不要动数据集**：metal_nut/、screw/ 只读（顶层 train/、test/、ground_truth/）。
  曾因解压套一层出现 metal_nut/metal_nut、screw/screw，已删除；若再出现则忽略。
- **不入库**：third_party/、build*/、models/（见 .gitignore）。
- **不执行 git 提交/推送等变更操作**，除非用户明确要求。
- 代码注释用中文，风格对齐现有文件（解释"为什么"，关键实测依据写入注释）。

## 验证标准

- 构建零错误；改动的代码无新警告
- 两个类别的 `--batch` 模式跑完不崩溃
- 指标口径固定用 ResultEvaluator（像素级 P/R/F1/IoU + 图像级检出率），
  新引擎与 v0.1 基线（DEVELOPMENT.md 第 4 节表格）同口径对比
