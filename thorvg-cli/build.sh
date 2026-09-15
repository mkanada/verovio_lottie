#!/usr/bin/env bash
# Builda o ThorVG local do repositório (thorvg/, com as correções próprias
# listadas em thorvg/VEROVIO_LOTTIE.md) com loaders de SVG e Lottie - o
# compare/ compila esse mesmo código via dotlottie-rs, mas só com o loader de
# Lottie, ver thorvg-cli/README.md - e compila a ferramenta thorvg-render
# (src/render.cpp) contra ele.
#
# Uso: thorvg-cli/build.sh
#
# Pré-requisito: `meson` (`pip install --user meson`; ninja já costuma
# estar disponível).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
THORVG_DIR="$REPO_ROOT/thorvg"
# Build fora da árvore de código: thorvg/ fica só com fonte versionado.
BUILD_DIR="$SCRIPT_DIR/build"
BIN_DIR="$SCRIPT_DIR/bin"

MESON="$(command -v meson || echo "$HOME/.local/bin/meson")"
if [[ ! -x "$MESON" ]]; then
    echo "meson não encontrado (nem no PATH nem em ~/.local/bin). Instale com:" >&2
    echo "  pip install --user meson" >&2
    exit 1
fi

if [[ ! -f "$BUILD_DIR/build.ninja" ]]; then
    echo "==> Configurando o build do ThorVG (loaders svg+lottie+ttf+otf+png)"
    "$MESON" setup "$BUILD_DIR" "$THORVG_DIR" \
        -Dloaders="svg,lottie,ttf,otf,png" \
        -Dsavers="" \
        -Dtools="svg2png" \
        -Dbindings="" \
        -Dengines="cpu" \
        -Dthreads=true \
        -Dstatic=true \
        -Dlog=true \
        -Dextra="" \
        -Dtests=false \
        --buildtype=release
fi

echo "==> Buildando o ThorVG (ninja)"
ninja -C "$BUILD_DIR"

echo "==> Compilando thorvg-render"
mkdir -p "$BIN_DIR"
# lodepng vem do próprio tools/svg2png/ do ThorVG (usado só pra CODIFICAR o
# PNG de saída - o loader de PNG do ThorVG, dentro de libthorvg, só
# decodifica; e libthorvg é uma lib DINÂMICA aqui, então seus símbolos
# internos de lodepng não ficam expostos no .so - sem colisão de símbolo
# com esta cópia, ao contrário do que acontecia linkando contra o
# libthorvg.a ESTÁTICO que o dotlottie-rs gera pro compare/ (ver notas de
# investigação na sessão que criou este diretório).
g++ -std=c++17 -O2 \
    -I "$THORVG_DIR/inc" \
    -I "$THORVG_DIR/tools/svg2png" \
    "$SCRIPT_DIR/src/render.cpp" \
    "$THORVG_DIR/tools/svg2png/lodepng.cpp" \
    -L "$BUILD_DIR/src" -lthorvg-1 \
    -Wl,-rpath,"$BUILD_DIR/src" \
    -o "$BIN_DIR/thorvg-render"

echo
echo "Concluído: $BIN_DIR/thorvg-render"
