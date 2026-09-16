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
COMPARE_BIN="$REPO_ROOT/compare/build/linux/x64/release/bundle/compare"
SVG_RENDER_BIN="$REPO_ROOT/compare/svg_render/target/release/svg_render"
RESOURCE_PATH="$REPO_ROOT/verovio/data"
OUT_DIR="$REPO_ROOT/compare/out"

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

if [[ ! -f "$INPUT_FILE" ]]; then
    echo "Arquivo de entrada não encontrado: $INPUT_FILE" >&2
    exit 1
fi

mkdir -p "$OUT_DIR"

BASENAME="$(basename "$INPUT_FILE")"
NAME="${BASENAME%.*}"
EXT="${BASENAME##*.}"
PREFIX="$OUT_DIR/${NAME}-p${PAGE}"

if [[ "$EXT" == "lottie" ]]; then
    # O arquivo já é um pacote dotLottie pronto (ex.: gerado com `-t dotlottie`
    # para a música inteira). Não há partitura de origem aqui, então não dá
    # pra (re)gerar o SVG — reaproveita o PNG do SVG já gerado por uma
    # execução anterior deste script sobre o arquivo de partitura original
    # (mesmo prefixo, já que NAME ignora a extensão) e só extrai a página
    # pedida do pacote.
    if [[ ! -f "$PREFIX-svg.png" ]]; then
        echo "PNG do SVG não encontrado em $PREFIX-svg.png." >&2
        echo "Rode antes: $0 <arquivo-de-partitura-original> $PAGE [tolerância]" >&2
        exit 1
    fi

    echo "==> Lendo dimensões do PNG do SVG existente"
    read -r WIDTH HEIGHT < <(python3 - "$PREFIX-svg.png" <<'PYEOF'
import struct
import sys

with open(sys.argv[1], "rb") as f:
    header = f.read(24)
width, height = struct.unpack(">II", header[16:24])
print(width, height)
PYEOF
)

    # Frame de repouso da câmera pra página $PAGE (docs/plano/C04-paginas-virada.md):
    # pacotes multi-página gerados a partir de C04 têm um marker "page<N-1>" em
    # a/score.json indicando o frame em que a câmera fica parada exatamente na
    # página N (a trilha horizontal + câmera substituiu a suposição antiga
    # "frame == página - 1"). Pacotes sem essa trilha (dotlottie-highlight, ou
    # um .lottie de antes de C04) não têm esse marker — nesse caso volta pra
    # suposição antiga.
    FRAME=$(python3 - "$INPUT_FILE" "$PAGE" <<'PYEOF'
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
    echo "==> Lottie -> PNG (pacote dotLottie existente, frame $FRAME, ${WIDTH}x${HEIGHT})"
    "${COMPARE_RUN[@]}" lottie-to-png "$INPUT_FILE" "$PREFIX-lottie.png" --width "$WIDTH" --height "$HEIGHT" --frame "$FRAME"

    echo "==> Diff (tolerância $TOLERANCE)"
    "${COMPARE_RUN[@]}" diff "$PREFIX-svg.png" "$PREFIX-lottie.png" "$PREFIX-diff.png" --tolerance "$TOLERANCE"

    exit 0
fi

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
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Regular.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Italic.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-Bold.ttf"
    "$REPO_ROOT/verovio/data/text/LiberationSerif-BoldItalic.ttf"
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
"$SVG_RENDER_BIN" "$PREFIX.svg" "$PREFIX-svg.png" "${FONT_ARGS[@]}" --pin-serif-family "Liberation Serif"

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
"${COMPARE_RUN[@]}" lottie-to-png "$PREFIX.json" "$PREFIX-lottie.png" --width "$WIDTH" --height "$HEIGHT"

echo "==> Diff (tolerância $TOLERANCE)"
"${COMPARE_RUN[@]}" diff "$PREFIX-svg.png" "$PREFIX-lottie.png" "$PREFIX-diff.png" --tolerance "$TOLERANCE"
