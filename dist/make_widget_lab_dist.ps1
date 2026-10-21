# Builds widget_lab and assembles a minimal runnable distribution zip:
#   dist/widget_lab_dist.zip
#   └── widget_lab/
#       ├── widget_lab.exe            (permonitorv2 manifest embedded)
#       ├── msvcp140/vcruntime140/vcruntime140_1.dll
#       └── resources/
#           ├── fonts/  (the four TTFs Label::initSysFont loads + OFL.txt)
#           └── images/ (full icon pool)
# Run:  powershell -NoProfile -ExecutionPolicy Bypass -File make_widget_lab_dist.ps1
param([string]$Configuration = "Release")
$ErrorActionPreference = "Stop"

$root  = Split-Path -Parent $PSScriptRoot          # OrcaSlicer/
$build = Join-Path $root "build"
$exe   = Join-Path $build "src/widget_lab/$Configuration/widget_lab.exe"
$res   = Join-Path $root "resources"

# 0) a lingering lab instance locks the font files - shut it down first
Get-Process widget_lab -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500

# 1) build
cmake --build $build --config $Configuration --target widget_lab -- -m
if ($LASTEXITCODE -ne 0) { throw "widget_lab build failed" }
if (-not (Test-Path $exe)) { throw "widget_lab.exe not found at $exe" }

# 2) stage the minimal runnable directory
$stage = Join-Path $root "dist/widget_lab"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path (Join-Path $stage "resources/fonts") -Force | Out-Null

Copy-Item $exe $stage
foreach ($dll in "msvcp140.dll", "vcruntime140.dll", "vcruntime140_1.dll") {
    Copy-Item (Join-Path $env:windir "System32/$dll") $stage
}
foreach ($ttf in "HarmonyOS_Sans_SC_Bold.ttf", "HarmonyOS_Sans_SC_Regular.ttf",
                 "NanumGothic-Bold.ttf", "NanumGothic-Regular.ttf", "OFL.txt") {
    Copy-Item (Join-Path $res "fonts/$ttf") (Join-Path $stage "resources/fonts/")
}
Copy-Item (Join-Path $res "images") (Join-Path $stage "resources/images") -Recurse

# 3) zip
$zip = Join-Path $root "dist/widget_lab_dist.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $stage -DestinationPath $zip -Force

$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
$files = (Get-ChildItem $stage -Recurse -File).Count
Write-Host "`nwidget_lab distribution: $zip ($mb MB, $files files)"
Write-Host "Receiver: unzip anywhere, double-click widget_lab.exe (no VC redist needed)."
