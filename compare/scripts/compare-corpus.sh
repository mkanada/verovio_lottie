#!/usr/bin/env bash
# Varre o corpus inteiro (corpus/mei + corpus/musicxml): para cada partitura,
# gera o SVG de todas as páginas, o pacote .lottie e, por página, o PNG do
# SVG, o PNG do frame correspondente do .lottie e a imagem de diff — ver
# docs/plano/A13-varredura-do-corpus.md.
#
# Uso: compare/scripts/compare-corpus.sh [tolerância]
#
# Saídas em compare/out/corpus/<peça>/ e uma tabela CSV consolidada em
# compare/out/corpus/resultado.csv (peça,pagina,pct_diferente,pixels_diferentes,pixels_totais).
set -euo pipefail

TOLERANCE=${1:-32}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VEROVIO_BIN="$REPO_ROOT/verovio/tools/verovio"
COMPARE_BIN="$REPO_ROOT/compare/build/linux/x64/release/bundle/compare"
SVG_RENDER_BIN="$REPO_ROOT/compare/svg_render/target/release/svg_render"
RESOURCE_PATH="$REPO_ROOT/verovio/data"
OUT_DIR="$REPO_ROOT/compare/out/corpus"

if [[ ! -x "$VEROVIO_BIN" ]]; then
    echo "Binário do Verovio não encontrado em $VEROVIO_BIN." >&2
    echo "Compile com: cd $REPO_ROOT/verovio/tools && cmake ../cmake && make -j4" >&2
    exit 1
fi

if [[ ! -x "$COMPARE_BIN" ]]; then
    echo "Binário do compare não encontrado em $COMPARE_BIN." >&2
    echo "Compile com: cd $REPO_ROOT/compare && flutter build linux --release" >&2
    exit 1
fi

if [[ ! -x "$SVG_RENDER_BIN" ]]; then
    echo "Binário do svg_render não encontrado em $SVG_RENDER_BIN." >&2
    echo "Compile com: cd $REPO_ROOT/compare/svg_render && cargo build --release" >&2
    exit 1
fi

# O compare é um app Flutter/Linux: mesmo em modo batch precisa de um display
# para inicializar o motor. Sem DISPLAY, roda sob xvfb-run.
if [[ -z "${DISPLAY:-}" ]] && command -v xvfb-run >/dev/null 2>&1; then
    COMPARE_RUN=(xvfb-run -a "$COMPARE_BIN")
else
    COMPARE_RUN=("$COMPARE_BIN")
fi

mkdir -p "$OUT_DIR"

FONTS=(
    "$REPO_ROOT/verovio/fonts/Leipzig/Leipzig.ttf"
    "$REPO_ROOT/verovio/fonts/Bravura/Bravura.otf"
    "$REPO_ROOT/verovio/fonts/Leland/Leland.otf"
    "$REPO_ROOT/verovio/fonts/Gootville/Gootville.otf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Regular.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Italic.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Bold.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-BoldItalic.ttf"
)
FONT_ARGS=()
for font in "${FONTS[@]}"; do
    FONT_ARGS+=(--font "$font")
done

CSV="$OUT_DIR/resultado.csv"
echo "peca,pagina,pct_diferente,pixels_diferentes,pixels_totais" > "$CSV"
: > "$OUT_DIR/tamanhos.txt"

shopt -s nullglob
INPUT_FILES=("$REPO_ROOT"/corpus/mei/*.mei "$REPO_ROOT"/corpus/musicxml/*.mxl)
shopt -u nullglob

for INPUT_FILE in "${INPUT_FILES[@]}"; do
    BASENAME="$(basename "$INPUT_FILE")"
    NAME="${BASENAME%.*}"
    WORKDIR="$OUT_DIR/$NAME"
    mkdir -p "$WORKDIR"
    # Prefixo sem pontos: nomes de peça com "." (ex.: alguns .mxl do corpus) fariam
    # o RemoveExtension do verovio truncar o caminho inteiro (não só a extensão) se
    # usados diretamente em -o — mesma pegadinha documentada em compare-page.sh.
    TMP_PREFIX="$OUT_DIR/_compare-corpus-tmp"

    echo "=== $NAME ==="

    echo "==> Renderizando SVG (todas as páginas)"
    "$VEROVIO_BIN" -t svg -a -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "$INPUT_FILE"

    echo "==> Renderizando dotLottie"
    "$VEROVIO_BIN" -t dotlottie -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "$INPUT_FILE"
    LOTTIE_FILE="$WORKDIR/$NAME.lottie"
    mv "$TMP_PREFIX.lottie" "$LOTTIE_FILE"

    LOTTIE_SIZE=$(stat -c%s "$LOTTIE_FILE")
    echo "$NAME: $LOTTIE_SIZE bytes" >> "$OUT_DIR/tamanhos.txt"

    # `-a` só sufixa "_NNN" quando há mais de uma página (from < to em main.cpp);
    # com página única o arquivo sai sem sufixo.
    shopt -s nullglob
    PAGE_SVGS=("$TMP_PREFIX"_*.svg)
    shopt -u nullglob
    if [[ ${#PAGE_SVGS[@]} -eq 0 ]]; then
        PAGE_SVGS=("$TMP_PREFIX.svg")
    fi

    PAGE_NUM=0
    for SVG_FILE in "${PAGE_SVGS[@]}"; do
        PAGE_NUM=$((PAGE_NUM + 1))
        PAGE_PREFIX="$WORKDIR/${NAME}-p${PAGE_NUM}"
        mv "$SVG_FILE" "$PAGE_PREFIX.svg"

        echo "==> Página $PAGE_NUM: SVG -> PNG"
        "$SVG_RENDER_BIN" "$PAGE_PREFIX.svg" "$PAGE_PREFIX-svg.png" "${FONT_ARGS[@]}" --pin-serif-family "Liberation Serif"

        read -r WIDTH HEIGHT < <(python3 - "$PAGE_PREFIX-svg.png" <<'PYEOF'
import struct
import sys

with open(sys.argv[1], "rb") as f:
    header = f.read(24)
width, height = struct.unpack(">II", header[16:24])
print(width, height)
PYEOF
)

        # Frame de repouso da câmera pra página $PAGE_NUM (docs/plano/C04-paginas-virada.md) -
        # mesmo formato de lookup de marker "page<N-1>" que compare-page.sh usa, com o mesmo
        # fallback pra "frame == página - 1" pra pacotes sem trilha horizontal (1 página só).
        FRAME=$(python3 - "$LOTTIE_FILE" "$PAGE_NUM" <<'PYEOF'
import json
import subprocess
import sys

lottie_path, page = sys.argv[1], int(sys.argv[2])
raw = subprocess.run(["unzip", "-p", lottie_path, "a/score.json"], capture_output=True, check=True).stdout
data = json.loads(raw)
marker_name = f"page{page - 1}"
for marker in data.get("markers", []):
    if marker.get("cm") == marker_name:
        print(marker["tm"])
        sys.exit(0)
print(page - 1)
PYEOF
)
        echo "==> Página $PAGE_NUM: Lottie -> PNG (frame $FRAME, ${WIDTH}x${HEIGHT})"
        "${COMPARE_RUN[@]}" lottie-to-png "$LOTTIE_FILE" "$PAGE_PREFIX-lottie.png" --width "$WIDTH" --height "$HEIGHT" --frame "$FRAME"

        echo "==> Página $PAGE_NUM: diff (tolerância $TOLERANCE)"
        DIFF_OUTPUT=$("${COMPARE_RUN[@]}" diff "$PAGE_PREFIX-svg.png" "$PAGE_PREFIX-lottie.png" "$PAGE_PREFIX-diff.png" --tolerance "$TOLERANCE")
        echo "$DIFF_OUTPUT"

        PCT=$(echo "$DIFF_OUTPUT" | grep -oP '\(\K[0-9.]+(?=%\))')
        DIFF_PIXELS=$(echo "$DIFF_OUTPUT" | grep -oP 'diferentes \(tolerância [0-9]+\): \K[0-9]+')
        TOTAL_PIXELS=$(echo "$DIFF_OUTPUT" | grep -oP 'comparados: \K[0-9]+')

        echo "$NAME,$PAGE_NUM,$PCT,$DIFF_PIXELS,$TOTAL_PIXELS" >> "$CSV"
    done

    rm -f "$TMP_PREFIX".*
done

echo
echo "Concluído. Resultados em $CSV, tamanhos em $OUT_DIR/tamanhos.txt"
