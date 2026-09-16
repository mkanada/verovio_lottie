#!/usr/bin/env bash
# Regenera assets/verovio_data.zip a partir de ../verovio/data (as fontes
# SMuFL — Bravura, Leipzig, Gootville, Petaluma, Leland — mais os TTFs
# Liberation usados no texto comum). Empacotado como um único zip (em vez
# de arquivos crus) porque o bundler de assets do Flutter lista diretórios
# sem recursão: uma pasta declarada em pubspec.yaml só traz os arquivos do
# topo, descartando silenciosamente as subpastas por glifo. Um arquivo
# único não tem esse problema; verovio_viewer descompacta em runtime (ver
# lib/src/resources.dart).
#
# Não versionado (ver .gitignore) — regenerável e derivado de ../verovio/data,
# que já está vendorizado no repositório. Rode após qualquer atualização de
# ../verovio/data.
set -euo pipefail

pkg_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
src="$pkg_dir/../verovio/data"
dst="$pkg_dir/assets/verovio_data.zip"

[[ -d "$src" ]] || { echo "ERROR: diretório não encontrado: $src" >&2; exit 1; }

mkdir -p "$(dirname "$dst")"
rm -f "$dst"
(cd "$src" && zip -rq -X "$dst" .)
echo "Gerado $dst ($(du -sh "$dst" | cut -f1), a partir de $src)"
