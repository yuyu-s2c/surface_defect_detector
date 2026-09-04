# 阶段实施日记

现行规格、口径表和下一步见 [DEVELOPMENT.md](DEVELOPMENT.md)。这里只留已完成阶段怎么做的、当时界面，以及各节重复的回归记录。数字若与 DEVELOPMENT.md 冲突，以 DEVELOPMENT.md 为准。

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

推理在 Phase 3.6 改为默认 DirectML（见下文工作项 1）；P2 收口时仍是 CPU ONNX、整批约十几分钟。
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

Widgets `MainWindow` / `ImageViewWidget` 已删除。GUI 改为 Qt Quick：自定义暗色设计系统。
当时顶栏是引擎分段开关，右侧是 Tab（当前 / 参数 / 指标 / 对比），画布上有判定徽章。
其后 3.7 改成检测/分析两态，工位可读性再收顶栏（见本节末）。算法、P2 工作点、`--batch` 口径不变。
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
  ├── WebcamSource   # P4.0 本机 USB；DropOldest；无限流
  └── CameraSource   # 完整 P4 填海康；本阶段 start() 失败「海康离线」
InspectionSession    # 有界队列 8 + 取流/检测双线程；NG → LogRejectSink（[DO] REJECT）
```

队列满时 Folder **阻塞取帧**（不丢图）；P4 相机应丢最旧帧（写在接口注释，本阶段 stub 不实现）。
Live 走同步 `detect()`，不走 `prepareAndDetectAsync`（忙碌会丢帧）。GUI 开始前 `prepareEngineAsync`，
取流中不盖 BusyOverlay。当时顶栏有「开始/停止取流」+ FPS，画布有 LiveHud；3.7 去掉画布 HUD，
工位可读性把 FPS 收到结果轨配方。停源后树可再选。

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

在 3.6 工作站骨架上补夜班能看见的状态，**不改算法、不改 P2 工作点、不重做 GUI、不接相机/PLC、不训练**。指标表以 Windows `--batch` 为准，本项不改口径。

做了：

- 缺 ONNX / `train/good` 不足：画布横幅给出约定路径，不进工作线程空转，不静默回退 CV；切 DL 时批量/取流按钮灰掉
- 标定蒙层写明「扫描 train/good，不是训练」；引擎状态芯片显示 CV vs EfficientAD、EP、是否已有 `.calib.json`（工位可读性之后在结果轨，不在检测态顶栏）
- 画布徽章统一为合格/不合格 + 分数过线；取流 HUD/底栏打 OK·NG，NG 闪红边（对应已有 `[DO] REJECT` 日志）
- 取流中禁止选图/切引擎/批量，画布有锁条；参数夹紧避免脏 QSettings 崩 OpenCV
- 打开数据集、关于、快捷键、导出成败 toast；空数据集树可恢复

随后又做了一轮产线体验优化（仍不改算法 / 工作点 / `--batch` 口径 / CSV）：

- 判定成为第一视觉：画布外框随判定着色；判定大号字在右侧（当时还在「当前」Tab；3.7 收到结果轨）
- GUI 表头与计数改为中文（精确率/召回率/综合分/交并比/图像检出；合格/不合格；真值）。`--batch` 与 CSV 仍用英文列名
- 已知缺陷类型/类别显示中文（`good`→良品 等），未知文件夹名原样；内部路径与 key 不变
- 真值叠加默认关闭，取流期间强制关掉（停流恢复）；当时顶栏去掉与右侧 Tab 重复的批量/对比，帧率 + 取流让位（帧率其后进结果轨配方）

明确没做：Phase 4 `CameraSource` 仍是空壳；未重训 EfficientAD、未改分辨率/类别/工作点；未把工程改成 Linux 构建。

### Phase 3.7 班次闭环 + 质检台布局 — ✅ 已完成

P4 仍等实机。本阶段把取流从「日志计数、停了就没了」收成可回看的班次，并把 IDE 三栏改成检测/分析两态工位。不改算法、P2 工作点、`--batch` 口径。

**布局（3.7 当时）：** 顶栏检测|分析、引擎、取流；数据集/导出/关于进「更多」。检测态右侧约 280px 结果轨。分析态右侧是参数 / 指标 / 对比。画布去掉判定徽章和 LiveHud，保留真值/检测开关与 NG 闪边。`TreeView` / `InspectionCanvas` 不变。其后工位可读性：检测态顶栏改为模式/图源/开线，帧率与引擎离开检测态顶栏。

**剔除落地：** `InspectionSession` 注入 `IRejectSink`。`CompositeRejectSink` = `LogRejectSink`（`[DO] REJECT`）+ `FileRejectSink`（叠图 + `rejects.csv`）。结束时写 `session.csv` / `summary.csv`。目录：数据集根 `_sessions/<时间>_<类>_<引擎>/`（gitignore）。

**实时策略：** `IFrameSource::overflowPolicy()`。Folder 默认 Block（跑完一类 test）；`--overflow drop` 或未来相机关 `DropOldest`。剔除窗口 = `1000 / targetFps` ms，超时 NG 仍落盘但标迟剔除。

实测（2026-09-03，metal_nut，DL DirectML，引擎已标定）：

| | 墙钟 | 有效 fps | 丢帧 | 迟剔除 | 最大队列 | 最大延迟 | 跑完 |
|---|---|---|---|---|---|---|---|
| 5 fps · block | 24.9 s | 4.61 | 0 | 1（首帧 259 ms > 200 ms 窗） | 0 | 259 ms | **115/115** |
| 15 fps · drop | 9.3 s | 12.32 | 0 | 5（窗 66 ms） | 2 | 250 ms | **115/115** |

5 fps 检测跟得上，队列 0，NG 90 与图像级 108/115 一致（88 TP + 2 good FP）。15 fps 吞吐仍高于节拍，队列最高 2、未打满 8，故 DropOldest 未触发；迟剔除能看见。真相机跟不上时才会丢最旧帧。班次目录含 `rejects.csv`、`session.csv`、不合格叠图。退出码 0。

回归未漂：`--batch metal_nut` 图像级 58/115、F1 0.2926；`--batch metal_nut --engine dl` 分数口径 108/115、F1 0.269。

### 离线工位闭环（面试演示，无新阶段号）

P4 仍等实机。在 3.7 班次骨架上补一条可演示的产线回路，**不改算法、P2 工作点、`--batch` 口径，不接相机/PLC，不训练**。请在 Windows 本机按 README「无相机 5 分钟演示」与下方命令验证。指标表不因本项更新。

做了：

- HMI 顶栏固定显示 **相机离线 / 模拟取流 FolderSource / 模拟 PLC·DO0.0**，不把 `CameraSource` 当假直播
- 开线前自检（相机、图源、模型/标定、输出目录、工单、连续 NG 联锁）；确认后才 `prepareEngine` + FolderSource
- 工单 / 操作员 / 连续 NG 阈值走 QSettings `station/`，不进 `--batch`
- 右侧结果轨：直通率、节拍、连续 NG 条、模拟 DO 脉冲列表
- `SimulatedDoSink`：班次目录写 `do_map.csv`（点表）+ `do_pulses.csv`（脉冲），日志 `[PLC-SIM]`；明确不是实 PLC
- `InspectionSession` 连续不合格联锁（默认 GUI=8；`--live-smoke` 保持 0 才能跑完一类 test）

明确没做：Phase 4 仍只是换 `CameraSource` + 再加一个真实 DO 的 `IRejectSink`。`CameraSource::start()` 继续失败并写明相机离线。

验证（Windows 本机，本变更后必跑）：

```bash
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
# 改了队列策略时另跑：
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 15 --overflow drop
```

`--live-smoke` 联锁关闭，应仍跑完一类 test 后退出 0；班次目录多 `do_map.csv`，有 NG 时有 `do_pulses.csv`。

### Phase 4.0 本机摄像头取流（预演，非完整 P4）

没有海康时用笔记本 USB 摄像头把「真连续取流」跑通。不改引擎、P2 工作点、`--batch` 口径。
`CameraSource` 仍空壳（海康离线）。FolderSource / `--live-smoke` 仍是默认可复现路径。

做了：

- `WebcamSource`：OpenCV `VideoCapture` + DirectShow，`DropOldest`，`plannedCount=0`，按目标 FPS 节拍；长边 >700 按比例缩小；`path` 合成 `cam_000123` 给叠图用
- 顶栏 `SourceSwitch`（文件夹 / 本机摄像头），写入 QSettings `station/liveSourceKind`
- 切到本机摄像头即 `WebcamPreview` 无检测预览（15 fps）；开线前释放设备给 `InspectionSession`，停线后回到预览
- 开线自检：预览已出帧则不再 `probe()`（设备已被预览占用）；否则短开短关，打不开不能确认
- `--webcam-smoke <类>` 默认 8 秒后 `stop()`；无设备退出 2
- HMI：Webcam 芯片写 **本机摄像头 · OpenCV / WebcamSource**，禁止「相机已连接」

摄像头帧相对 metal_nut/screw 是分布外，整班 NG 是预期。指标仍以 `--batch` / `--live-smoke` 为准。

验证：

```bash
./build/surface_defect_detector.exe --batch metal_nut
./build/surface_defect_detector.exe --batch metal_nut --engine dl
./build/surface_defect_detector.exe --live-smoke metal_nut --engine dl --fps 5
./build/surface_defect_detector.exe --webcam-smoke metal_nut --engine dl --fps 5
```

### 工位可读性（P4.0 之后，无新阶段号）

不改算法、P2 工作点、`--batch` 口径，不重做 GUI。只减顶栏噪声、按班次状态切结果轨、把摄像头分布外说成人话。

做了：

- 检测态顶栏只留模式 / 图源 / 开线；标题在窄窗隐藏。帧率进结果轨配方；引擎在检测态结果轨、分析态顶栏切换
- 结果轨：开线前配方（工单/操作员/帧率/连续 NG），开线中本班数字（合格/不合格/直通率/节拍须在首屏），停线后摘要 + 下一班配方在列表下
- 切到本机摄像头（预览或开线）：画布横幅「分布外，整班不合格是预期」
- 关于 / 报错 / 开线自检走 `AppDialog` 工位主题，不再用系统灰窗

