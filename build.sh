#!/usr/bin/env bash
# Compilation de TimeOverlay sous Linux.
set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="build"

echo "=== TimeOverlay - compilation (${BUILD_TYPE}) ==="

missing=()
command -v cmake >/dev/null 2>&1 || missing+=("cmake")
command -v g++   >/dev/null 2>&1 || missing+=("g++")

if [ ${#missing[@]} -ne 0 ]; then
    echo "Outils manquants : ${missing[*]}" >&2
    echo "Sur Debian/Ubuntu :" >&2
    echo "  sudo apt install build-essential cmake qt6-base-dev qt6-multimedia-dev" >&2
    exit 1
fi

# Verifie Qt6 avant de lancer CMake, pour donner un message utile plutot que
# de laisser echouer find_package.
if ! cmake --find-package -DNAME=Qt6 -DCOMPILER_ID=GNU -DLANGUAGE=CXX \
        -DMODE=EXIST >/dev/null 2>&1; then
    echo "Qt6 est introuvable. Sur Debian/Ubuntu :" >&2
    echo "  sudo apt install qt6-base-dev qt6-multimedia-dev" >&2
    echo "Si Qt est installe ailleurs, passez son chemin :" >&2
    echo "  cmake -B build -DCMAKE_PREFIX_PATH=/chemin/vers/Qt/6.x/gcc_64" >&2
fi

cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo
echo "=== Test de fumee ==="
# Verifie sans interface graphique que le minuteur bascule bien en montee a
# zero et que les fichiers de sortie sont independants. Quatre secondes.
if ! "${BUILD_DIR}/tests/TimeOverlaySmokeTest"; then
    echo "ATTENTION : des tests ont echoue. Le binaire existe mais le" >&2
    echo "comportement n'est pas conforme -- ne pas utiliser en direct." >&2
    exit 1
fi

echo
echo "=== Compilation terminee ==="
echo "Binaire : ${BUILD_DIR}/src/TimeOverlay"
echo "Lancement : ./${BUILD_DIR}/src/TimeOverlay"
