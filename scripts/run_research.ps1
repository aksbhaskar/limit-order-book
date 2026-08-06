# Reproducible research pipeline (Windows / PowerShell): build, run the
# parameter study, render figures.
#
# Requires a C++20 toolchain, CMake, and Python 3 (standard library only) on PATH.
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

New-Item -ItemType Directory -Force "$root\results" | Out-Null
Push-Location "$root\results"
& "$root\build\examples\lob_param_study.exe"
Pop-Location

python "$root\viz\plot_study.py" --runs "$root\results\study_runs.csv" --outdir "$root\docs\figures"

Write-Host "Done. Study data in results/, figures in docs/figures/."
