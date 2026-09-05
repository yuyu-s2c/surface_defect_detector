#!/usr/bin/env bash
# AutoDL / Linux：在仓库根（含 screw/ 与 tools/training/）执行。
#   bash tools/training/train_screw_m.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
mkdir -p models/screw_m
export PYTHONUNBUFFERED=1
PY="${PYTHON:-python}"
"$PY" -u tools/training/train_efficientad.py screw \
  --model-size medium \
  --steps 70000 \
  --image-size 256 \
  --num-workers 4 \
  2>&1 | tee -a models/screw_m/train.log
