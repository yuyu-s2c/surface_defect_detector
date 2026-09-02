# 把 Microsoft.ML.OnnxRuntime.DirectML 解到 third_party/onnxruntime/
# （覆盖 CPU 包。DML 包内仍含 CPU EP，一份 dll 同时走 GPU / 回退。）
#
# 版本说明：C++ DirectML 预编译包目前最新是 1.24.4，没有 1.29 的 DML zip/nupkg。
# Python 训练侧仍锁 onnxruntime==1.29.0；ONNX 图本身可被 1.24.4 加载。
# 用法（仓库根）：powershell -ExecutionPolicy Bypass -File tools/fetch_onnxruntime_dml.ps1
$ErrorActionPreference = 'Stop'

$Version = '1.24.4'
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot '..')
$Dest = Join-Path $RepoRoot 'third_party/onnxruntime'
$Url = "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime.directml/$Version/microsoft.ml.onnxruntime.directml.$Version.nupkg"
$Work = Join-Path $env:TEMP "ort-dml-$Version"
$Nupkg = Join-Path $Work "onnxruntime-directml.$Version.zip"

Write-Host "下载 $Url"
if (Test-Path $Work) { Remove-Item -Recurse -Force $Work }
New-Item -ItemType Directory -Path $Work | Out-Null
Invoke-WebRequest -Uri $Url -OutFile $Nupkg -UseBasicParsing

$Extract = Join-Path $Work 'extract'
Expand-Archive -Path $Nupkg -DestinationPath $Extract -Force

$nativeDir = Get-ChildItem -Path $Extract -Recurse -Directory |
    Where-Object { $_.Name -eq 'native' -and $_.FullName -match 'win-x64' } |
    Select-Object -First 1
if (-not $nativeDir) {
    throw "nupkg 里没有 runtimes/win-x64/native（包结构变了？）"
}

$includeSrc = Join-Path $Extract 'build/native/include'
if (-not (Test-Path $includeSrc)) {
    throw "nupkg 里没有 build/native/include"
}

New-Item -ItemType Directory -Path (Join-Path $Dest 'lib') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $Dest 'include') -Force | Out-Null

Copy-Item -Path (Join-Path $nativeDir.FullName '*') -Destination (Join-Path $Dest 'lib') -Force
Copy-Item -Path (Join-Path $includeSrc '*') -Destination (Join-Path $Dest 'include') -Recurse -Force

# 方便 #include <dml_provider_factory.h>（官方路径可能套一层）
$dmlHdr = Get-ChildItem -Path (Join-Path $Dest 'include') -Recurse -Filter 'dml_provider_factory.h' |
    Select-Object -First 1
if ($dmlHdr -and $dmlHdr.DirectoryName -ne (Join-Path $Dest 'include')) {
    Copy-Item -Path $dmlHdr.FullName -Destination (Join-Path $Dest 'include/dml_provider_factory.h') -Force
}

# DirectML.dll 在依赖包 Microsoft.AI.DirectML 里，ORT nupkg 不带
$DmlVer = '1.15.4'
$DmlUrl = "https://api.nuget.org/v3-flatcontainer/microsoft.ai.directml/$DmlVer/microsoft.ai.directml.$DmlVer.nupkg"
$DmlZip = Join-Path $Work "directml.$DmlVer.zip"
$DmlExtract = Join-Path $Work 'directml-extract'
Write-Host "下载 $DmlUrl"
Invoke-WebRequest -Uri $DmlUrl -OutFile $DmlZip -UseBasicParsing
Expand-Archive -Path $DmlZip -DestinationPath $DmlExtract -Force
$dmlDll = Get-ChildItem -Path $DmlExtract -Recurse -Filter 'DirectML.dll' |
    Where-Object { $_.FullName -match 'x64|win-x64' } |
    Select-Object -First 1
if (-not $dmlDll) {
    $dmlDll = Get-ChildItem -Path $DmlExtract -Recurse -Filter 'DirectML.dll' | Select-Object -First 1
}
if (-not $dmlDll) {
    throw "Microsoft.AI.DirectML $DmlVer 里没有 DirectML.dll"
}
Copy-Item -Path $dmlDll.FullName -Destination (Join-Path $Dest 'lib/DirectML.dll') -Force
Write-Host ("  复制 DirectML.dll 自 " + $dmlDll.FullName)

$verFile = Join-Path $Dest 'VERSION_NUMBER'
Set-Content -Path $verFile -Value $Version -NoNewline -Encoding ascii

Write-Host "已安装到 $Dest"
Get-ChildItem (Join-Path $Dest 'lib') | ForEach-Object { Write-Host ("  lib/" + $_.Name) }
if (-not (Test-Path (Join-Path $Dest 'lib/DirectML.dll'))) {
    Write-Warning "lib/DirectML.dll 不在包里：运行时将依赖系统 DirectML，或 DML 会话创建失败后回 CPU"
}
if (-not (Test-Path (Join-Path $Dest 'include/dml_provider_factory.h'))) {
    throw "缺少 dml_provider_factory.h，无法编译 DML EP"
}
Write-Host "OK"
