#!/usr/bin/env bash
set -euo pipefail

APP_NAME="hackenbush"
DIST_DIR="./../dist"
DIST_APP_DIR="${DIST_DIR}/${APP_NAME}.dist"

LIB_SRC="../../../../build/short-games/hackenbush/libhackenbush.so"
FIGS_SRC="./../figs"
MAIN_SRC="./../hackenbush.py"

rm -rf "${DIST_DIR}"
rm -rf "${APP_NAME}.build"

python3 -m nuitka \
  --mode=standalone \
  --enable-plugin=pyside6 \
  --output-dir="${DIST_DIR}" \
  "${MAIN_SRC}"

mkdir -p "${DIST_APP_DIR}"

cp "${LIB_SRC}" "${DIST_APP_DIR}/libhackenbush.so"
cp -a "${FIGS_SRC}" "${DIST_APP_DIR}/figs"
(cd "${DIST_DIR}" && zip -r "${APP_NAME}.zip" "${APP_NAME}.dist")

echo "Done"
echo "Run with:"
echo "./${DIST_APP_DIR}/${APP_NAME}.bin"
