#!/usr/bin/env bash
set -euo pipefail

APP_NAME="main"
DIST_DIR="dist"
DIST_APP_DIR="${DIST_DIR}/${APP_NAME}.dist"

LIB_SRC="../../../../build/RBG/libhb.so"
FIGS_SRC="./../figs"
MAIN_SRC="./../main.py"

rm -rf "${DIST_DIR}"
rm -rf "${APP_NAME}.build"

python3 -m nuitka \
  --mode=standalone \
  --enable-plugin=pyside6 \
  --output-dir="${DIST_DIR}" \
  "${MAIN_SRC}"

mkdir -p "${DIST_APP_DIR}"

cp "${LIB_SRC}" "${DIST_APP_DIR}/libhb.so"
cp -a "${FIGS_SRC}" "${DIST_APP_DIR}/figs"

echo "Hotovo"
echo "Spusteni:"
echo "./${DIST_APP_DIR}/${APP_NAME}"
