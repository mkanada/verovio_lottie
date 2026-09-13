#!/usr/bin/env bash
# Gera, para uma página de uma partitura, o PNG do SVG, o PNG do Lottie e a
# imagem de diferença — ver compare/README.md.
set -euo pipefail

if [[ $# -lt 2 || $# -gt 3 ]]; then
    echo "Uso: $0 <arquivo> <página> [tolerância]" >&2
    exit 1
fi

INPUT_FILE=$1
PAGE=$2
TOLERANCE=${3:-32}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VEROVIO_BIN="$REPO_ROOT/verovio/tools/verovio"
COMPARE_BIN="$REPO_ROOT/compare/target/release/compare"
RESOURCE_PATH="$REPO_ROOT/verovio/data"
OUT_DIR="$REPO_ROOT/compare/out"

if [[ ! -x "$VEROVIO_BIN" ]]; then
    echo "Binário do Verovio não encontrado em $VEROVIO_BIN." >&2
    echo "Compile com: cd $REPO_ROOT/verovio/tools && cmake ../cmake && make -j4" >&2
    exit 1
fi

if [[ ! -x "$COMPARE_BIN" ]]; then
    echo "Binário do compare não encontrado em $COMPARE_BIN." >&2
    echo "Compile com: cd $REPO_ROOT/compare && cargo build --release" >&2
    exit 1
fi

if [[ ! -f "$INPUT_FILE" ]]; then
    echo "Arquivo de entrada não encontrado: $INPUT_FILE" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

BASENAME="$(basename "$INPUT_FILE")"
NAME="${BASENAME%.*}"
PREFIX="$OUT_DIR/${NAME}-p${PAGE}"

# Renderiza com um prefixo sem pontos: o `-o` do verovio trunca tudo a partir
# do último "." do caminho (RemoveExtension em tools/main.cpp), o que
# corromperia nomes de saída para arquivos de entrada cujo nome já tem pontos
# (ex.: alguns .mxl do corpus). Renderiza num nome temporário e move depois.
TMP_PREFIX="$OUT_DIR/_compare-page-tmp"

FONTS=(
    "$REPO_ROOT/verovio/fonts/Leipzig/Leipzig.ttf"
    "$REPO_ROOT/verovio/fonts/Bravura/Bravura.otf"
    "$REPO_ROOT/verovio/fonts/Leland/Leland.otf"
    "$REPO_ROOT/verovio/fonts/Gootville/Gootville.otf"
)
FONT_ARGS=()
for font in "${FONTS[@]}"; do
    FONT_ARGS+=(--font "$font")
done

echo "==> Renderizando SVG (página $PAGE)"
"$VEROVIO_BIN" -t svg -p "$PAGE" -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "$INPUT_FILE"
mv "$TMP_PREFIX.svg" "$PREFIX.svg"

echo "==> Renderizando Lottie (página $PAGE)"
"$VEROVIO_BIN" -t lottie -p "$PAGE" -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "$INPUT_FILE"
mv "$TMP_PREFIX.json" "$PREFIX.json"

echo "==> SVG -> PNG"
"$COMPARE_BIN" svg-to-png "$PREFIX.svg" "$PREFIX-svg.png" "${FONT_ARGS[@]}"

echo "==> Lendo dimensões do PNG do SVG"
read -r WIDTH HEIGHT < <(python3 - "$PREFIX-svg.png" <<'PYEOF'
import struct
import sys

with open(sys.argv[1], "rb") as f:
    header = f.read(24)
width, height = struct.unpack(">II", header[16:24])
print(width, height)
PYEOF
)

echo "==> Lottie -> PNG (${WIDTH}x${HEIGHT})"
"$COMPARE_BIN" lottie-to-png "$PREFIX.json" "$PREFIX-lottie.png" --width "$WIDTH" --height "$HEIGHT"

echo "==> Diff (tolerância $TOLERANCE)"
"$COMPARE_BIN" diff "$PREFIX-svg.png" "$PREFIX-lottie.png" "$PREFIX-diff.png" --tolerance "$TOLERANCE"
