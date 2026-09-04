# EfficientAD 训练与部署笔记（Phase 2）

没事翻这一份就行：模型在干什么、我们实际怎么训的、日志怎么读、C++ 侧为什么和 anomalib 分数差一截。  
命令抄 `TRAIN.md`；阶段结论和对比表在仓库根目录 `DEVELOPMENT.md`（实施日记 `DEVELOPMENT-HISTORY.md`）。

---

## 1. 任务设定

这是 **无监督异常检测**，不是「划痕/螺纹」那种多分类。

- `train/good` 只有良品。模型只学「正常长什么样」。
- `test/` 里既有良品也有缺陷；`ground_truth/` 是像素级掩码，**训练时不用**，只用来打分。
- 每个产品类别 **单独一个模型**：`metal_nut` 和 `screw` 不共享权重，也不能接着对方的 ckpt 再训。

数据集是 MVTec AD，图 700×700 PNG。训练侧只用了两个类（脚本 `choices` 写死这两名，
不为刷表再训 hazelnut / bottle）。C++ 推理不白名单类别：按同样布局丢目录即可接入。

| | train/good | test（含 good） | 缺陷类型 |
|---|---|---|---|
| metal_nut | 220 | 115 | bent / color / flip / scratch |
| screw | 320 | 160 | scratch_head / scratch_neck / thread_side / thread_top / manipulated_front |

解压时曾套出 `metal_nut/metal_nut`、`screw/screw` 一整份副本。anomalib 用 `glob('**/*')` 会把样本数翻倍。那两层已经删了；训练脚本里仍按 `image_path` 去重，防止以后再套一层。

---

## 2. 为什么是 EfficientAD

论文：Batzner et al., *EfficientAd: Accurate Visual Anomaly Detection at Millisecond-Level Latencies*, WACV 2024。

选型理由（相对本仓库）：

- MVTec 上图像 AUROC 约 99%、像素约 97%（论文值），推理毫秒级。
- 小模型，256×256、batch=1，4GB 显存（本机 RTX 3050 Ti）能训。
- anomalib 有实现，能导出 ONNX，C++ 用 ONNX Runtime 加载。

没选 PatchCore：不用训，但 coreset + k-NN 很难 ONNX 化，C++ 部署贵。

实现锁在 `tools/training/requirements.txt`：anomalib 2.6.0，torch 2.13.0+cu126。版本别随便升。

---

## 3. 模型在算什么

三块网络，只更新后两块：

1. **Teacher**：预训练 EfficientNet，权重冻结。把图编成特征。
2. **Student**：小网络，学着复现 teacher 在 **良品** 上的特征。缺陷图上复现不了，特征差就是异常。
3. **Autoencoder（AE）**：再学一版良品重构。用来压「逻辑异常」之类 student 可能跟着学歪的情况。

训练损失（日志里能看到）大致是：

| 日志名 | 含义 |
|---|---|
| `train_st` / `train_st_epoch` | student 对 teacher 的蒸馏损失 |
| `train_ae` | 自编码器重构损失 |
| `train_stae` | student 与 AE 的一致性（论文里的惩罚项之一） |
| `train_loss` | 上面三项之和 |

这些 **不是测试分数**。第一回 metal_nut 日志里只有这些，是因为当时脚本只 `fit`、没 `test`。

另外还有 **imagenette** 惩罚：训练时混入 ImageNet 风格的无关图，防止 student 把「什么都复现」学成恒等映射。缓存目录是 `tools/training/.cache/imagenette`，第一回会下载，之后不用再下。

EfficientAD 内部硬约束：`train_batch_size=1`。输入 256×256。ImageNet 归一化在模型 **forward 里面**做，ONNX 的输入是 0~1 的 RGB 浮点图（C++ 侧 BGR→RGB、`/255`、NCHW）。

分位数标定发生在 `on_validation_start`：用验证集良品热图的 90% / 99.5% 分位去归一化 student/AE 的图。所以完整评估必须先 `validate` 再 `test`，只调 `test` 会缺这一步。

---

## 4. 我们实际怎么训的

脚本：`tools/training/train_efficientad.py`。venv：`tools/training/.venv`。

超参两边一样（论文默认 step 数）：

- `max_steps = 70000`，`accelerator=gpu`，`devices=1`
- batch=1，image size=256，`num_workers=0`（Windows 多进程 dataloader 易卡）
- 每类从随机初始化开训，不加载另一类的权重

metal_nut 约 220 step/epoch → 约 318 epoch；screw 约 320 step/epoch → 约 219 epoch。总 step 相同，墙钟同一量级。

产物（`models/` 已 gitignore，不入库）：

```
models/<类别>/EfficientAd/.../weights/lightning/model.ckpt
models/<类别>/weights/onnx/<类别>.onnx
models/<类别>/test_metrics.txt      # anomalib 测试分
models/<类别>/train.log             # 若用 Tee 重定向
```

screw 的 lightning 目录名是 `MvtecTopLevel` 而不是 `MVTecAD`，因为脚本里为了去重子类化了 datamodule。找 ckpt 不要写死路径，搜 `**/model.ckpt`。

命令见 `TRAIN.md`。PowerShell 不要把 `screw --steps 70000` 拆到下一行。

---

## 5. 两套分数，别混

### anomalib（训练脚本打出来的）

看排序好不好，阈值是它自己自适应的。

| | image_AUROC | image_F1 | pixel_AUROC | pixel_F1 |
|---|---|---|---|---|
| metal_nut | 0.986 | 0.973 | 0.974 | 0.806 |
| screw | 0.975 | 0.948 | 0.985 | 0.511 |

这说明 **网络把正常/异常分开了**。screw 像素 F1 只有 0.51：排序可以（AUROC 0.985），硬切出来的掩码贴不紧，细缺陷（螺纹）天生如此。

### C++ `ResultEvaluator`（`--batch --engine dl`）

像素级仍和 v0.1 同一套评估器：热图 → 阈值 → 形态学 → 连通域 → P/R/F1/IoU。
图像级 Phase 3.6 起是分数过线（`max(热图) >= mean+kσ`），不再用连通域面积。

C++ **不用** anomalib 存在 ckpt 里的阈值。ONNX 里也没有完整后处理。做法是：用全部 `train/good` 跑热图，取每张 256 图最大值的 `mean + kσ` 当像素阈值，再加形态学。

工作点（已收住，写在 `EngineParams.h`）：

| | kσ | 叠加面积门 |
|---|---|---|
| metal_nut | 3.0 | 1000 |
| screw | 1.0 | 300 |

连通域最小面积两边都是 100；闭运算核 21。
Phase 3.6 起图像级判定是分数过线（`max(热图) >= mean+kσ`），面积门只影响绿叠加和 `--batch` 对照列。
试过良品 95% 分位当图像阈：screw train/good 有饱和到 1.0 的长尾，分位 0.84 把召回压到 0.67；
metal_nut 分布极紧，同一 95% 误报 13.6%。所以图像阈与像素阈共用 mean+kσ。

---

## 6. ONNX 导出那个报错

screw 训完、测试分都打出来了，导出时：

```
RuntimeError: Inference tensors do not track version counter.
```

原因：`engine.test()` 之后部分 buffer 变成 inference tensor，`torch.onnx.export` 去重 initializer 会炸。metal_nut 第一回是 `fit` 完立刻 export，没踩到。

处理：从磁盘 ckpt **重新 load** 再导出（`--export-only`）。以后脚本也是 test 完走这条路径。`TracerWarning`、`triton not found`、constant folding 都可以忽略。

C++ 找模型路径：先 `models/<类别>/weights/onnx/<类别>.onnx`，再回退
`models/<类别>/<类别>.onnx`。新类接入只放文件，不改 C++ / QML；标定缓存在模型旁
`.calib.json`，不要把别类的 onnx 指过来。训练脚本仍只认 metal_nut / screw。

---

## 7. screw 后处理是怎么收到现在的

anomalib 图像 F1 0.95，C++ 第一版（k=3、面积 1000）只有图像级 0.54。不是没训好，是硬阈值太严。

k 扫描（面积门 1000）：

| k | 图像级 | 缺陷检出 | good 误报 | 像素 F1 |
|---|---|---|---|---|
| 3.0 | 87/160（0.54） | 46/119 | 0/41 | 0.40 |
| 2.0 | 97/160（0.61） | 56/119 | 0/41 | 0.46 |
| 1.5 | 109/160（0.68） | 68/119 | 0/41 | 0.48 |
| 1.0 | 111/160（0.69） | 71/119 | 1/41（2.4%） | 0.48 |

1.0 之后 F1 走平，`thread_side` 不再跟 k 涨。再降 σ 没意义。

面积门 1000→300（k 维持 1.0）：像素 F1 不变（掩码没变），图像级 **111→120/160（0.75）**，误报仍 1/41。多抓的是「热图过线但总面积不到 1000」的细缺陷。

难例收口后仍主要是：

- `thread_side` 8/23，像素 F1 0.06（能报警，框不准）
- `manipulated_front` 9/24

`thread_top` 23/23、`scratch_neck` 23/25 已经够用。

metal_nut 没做同样扫描：k=3、面积 1000 就到了图像级 0.92、误报 4.5%、scratch F1 0.45（v0.1 是 0.02），达标。汇总像素 F1 0.27 略低于 v0.1 的 0.29，因为 flip 的 GT 是整颗翻转件，热图只打局部，召回 0.09 把像素汇总拉下去；图像级 flip 是 23/23。

---

## 8. 瓶颈怎么理解

两层：

1. **网络（排序）**：AUROC 已经在 0.97+，再训一轮、调 lr，不会质变。256 的 EfficientAD-S 对侧螺纹、正面小改动会糊，这是模型+分辨率上限。
2. **C++ 硬判决**：和 anomalib 的 0.95 图像 F1 还有缝。面积门已经解耦（DEVELOPMENT.md §4.3）：
   图像级走 `max(热图) >= mean+kσ`，screw 从 0.75 提到 0.781，metal_nut 0.939。
   剩下到不了 0.90 的是 `thread_side` / `manipulated_front` 的 max 落在良品分数带里；
   anomalib 那 0.95 用的是带标签的 F1 自适应阈。再拧 k 会先打穿误报预算。

产线若只做「剔除不良」，看图像级 / 缺陷检出；要叠掩码看像素 F1。screw 现在是前者能用、后者在细缺陷上不行。

C++ 推理默认 DirectML（Phase 3.6 工作项 1），metal_nut 整批含标定约 13 s；`--provider cpu` 约 10 分钟。
GUI 把加载/标定/单张/批量放在工作线程；无缓存时才对全部 train/good 做阈值标定（不是训练），
结果写到模型旁 `.calib.json`（v3 含每图 max 与 cpu/dml，两套阈值不混用）。
模型、良品图指纹与 EP 未变则下次启动跳过。同进程内再切走内存缓存。
更大模型 / 更高分辨率受本机 RTX 3050 Ti 4GB 限制，P2 已收住，不要再堆 step。
摄像头 / 现场几张良品标定救不了识别率：现成 ONNX 是 MVTec 域，传统 CV 在 220 张受控良品上已触顶。

---

## 9. 想自己复现时看哪

```text
tools/training/train_efficientad.py   训练 / 测试 / 导出
tools/training/TRAIN.md               复制命令
tools/training/requirements.txt       锁版本
src/DLDetectionEngine.cpp             ONNX 推理 + 热图后处理 + 图像级分数
src/EngineParams.h                    默认 k=3 / 面积 1000；仅 screw 为 P2 的 1 / 300
src/IDetectionEngine.h                DetectionResult：detected() = 分数过线
src/ResultEvaluator.cpp               指标口径
```

批处理：

```powershell
.\build\surface_defect_detector.exe --batch metal_nut --engine dl
.\build\surface_defect_detector.exe --batch metal_nut --engine dl --provider cpu
.\build\surface_defect_detector.exe --batch screw --engine dl
```
