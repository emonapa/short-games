#!/usr/bin/env bash

set -Eeuo pipefail

readonly EMSDK_VERSION="6.0.1"
readonly EMSDK_REPOSITORY="https://github.com/emscripten-core/emsdk.git"

PROJECT_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
readonly PROJECT_ROOT
readonly VENV_DIR="${PROJECT_ROOT}/venv"
readonly EMSDK_DIR="${PROJECT_ROOT}/emsdk"
readonly REQUIREMENTS_FILE="${PROJECT_ROOT}/requirements-dev.txt"

step() {
    printf '\n[setup] %s\n' "$1"
}

fail() {
    printf '\n[setup] Error: %s\n' "$1" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 ||
        fail "Missing required command '$1'. Install it with your system package manager and run this script again."
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    printf 'Usage: %s\n' "${0##*/}"
    printf 'Creates venv, installs Python dependencies and prepares Emscripten %s.\n' "${EMSDK_VERSION}"
    exit 0
fi

[[ $# -eq 0 ]] || fail "Unknown argument '$1'. Use --help for usage."

case "$(uname -s)" in
    Linux|Darwin) ;;
    *) fail "This setup script supports Linux and macOS. On Windows, use WSL." ;;
esac

for tool in python3 git make cc; do
    require_command "${tool}"
done

python3 -c 'import sys; raise SystemExit(sys.version_info < (3, 8))' ||
    fail "Python 3.8 or newer is required."

[[ -f "${REQUIREMENTS_FILE}" ]] ||
    fail "Missing ${REQUIREMENTS_FILE}."

step "Preparing Python virtual environment"
if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
    python3 -m venv "${VENV_DIR}" ||
        fail "Could not create venv. Install the venv module for your Python distribution and run this script again."
fi

"${VENV_DIR}/bin/python" -m pip install --upgrade pip
"${VENV_DIR}/bin/python" -m pip install -r "${REQUIREMENTS_FILE}"

step "Preparing Emscripten SDK ${EMSDK_VERSION}"
if [[ -e "${EMSDK_DIR}" && ! -x "${EMSDK_DIR}/emsdk" ]]; then
    fail "${EMSDK_DIR} exists but is not a valid emsdk checkout."
fi

if [[ ! -d "${EMSDK_DIR}" ]]; then
    git clone --depth 1 "${EMSDK_REPOSITORY}" "${EMSDK_DIR}"
fi

"${EMSDK_DIR}/emsdk" install "${EMSDK_VERSION}"
"${EMSDK_DIR}/emsdk" activate "${EMSDK_VERSION}"

# Activating emsdk only affects this process. Source it here to verify the
# compiler, then print the two commands needed for the user's current shell.
# shellcheck disable=SC1091
export EMSDK_QUIET=1
source "${EMSDK_DIR}/emsdk_env.sh" >/dev/null
emcc --version >/dev/null

step "Setup complete"
printf '%s\n' 'Activate the tools in your current shell with:'
printf '  source %q\n' "${VENV_DIR}/bin/activate"
printf '  source %q\n' "${EMSDK_DIR}/emsdk_env.sh"
printf '\n%s\n' 'Then build the native libraries or the web version with:'
printf '%s\n' '  make all'
printf '%s\n' '  make -f web_Makefile all'
