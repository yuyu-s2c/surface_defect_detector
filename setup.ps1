# 朋友把 sdd-*.zip 放到仓库根后运行本脚本，配齐 Git 里没有的依赖。
# 用法：双击 setup.bat，或在仓库根
#   powershell -ExecutionPolicy Bypass -File .\setup.ps1
$ErrorActionPreference = 'Stop'

$Root = $PSScriptRoot
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) {
    Write-Host '请把 setup.ps1 / setup.bat 放在仓库根（和 CMakeLists.txt 同一层）再运行。' -ForegroundColor Red
    exit 2
}

function Find-Zip([string]$Name) {
    foreach ($dir in @($Root, (Join-Path $Root '_dist'))) {
        $p = Join-Path $dir $Name
        if (Test-Path -LiteralPath $p) { return (Resolve-Path -LiteralPath $p).Path }
    }
    return $null
}

function Test-Rel([string]$Rel) {
    return (Test-Path -LiteralPath (Join-Path $Root $Rel))
}

function Move-IfNested([string]$WrongRel, [string]$InnerName) {
    $wrong = Join-Path $Root $WrongRel
    $inner = Join-Path $wrong $InnerName
    $dest = Join-Path $Root $InnerName
    if ((Test-Path -LiteralPath $inner) -and -not (Test-Path -LiteralPath $dest)) {
        Write-Host "  纠正套层：$WrongRel/$InnerName → $InnerName"
        Move-Item -LiteralPath $inner -Destination $dest
        if ((Get-ChildItem -LiteralPath $wrong -Force -ErrorAction SilentlyContinue | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $wrong -Force -ErrorAction SilentlyContinue
        }
        return $true
    }
    return $false
}

function Flatten-Category([string]$Name) {
    $outer = Join-Path $Root $Name
    $inner = Join-Path $outer $Name
    if ((Test-Path -LiteralPath (Join-Path $inner 'train')) -and
        -not (Test-Path -LiteralPath (Join-Path $outer 'train'))) {
        Write-Host "  纠正套层：$Name/$Name → $Name"
        Get-ChildItem -LiteralPath $inner | Move-Item -Destination $outer
        Remove-Item -LiteralPath $inner -Recurse -Force
    }
}

function Expand-Pack([string]$ZipName, [string]$MarkerRel, [bool]$Required) {
    $zip = Find-Zip $ZipName
    if ($zip) {
        Write-Host "解压 $ZipName"
        Write-Host "  来自 $zip"
        & tar -xf $zip -C $Root
        if ($LASTEXITCODE -ne 0) {
            Write-Host "  失败：tar 解压出错（zip 是否下完整？）" -ForegroundColor Red
            return $false
        }
        return $true
    }
    if (Test-Rel $MarkerRel) {
        Write-Host "已有 $MarkerRel（未找到 $ZipName，跳过）"
        return $true
    }
    if ($Required) {
        Write-Host "缺少 $ZipName，也没有 $MarkerRel。" -ForegroundColor Red
        Write-Host "  把 $ZipName 放到仓库根（和 CMakeLists.txt 同一层）再跑一次。" -ForegroundColor Red
        return $false
    }
    Write-Host "未找到 $ZipName（可选，没有则用传统 CV）" -ForegroundColor Yellow
    return $true
}

Write-Host "仓库根：$Root"
Write-Host ''

$okThird = Expand-Pack 'sdd-third_party.zip' 'third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake' $true
$okData = Expand-Pack 'sdd-datasets.zip' 'metal_nut/train/good' $true
$okModels = Expand-Pack 'sdd-models.zip' 'models/metal_nut/weights/onnx/metal_nut.onnx' $false

Write-Host ''
Write-Host '纠正常见套层…'
[void](Move-IfNested 'sdd-third_party' 'third_party')
[void](Move-IfNested 'sdd-datasets' 'metal_nut')
[void](Move-IfNested 'sdd-datasets' 'screw')
[void](Move-IfNested 'sdd-models' 'models')
Flatten-Category 'metal_nut'
Flatten-Category 'screw'

Write-Host ''
Write-Host '检查：'
$checks = @(
    @{ Rel = 'third_party/opencv/x64/mingw/lib/OpenCVConfig.cmake'; Need = $true; Name = 'OpenCV' },
    @{ Rel = 'third_party/onnxruntime/lib/onnxruntime.dll'; Need = $true; Name = 'ONNX Runtime' },
    @{ Rel = 'metal_nut/train/good'; Need = $true; Name = 'metal_nut 数据集' },
    @{ Rel = 'screw/train/good'; Need = $true; Name = 'screw 数据集' },
    @{ Rel = 'models/metal_nut/weights/onnx/metal_nut.onnx'; Need = $false; Name = 'metal_nut ONNX' },
    @{ Rel = 'models/screw/weights/onnx/screw.onnx'; Need = $false; Name = 'screw ONNX' }
)

$failed = $false
foreach ($c in $checks) {
    if (Test-Rel $c.Rel) {
        Write-Host ("  [OK]  {0}" -f $c.Name)
    } elseif ($c.Need) {
        Write-Host ("  [缺]  {0}  →  {1}" -f $c.Name, $c.Rel) -ForegroundColor Red
        $failed = $true
    } else {
        Write-Host ("  [--]  {0}（没有也能跑，引擎选传统 CV）" -f $c.Name) -ForegroundColor Yellow
    }
}

Write-Host ''
if ($failed -or -not $okThird -or -not $okData) {
    Write-Host '还没配齐。把缺的 zip 放到仓库根，再双击 setup.bat。' -ForegroundColor Red
    exit 1
}

Write-Host '配好了。下一步：Qt Creator 打开本目录的 CMakeLists.txt，套件选 MinGW 64-bit，构建并运行。' -ForegroundColor Green
Write-Host '登录 engineer / 123456，Ctrl+O 选这个仓库根（能看见 metal_nut 的那一层）。'
if (-not (Test-Rel 'models/metal_nut/weights/onnx/metal_nut.onnx')) {
    Write-Host '没有模型包：引擎选传统 CV。'
}
exit 0
