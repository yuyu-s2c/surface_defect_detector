# surface_defect_detector

Industrial surface-defect inspection workstation (Qt6 Quick / QML + OpenCV + EfficientAD ONNX).

Repo: [Gitee](https://gitee.com/xhssdwf/surface_defect_detector)

This workspace is closed. Hikvision and a real PLC are out of scope. Metrics come from `--batch` / `--live-smoke`. Laptop-webcam frames are out-of-distribution vs MVTec; an all-NG shift is expected.

The GUI is Chinese. Quoted labels below match the running app.

---

## New machine (clone → Qt Creator → run)

Goal: clone from Gitee, build with **Qt Creator**, see the login window. Do not use Visual Studio / MSVC.

### 1. Install Qt Creator

Use the [Qt online installer](https://www.qt.io/download-qt-installer). After signing in, select:

- **Qt 6.5 or newer** (verified on 6.11.2)
- **MinGW 64-bit** for that Qt version (not MSVC-only)
- **Qt Creator** (usually default)
- **CMake**, **Ninja**, and **MinGW** under Developer and Designer Tools

You do not need those tools on PATH if you only build inside Qt Creator.

### 2. Clone

```bash
git clone https://gitee.com/xhssdwf/surface_defect_detector.git
cd surface_defect_detector
```

Or on the Gitee page: **Clone / Download → Download ZIP**.

### 3. Missing pieces (zips from the author)

`third_party/`, `metal_nut/`, `screw/`, and `models/` are not in Git.

The author sends three zips (cloud drive / QQ; WeChat’s ~100 MB cap usually cannot take the dataset pack):

| Zip | Required? | After extract |
|---|---|---|
| `sdd-third_party.zip` | to compile | `third_party/opencv/`, `third_party/onnxruntime/` |
| `sdd-datasets.zip` | to see images | `metal_nut/`, `screw/` |
| `sdd-models.zip` | for EfficientAD | `models/<cat>/weights/onnx/<cat>.onnx` |

**Do not extract by hand.** Copy the zips next to `CMakeLists.txt` and double-click `setup.bat` (on remote help, ask for a screenshot of the script window).

```
third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake
third_party/onnxruntime/lib/onnxruntime.dll
metal_nut/train/good/
screw/train/good/
models/metal_nut/weights/onnx/metal_nut.onnx    ← models pack only
```

Without `sdd-models.zip`, use **传统 CV**. MVTec AD is CC BY-NC-SA — private share only, do not republish.

### 4. Build and run in Qt Creator

1. Open **Qt Creator**
2. **File → Open File or Project** → `CMakeLists.txt` at the repo root
3. Kit: **Desktop Qt 6.x.x MinGW 64-bit** (not MSVC)
4. **Configure Project**. If this fails, fix the OpenCV path in step 3
5. Hammer **Build**
6. Green **Run** (no extra arguments)

Login window. First-run accounts (password `123456` for all):

| User | Role |
|---|---|
| `operator` | Inspect only |
| `engineer` | Analyze + change params / engine |
| `admin` | Also manages local accounts |

Use `engineer` or `admin` to change the recipe.

---

## First run (no industrial camera)

1. **打开数据集** (`Ctrl+O`) and pick the **repo root** (the folder that contains `metal_nut`, not `metal_nut` itself).
2. Stay in **检测**. Select `metal_nut`. Engine: **传统 CV** if you have no ONNX.
3. Identity bar should read **相机离线 / 模拟取流 · FolderSource / 模拟 PLC · DO0.0**. Source defaults to **文件夹**.
4. **模拟开线** (or Space) → self-check → confirm.
5. After ~8 consecutive NG the line interlocks. Shift files land in `_sessions/<shift>/`.

To run a full `test/` category, set consecutive NG to **0**. **本机摄像头** previews immediately and detects only after 开线; all-NG is expected.

---

## CLI (optional)

The exe is `surface_defect_detector.exe` under `build/` or `build/Desktop_Qt_.../`.

```bash
./surface_defect_detector.exe
./surface_defect_detector.exe --batch metal_nut
./surface_defect_detector.exe --batch metal_nut --engine dl
```

`--batch` / `--live-smoke` / `--webcam-smoke` skip login. Do not pass `--engine dl` without ONNX. Metrics and extra flags: [DEVELOPMENT.md](DEVELOPMENT.md). Command-line CMake is documented there; day-to-day use Qt Creator.

---

## If it fails

| Symptom | Fix |
|---|---|
| CMake cannot find OpenCV / `videoio` | Missing `third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake`, or MSVC kit |
| Link error on onnxruntime | Re-extract `sdd-third_party.zip` into the repo root |
| Empty tree after login | `Ctrl+O` on the repo root; check `metal_nut/train/good` is not nested |
| EfficientAD will not start | Use 传统 CV, or add the ONNX file |
| Kit list is MSVC only | Add MinGW 64-bit in the Qt installer and reopen the project |

---

## Packing (author)

```powershell
powershell -ExecutionPolicy Bypass -File tools/pack_missing.ps1
```

Writes the three zips under `_dist/` (no `.pdb` / training `.ckpt`). `_dist/` is gitignored.

---

## More docs

- Phases, architecture, measured metrics: [DEVELOPMENT.md](DEVELOPMENT.md)
- Phase diary: [DEVELOPMENT-HISTORY.md](DEVELOPMENT-HISTORY.md)
- Training (frozen; usually skip): [tools/training/TRAIN.md](tools/training/TRAIN.md)
