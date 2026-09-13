#!/usr/bin/env bash
# Simula um host: converte a saída real de `verovio -t timemap` num roteiro
# de `compare sm-render` (dispara os eventos de destaque nos instantes reais
# de onset das notas) e salva PNGs de evidência em instantes-chave — ver
# docs/plano/C05-host-simulado-timemap.md.
set -euo pipefail

if [[ $# -lt 1 || $# -gt 3 ]]; then
    echo "Uso: $0 <arquivo> [máx-eventos] [seed]" >&2
    exit 1
fi

INPUT_FILE=$1
MAX_EVENTS=${2:-12}
SEED=${3:-42}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

VEROVIO_BIN="$REPO_ROOT/verovio/tools/verovio"
COMPARE_BIN="$REPO_ROOT/compare/target/release/compare"
RESOURCE_PATH="$REPO_ROOT/verovio/data"
OUT_DIR="$REPO_ROOT/compare/out/c05"

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

BASENAME="$(basename "$INPUT_FILE")"
NAME="${BASENAME%.*}"
PIECE_OUT_DIR="$OUT_DIR/$NAME"
mkdir -p "$PIECE_OUT_DIR"

# Nome temporário sem pontos: o -o do verovio trunca tudo a partir do último
# "." do caminho (RemoveExtension em tools/main.cpp), o que corromperia nomes
# de saída para arquivos de entrada cujo nome já tem pontos.
TMP_PREFIX="$OUT_DIR/_sm-playback-tmp"

# Seed fixa nas duas chamadas (ver docs/plano/C05-host-simulado-timemap.md,
# seção "Achado"): sem -x, verovio/src/options.cpp diz explicitamente que os
# xml:id atribuídos a notas sem id explícito no MEI de origem são
# aleatórios entre execuções — precisamos que os ids do timemap (chamada 1)
# batam com os eventos da state machine do pacote dotlottie (chamada 2).
echo "==> Gerando pacote dotlottie (partitura inteira, C04, seed $SEED)"
"$VEROVIO_BIN" -t dotlottie -x "$SEED" -o "$TMP_PREFIX" --resource-path "$RESOURCE_PATH" "$INPUT_FILE"
mv "$TMP_PREFIX.lottie" "$PIECE_OUT_DIR/$NAME.lottie"

echo "==> Gerando timemap real (verovio -t timemap, seed $SEED)"
"$VEROVIO_BIN" -t timemap -x "$SEED" -o - --resource-path "$RESOURCE_PATH" "$INPUT_FILE" > "$PIECE_OUT_DIR/timemap.json"

echo "==> Derivando roteiro de compare sm-render a partir do timemap"
read -r WIDTH HEIGHT TOTAL INCLUDED <<< "$(python3 - "$PIECE_OUT_DIR/timemap.json" "$PIECE_OUT_DIR/$NAME.lottie" \
    "$MAX_EVENTS" "$PIECE_OUT_DIR/roteiro.txt" "$PIECE_OUT_DIR/script.txt" "$PIECE_OUT_DIR/snap.txt" <<'PYEOF'
import json
import subprocess
import sys

timemap_path, lottie_path, max_events, roteiro_path, script_path, snap_path = sys.argv[1:7]
max_events = int(max_events)

with open(timemap_path) as f:
    timemap = json.load(f)

# fr=30 fixo (verovio/src/lottiewriter.cpp L637); kHighlightDurationFrames=20
# é o default hardcoded hoje em Toolkit::RenderToDotLottieFile (C02) - ver
# docs/plano/C06-opcoes-cor-duracao.md se isso um dia virar configurável por
# CLI (este script não lê a opção, replica o valor à mão - dívida
# documentada em C05).
FADE_MS = round(20 / 30 * 1000)

onsets = [(e["tstamp"], e["on"][0]) for e in timemap if "on" in e]
onsets.sort(key=lambda pair: pair[0])
included = onsets[:max_events]

actions = [f"{int(round(ms))}:fire {xml_id}" for ms, xml_id in included]
snaps = {0}
for ms, _ in included:
    snaps.add(int(round(ms)))
if included:
    snaps.add(int(round(included[-1][0])) + FADE_MS)

with open(script_path, "w") as f:
    f.write(";".join(actions))
with open(snap_path, "w") as f:
    f.write(",".join(str(s) for s in sorted(snaps)))
with open(roteiro_path, "w") as f:
    f.write(f"# {len(included)} de {len(onsets)} instantes de onset reais (timemap), "
             f"fade assumido de {FADE_MS}ms\n")
    for ms, xml_id in included:
        f.write(f"t={int(round(ms))}ms fire {xml_id}\n")
    if included:
        f.write(f"t={int(round(included[-1][0])) + FADE_MS}ms (snap final, fade completo)\n")

raw = subprocess.run(["unzip", "-p", lottie_path, "a/score.json"], capture_output=True, check=True).stdout
data = json.loads(raw)
print(data["w"], data["h"], len(onsets), len(included))
PYEOF
)"

echo "==> $INCLUDED de $TOTAL instantes de onset reais incluídos (roteiro em $PIECE_OUT_DIR/roteiro.txt)"

echo "==> compare sm-render (sm_highlight, ${WIDTH}x${HEIGHT})"
"$COMPARE_BIN" sm-render "$PIECE_OUT_DIR/$NAME.lottie" "$PIECE_OUT_DIR" \
    --sm sm_highlight --width "$WIDTH" --height "$HEIGHT" \
    --script "$(cat "$PIECE_OUT_DIR/script.txt")" --snap "$(cat "$PIECE_OUT_DIR/snap.txt")" \
    --prefix playback
