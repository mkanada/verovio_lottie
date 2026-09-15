#!/usr/bin/env bash
# Varre uma matriz de combinações de opções de layout do Verovio (tamanho de
# página, orientação, cabeçalho, rodapé) para UMA peça do corpus e compara,
# para cada combinação, a página 1 renderizada em SVG vs. a mesma página do
# pacote .lottie gerado com as mesmas opções — ver docs/matriz-layout/README.md
# para o motivo de cada eixo e os resultados consolidados.
#
# Uso: compare/scripts/compare-layout-matrix.sh [arquivo] [tolerância]
#
# Saídas (comitadas, ao contrário de compare/out/) em
# docs/matriz-layout/<combinação>/ e uma tabela CSV consolidada em
# docs/matriz-layout/resultado.csv.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

INPUT_FILE=${1:-"$REPO_ROOT/corpus/mei/Chopin_Etude_Op10_No9.mei"}
TOLERANCE=${2:-32}

VEROVIO_BIN="$REPO_ROOT/verovio/tools/verovio"
COMPARE_BIN="$REPO_ROOT/compare/target/release/compare"
RESOURCE_PATH="$REPO_ROOT/verovio/data"
OUT_DIR="$REPO_ROOT/docs/matriz-layout"
TMP_PREFIX="$OUT_DIR/_compare-layout-matrix-tmp"

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

# Eixos da matriz — ver docs/matriz-layout/README.md pela justificativa de
# cada valor escolhido.
SIZES=(a4 tela)
ORIENTATIONS=(portrait landscape)
HEADERS=(header no-header)
FOOTERS=(footer no-footer)

CSV="$OUT_DIR/resultado.csv"
echo "combinacao,tamanho,orientacao,cabecalho,rodape,largura_px,altura_px,paginas,pct_diferente_p1,pixels_diferentes_p1,pixels_totais_p1,tamanho_lottie_bytes" > "$CSV"

for SIZE in "${SIZES[@]}"; do
    if [[ "$SIZE" == "a4" ]]; then
        BASE_WIDTH=2100
        BASE_HEIGHT=2970
    else
        # "Tela": tamanho pensado pra exibição em tela (celular/tablet em pé),
        # bem menor que a A4 (2100x2970) — ver docs/matriz-layout/README.md.
        BASE_WIDTH=1080
        BASE_HEIGHT=1920
    fi

    for ORIENTATION in "${ORIENTATIONS[@]}"; do
        SIZE_ARGS=(--page-width "$BASE_WIDTH" --page-height "$BASE_HEIGHT")
        if [[ "$ORIENTATION" == "landscape" ]]; then
            SIZE_ARGS+=(--landscape)
        fi

        for HEADER in "${HEADERS[@]}"; do
            HEADER_ARGS=()
            if [[ "$HEADER" == "no-header" ]]; then
                HEADER_ARGS=(--header none)
            fi

            for FOOTER in "${FOOTERS[@]}"; do
                FOOTER_ARGS=()
                if [[ "$FOOTER" == "no-footer" ]]; then
                    FOOTER_ARGS=(--footer none)
                fi

                SLUG="${SIZE}-${ORIENTATION}-${HEADER}-${FOOTER}"
                WORKDIR="$OUT_DIR/$SLUG"
                mkdir -p "$WORKDIR"
                PREFIX="$WORKDIR/${NAME}-p1"

                ARGS=("${SIZE_ARGS[@]}" "${HEADER_ARGS[@]}" "${FOOTER_ARGS[@]}")

                echo "=== $SLUG ==="

                echo "==> Renderizando SVG (todas as páginas)"
                "$VEROVIO_BIN" -t svg -a -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "${ARGS[@]}" "$INPUT_FILE"

                shopt -s nullglob
                PAGE_SVGS=("$TMP_PREFIX"_*.svg)
                shopt -u nullglob
                if [[ ${#PAGE_SVGS[@]} -eq 0 ]]; then
                    PAGE_SVGS=("$TMP_PREFIX.svg")
                fi
                PAGE_COUNT=${#PAGE_SVGS[@]}
                # A primeira página é a mesma tanto com "-a" (sufixo "_001")
                # quanto sem (sem sufixo, quando há só 1 página).
                FIRST_SVG="${PAGE_SVGS[0]}"
                mv "$FIRST_SVG" "$PREFIX.svg"
                rm -f "$TMP_PREFIX"_*.svg "$TMP_PREFIX.svg"

                echo "==> Renderizando dotLottie (peça inteira, $PAGE_COUNT página(s))"
                "$VEROVIO_BIN" -t dotlottie -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "${ARGS[@]}" "$INPUT_FILE"
                LOTTIE_FILE="$WORKDIR/$NAME.lottie"
                mv "$TMP_PREFIX.lottie" "$LOTTIE_FILE"
                LOTTIE_SIZE=$(stat -c%s "$LOTTIE_FILE")

                echo "==> Página 1: SVG -> PNG"
                "$COMPARE_BIN" svg-to-png "$PREFIX.svg" "$PREFIX-svg.png" "${FONT_ARGS[@]}" --pin-serif-family "Liberation Serif"
                rm -f "$PREFIX.svg"

                read -r WIDTH HEIGHT < <(python3 - "$PREFIX-svg.png" <<'PYEOF'
import struct
import sys

with open(sys.argv[1], "rb") as f:
    header = f.read(24)
width, height = struct.unpack(">II", header[16:24])
print(width, height)
PYEOF
)

                # Frame de repouso da câmera pra página 1 (docs/plano/C04-paginas-virada.md) -
                # mesmo lookup de marker "page0" que compare-page.sh/compare-corpus.sh usam,
                # com fallback pra frame 0 em pacotes sem trilha horizontal (1 página só).
                FRAME=$(python3 - "$LOTTIE_FILE" <<'PYEOF'
import json
import subprocess
import sys

lottie_path = sys.argv[1]
raw = subprocess.run(["unzip", "-p", lottie_path, "a/score.json"], capture_output=True, check=True).stdout
data = json.loads(raw)
for marker in data.get("markers", []):
    if marker.get("cm") == "page0":
        print(marker["tm"])
        sys.exit(0)
print(0)
PYEOF
)
                echo "==> Página 1: Lottie -> PNG (frame $FRAME, ${WIDTH}x${HEIGHT})"
                "$COMPARE_BIN" lottie-to-png "$LOTTIE_FILE" "$PREFIX-lottie.png" --width "$WIDTH" --height "$HEIGHT" --frame "$FRAME"

                echo "==> Página 1: diff (tolerância $TOLERANCE)"
                DIFF_OUTPUT=$("$COMPARE_BIN" diff "$PREFIX-svg.png" "$PREFIX-lottie.png" "$PREFIX-diff.png" --tolerance "$TOLERANCE")
                echo "$DIFF_OUTPUT"

                PCT=$(echo "$DIFF_OUTPUT" | grep -oP '\(\K[0-9.]+(?=%\))')
                DIFF_PIXELS=$(echo "$DIFF_OUTPUT" | grep -oP 'diferentes \(tolerância [0-9]+\): \K[0-9]+')
                TOTAL_PIXELS=$(echo "$DIFF_OUTPUT" | grep -oP 'comparados: \K[0-9]+')

                echo "$SLUG,$SIZE,$ORIENTATION,$HEADER,$FOOTER,$WIDTH,$HEIGHT,$PAGE_COUNT,$PCT,$DIFF_PIXELS,$TOTAL_PIXELS,$LOTTIE_SIZE" >> "$CSV"

                rm -f "$LOTTIE_FILE"
            done
        done
    done
done

rm -f "$TMP_PREFIX".*

echo
echo "Concluído. Resultados em $CSV"
