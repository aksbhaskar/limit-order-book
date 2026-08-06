#!/usr/bin/env bash
# Reproducible research pipeline: build, run the parameter study, render figures.
#
# Requires a C++20 toolchain, CMake, and Python 3 (standard library only) on PATH.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

BIN="$ROOT/build/examples/lob_param_study"
[ -f "$BIN.exe" ] && BIN="$BIN.exe"

mkdir -p "$ROOT/results"
( cd "$ROOT/results" && "$BIN" )

python "$ROOT/viz/plot_study.py" \
    --runs "$ROOT/results/study_runs.csv" \
    --outdir "$ROOT/docs/figures"

echo "Done. Study data in results/, figures in docs/figures/."
