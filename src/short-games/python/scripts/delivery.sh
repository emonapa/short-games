#!/usr/bin/env bash
set -euo pipefail

APP_NAME="hotpotch"
DIST_DIR="./../dist"
DIST_APP_DIR="${DIST_DIR}/${APP_NAME}.dist"

LIB_SRC="../../../../build/short-games/hotpotch/libhotpotch.so"
UTILS_SRC="./../hotpotch_utils"
FIGS_SRC="${UTILS_SRC}/figs"
MAIN_SRC="./../hotpotch.py"

rm -rf "${DIST_DIR}"
rm -rf "${APP_NAME}.build"

python3 -m nuitka \
  --mode=standalone \
  --enable-plugin=pyside6 \
  --output-dir="${DIST_DIR}" \
  --include-plugin-directory="${UTILS_SRC}" \
  --include-data-dir="${FIGS_SRC}=hotpotch_utils/figs" \
  "${MAIN_SRC}"

mkdir -p "${DIST_APP_DIR}"

cp "${LIB_SRC}" "${DIST_APP_DIR}/libhotpotch.so"

(cd "${DIST_DIR}" && zip -r "${APP_NAME}.zip" "${APP_NAME}.dist")

echo "Done"
echo "Run with:"
echo "./${DIST_APP_DIR}/${APP_NAME}.bin"
