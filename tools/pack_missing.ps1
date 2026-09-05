# 把仓库里不入库的依赖打成 3 个 zip，发给朋友后解压到仓库根即可。
# 不含 onnxruntime.pdb、训练 ckpt、日志（朋友编译/跑检测用不到）。
# 用法（仓库根）：powershell -ExecutionPolicy Bypass -File tools/pack_missing.ps1
$ErrorActionPreference = 'Stop'

$Root = Resolve-Path (Join-Path $PSScriptRoot '..')
$Out = Join-Path $Root '_dist'
$Stage = Join-Path $Out '_stage'

function Assert-Exists([string]$Rel) {
    $p = Join-Path $Root $Rel
    if (-not (Test-Path $p)) { throw "缺少 $Rel，先在本机准备好再打包" }
}

function New-ZipFromStage([string]$ZipName, [string[]]$TopNames) {
    $zip = Join-Path $Out $ZipName
    if (Test-Path $zip) { Remove-Item -Force $zip }
    $args = @('-a', '-cf', $zip, '-C', $Stage) + $TopNames
    & tar @args
    if ($LASTEXITCODE -ne 0) { throw "打包 $ZipName 失败" }
    $mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
    Write-Host ("  {0}  {1} MB" -f $ZipName, $mb)
}

Assert-Exists 'third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake'
Assert-Exists 'third_party/onnxruntime/lib/onnxruntime.dll'
Assert-Exists 'metal_nut/train/good'
Assert-Exists 'screw/train/good'
Assert-Exists 'models/metal_nut/weights/onnx/metal_nut.onnx'
Assert-Exists 'models/screw/weights/onnx/screw.onnx'

if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force -Path $Out | Out-Null
New-Item -ItemType Directory -Force -Path $Stage | Out-Null

Write-Host '准备 third_party（去掉 .pdb）...'
Copy-Item -Recurse (Join-Path $Root 'third_party\opencv') (Join-Path $Stage 'third_party\opencv')
New-Item -ItemType Directory -Force -Path (Join-Path $Stage 'third_party\onnxruntime\include') | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Stage 'third_party\onnxruntime\lib') | Out-Null
Copy-Item -Recurse (Join-Path $Root 'third_party\onnxruntime\include\*') (Join-Path $Stage 'third_party\onnxruntime\include')
Get-ChildItem (Join-Path $Root 'third_party\onnxruntime\lib') -File |
    Where-Object { $_.Extension -ne '.pdb' } |
    ForEach-Object { Copy-Item $_.FullName (Join-Path $Stage 'third_party\onnxruntime\lib') }
Get-ChildItem (Join-Path $Root 'third_party\onnxruntime') -File |
    ForEach-Object { Copy-Item $_.FullName (Join-Path $Stage 'third_party\onnxruntime') }

Write-Host '准备数据集...'
Copy-Item -Recurse (Join-Path $Root 'metal_nut') (Join-Path $Stage 'metal_nut')
Copy-Item -Recurse (Join-Path $Root 'screw') (Join-Path $Stage 'screw')

Write-Host '准备模型（只要推理 ONNX / 标定 json）...'
foreach ($cat in @('metal_nut', 'screw')) {
    $src = Join-Path $Root "models\$cat\weights\onnx"
    $dst = Join-Path $Stage "models\$cat\weights\onnx"
    New-Item -ItemType Directory -Force -Path $dst | Out-Null
    Get-ChildItem $src -File | Where-Object { $_.Extension -in '.onnx', '.json' } |
        ForEach-Object { Copy-Item $_.FullName $dst }
}

Write-Host '压缩...'
New-ZipFromStage 'sdd-third_party.zip' @('third_party')
New-ZipFromStage 'sdd-datasets.zip' @('metal_nut', 'screw')
New-ZipFromStage 'sdd-models.zip' @('models')

Remove-Item -Recurse -Force $Stage
Write-Host "完成：$Out"
Write-Host '发给朋友：三个 zip 放到仓库根，双击 setup.bat。'
Write-Host '微信约 100MB 上限，数据集包请走网盘或 QQ。'
