# surface_defect_detector

工业产品表面缺陷检测工作站（Qt6 Quick / QML + OpenCV + EfficientAD ONNX）。

仓库：[Gitee](https://gitee.com/xhssdwf/surface_defect_detector)

本工作区已收工。海康相机 / 真实 PLC 不在交付范围。识别数字以 `--batch` / `--live-smoke` 为准；本机摄像头相对 MVTec 是分布外，整班 NG 是预期。

---

## 新机上手（给第一次拉仓库的人）

目标：从 Gitee 拉下来，用 **Qt Creator** 编过，能弹出登录窗。不要用 Visual Studio / MSVC。

### 1. 装 Qt Creator

到 [Qt 在线安装器](https://www.qt.io/download-qt-installer) 安装，登录 Qt 账号后勾选：

- **Qt 6.5 或更新**（本仓库用 6.11.2 验证过）
- 该版本下的 **MinGW 64-bit**（不要只勾 MSVC）
- **Qt Creator**（安装器一般默认带上）
- Developer and Designer Tools 里的 **CMake**、**Ninja**、**MinGW**

装完能打开 Qt Creator 即可，不必把编译器加到系统 PATH。

### 2. 从 Gitee 拉代码

```bash
git clone https://gitee.com/xhssdwf/surface_defect_detector.git
cd surface_defect_detector
```

不会用 Git：打开上面的 Gitee 页，点 **克隆/下载 → 下载 ZIP**，解压后下面步骤一样。

### 3. 补仓库里没有的东西（用我发的压缩包）

`third_party/`、`metal_nut/`、`screw/`、`models/` **不在 Git 里**。只拉代码会编不过，或编过了左侧是空的。

我会另发 3 个 zip（网盘/QQ；微信大约 100MB 发不了数据集包）：

| 压缩包 | 必须？ | 解压后应出现 |
|---|---|---|
| `sdd-third_party.zip` | 要编译就必须 | `third_party/opencv/...`、`third_party/onnxruntime/...` |
| `sdd-datasets.zip` | 要看图就必须 | `metal_nut/`、`screw/` |
| `sdd-models.zip` | 要 EfficientAD 才要 | `models/metal_nut/weights/onnx/metal_nut.onnx` |

**不要自己解压。** 把 zip 放到仓库根，双击 `setup.bat`（远程时让对方截一张脚本窗口）。

1. 打开刚 clone 的 `surface_defect_detector` 文件夹，确认能看见 `CMakeLists.txt`、`setup.bat`
2. 把 3 个 zip **复制进这个文件夹**（和 `CMakeLists.txt` 同一层）
3. 双击 `setup.bat`，等到出现「配好了」
4. 配好后 zip 可以删

脚本会解压并纠正常见套层，最后检查这些路径：

```
third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake
third_party/onnxruntime/lib/onnxruntime.dll
metal_nut/train/good/
screw/train/good/
models/metal_nut/weights/onnx/metal_nut.onnx    ← 只有 models 包才有
```

没有 `sdd-models.zip` 也能跑，登录后引擎选 **传统 CV**。

数据集是 MVTec AD（CC BY-NC-SA），只私下给看这个项目的人，不要公开挂网盘转载。

### 4. Qt Creator 打开、编译、运行

1. 打开 **Qt Creator**
2. **文件 → 打开文件或项目**，选仓库根的 `CMakeLists.txt`
3. 套件选 **Desktop Qt 6.x.x MinGW 64-bit**（不要选 MSVC）
4. 点 **配置项目**。失败先回头核对第 3 步的 OpenCV 路径
5. 左下角锤子 **构建**
6. 绿色三角 **运行**（不要先改命令行参数）

应弹出登录窗。首次账号（密码都是 `123456`）：

| 账号 | 能做什么 |
|---|---|
| `operator` | 只跑检测台 |
| `engineer` | 可进分析台、改参数 / 引擎 |
| `admin` | 另管本机账号 |

改配方请用 `engineer` 或 `admin`，不要用操作员。

---

## 第一次跑通（无工业相机）

登录后：

1. 顶栏或左侧点 **打开数据集**（`Ctrl+O`），选**仓库根目录**（能同时看到 `metal_nut` 文件夹的那一层，不要点进 `metal_nut` 里面）。
2. 顶栏保持 **检测**，左侧点 `metal_nut`。右侧配方上方引擎选 **传统 CV**（没有 ONNX 时不要选 EfficientAD）。
3. 身份条应是 **相机离线 / 模拟取流 · FolderSource / 模拟 PLC · DO0.0**。顶栏图源默认 **文件夹**。
4. 点 **模拟开线**（或空格）→ 自检 → **确认开线**。
5. 右侧看合格/不合格、直通率、节拍。连续不合格默认 8 张后会 **联锁停线**。
6. 停线后打开仓库下 `_sessions/<班次>/`，里面有 `session.csv`、`rejects.csv`。

想跑完整一类 `test/`：连续 NG 调到 **0** 再开线（联锁关掉）。

切到 **本机摄像头** 会立刻出预览（不检测），开线才检测。摄像头画面不是螺丝/螺母，整班 NG 是正常的。

---

## 命令行（可选）

Qt Creator 构建目录一般在 `build/` 或 `build/Desktop_Qt_.../`，exe 叫 `surface_defect_detector.exe`。在 exe 所在目录：

```bash
./surface_defect_detector.exe
./surface_defect_detector.exe --batch metal_nut
./surface_defect_detector.exe --batch metal_nut --engine dl
```

`--batch` / `--live-smoke` / `--webcam-smoke` 不登录。没有 ONNX 时不要加 `--engine dl`。

- `--batch`：图像级默认分数过线，另打面积门对照列。
- `--live-smoke`：关连续 NG 联锁，应跑完一类 test 后退出 0。
- `--webcam-smoke`：限时打开本机摄像头（默认 8 秒）；没设备退出 2，不崩。

更多参数与口径表：[DEVELOPMENT.md](DEVELOPMENT.md)。命令行 CMake 构建也写在那里，日常请用 Qt Creator。

---

## 常见卡住

| 现象 | 怎么办 |
|---|---|
| CMake 报找不到 OpenCV / `videoio` | `third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake` 不存在，或套件选成了 MSVC |
| 链接报 onnxruntime | 重新解压 `sdd-third_party.zip` 到仓库根 |
| 登录后左侧空白 | `Ctrl+O` 选仓库根；检查有没有 `metal_nut/train/good`，不要多套一层 |
| EfficientAD 起不来 | 先改传统 CV；或补上 `models/<类>/weights/onnx/<类>.onnx` |
| 套件只有 MSVC | 回 Qt 安装器补 MinGW 64-bit，重新打开项目 |

---

## 作者打包（你发给朋友之前）

本机已经有 OpenCV / ONNX / 数据集 / 模型时，在仓库根跑：

```powershell
powershell -ExecutionPolicy Bypass -File tools/pack_missing.ps1
```

会在 `_dist/` 生成上面 3 个 zip（已去掉 `.pdb`、训练 `.ckpt`）。把 zip 拷到仓库根（或直接留在 `_dist/`，`setup.bat` 两边都会找）。数据集包走网盘或 QQ。`_dist/` 不入库。

---

## 更多文档

- 阶段、架构、实测指标：[DEVELOPMENT.md](DEVELOPMENT.md)
- 阶段实施日记：[DEVELOPMENT-HISTORY.md](DEVELOPMENT-HISTORY.md)
- 训练（已收住，一般不必跑）：[tools/training/TRAIN.md](tools/training/TRAIN.md)

新类别不改代码：数据集根放入 `<类>/train/good` + `<类>/test/...`，DL 再放 `models/<类>/weights/onnx/<类>.onnx`（标定至少 3 张良品）。
