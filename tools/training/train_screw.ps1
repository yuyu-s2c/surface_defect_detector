# 在仓库根目录执行：powershell -File tools\training\train_screw.ps1
$ErrorActionPreference = "Stop"
Set-Location (Resolve-Path "$PSScriptRoot\..\..")
New-Item -ItemType Directory -Force -Path "models\screw" | Out-Null
$env:PYTHONUNBUFFERED = "1"
& "tools\training\.venv\Scripts\python.exe" -u "tools\training\train_efficientad.py" screw --steps 70000 2>&1 |
    Tee-Object -FilePath "models\screw\train.log"
exit $LASTEXITCODE
