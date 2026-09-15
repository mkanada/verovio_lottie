# ThorVG local (verovio_lottie)

Cópia vendorizada do [ThorVG](https://github.com/thorvg/thorvg) com correções
próprias deste projeto. É o motor que renderiza os `.lottie` em todo o
tooling do repositório:

- `compare/` compila este código via [`dotlottie-rs/`](../dotlottie-rs/VEROVIO_LOTTIE.md)
  (cópia local do dotlottie-rs, cujo `deps/thorvg` é um symlink pra cá).
- `thorvg-cli/build.sh` compila este código via meson (build fora da árvore,
  em `thorvg-cli/build/`), com o loader de SVG ligado.

**Atenção:** os players oficiais de dotLottie (que usam o dotlottie-rs
original) **não** têm as correções abaixo. Um host que não compile este
ThorVG continua com os problemas descritos aqui.

## Origem

- Commit `019d18c7592ad6370059f647512b00be5f5f868e` ("lottie: let the media
  backend own the seek policy") do fork `https://github.com/theashraf/thorvg`
  (branch `dotlottie/video-20260812`) — exatamente o submódulo
  `dotlottie-rs/deps/thorvg` do dotlottie-rs `eb44c991` que o `compare/`
  usava antes desta cópia.
- Copiados: `src/`, `inc/`, `tools/` (o `meson.build` raiz faz
  `subdir('tools')` incondicionalmente), `meson.build`, `meson_options.txt`,
  `LICENSE`, `README.md`, `CONTRIBUTORS.md`.
- Omitidos (nenhum build deste repositório usa): `test/`, `cross/`, `.github/`,
  `.git`, arquivos de formatação/CI.
- Licença MIT (ver `LICENSE`).

## Modificações locais

Toda linha alterada leva o marcador `verovio_lottie` num comentário:
`grep -rn verovio_lottie thorvg/` lista todas.

### 1. Itálico sintético aplicado por cima de fonte já itálica

Detalhes, evidência e medições em
[`docs/plano/D01-4-italico-sintetico-thorvg.md`](../docs/plano/D01-4-italico-sintetico-thorvg.md).

- **Sintoma:** texto comum em itálico de um `.lottie` (camada `ty:5`) sai
  inclinado demais e deslocado, com as letras "grudadas" umas nas outras;
  texto reto e negrito saem certos.
- **Causa:** o loader de Lottie chamava `Text::italic()` (cisalhamento de
  0,18 ≈ 10°) sempre que `fonts.list[].fStyle` contém `"Italic"`, sem
  verificar se a face carregada já é itálica. Com
  `LiberationSerif-Italic.ttf` embutida (inclinação própria de 16,3°), a
  inclinação era aplicada duas vezes, mais um deslocamento horizontal
  proporcional à largura do primeiro glifo (`SfntLoader::transform`).
- **Correção:**
  - `src/loaders/sfnt/tvgSfntReader.{h,cpp}`: `SfntReader::header()` preenche
    `metrics.italic` a partir de `head.macStyle` (bit 1) e
    `OS/2.fsSelection` (bit 0 ITALIC, bit 9 OBLIQUE).
  - `src/renderer/tvgLoader.h`: `FontLoader::italic()` (padrão `false`),
    implementado por `SfntLoader` em `src/loaders/sfnt/tvgSfntLoader.h`.
  - `src/loaders/lottie/tvgLottieBuilder.cpp`: o itálico sintético só é
    aplicado se a face resolvida **não** for itálica por desenho. Fonte
    regular com `fStyle` "Italic" continua recebendo o cisalhamento.
  - API pública (`inc/thorvg.h`) intacta: `Text::italic()` chamado
    diretamente continua cisalhando qualquer fonte.
- **Upstream:** não enviado. O `main` de `thorvg/thorvg` em 2026-09-15
  (`b07c83fe`) ainda tem a lógica antiga.

## Atualizando o ThorVG

1. Trocar `src/`, `inc/`, `tools/` e os arquivos da raiz pelos de um commit
   novo — de preferência o mesmo submódulo do dotlottie-rs para o qual
   `dotlottie-rs/` for atualizado (ver `dotlottie-rs/VEROVIO_LOTTIE.md`).
2. Para cada item de "Modificações locais": conferir se o upstream já
   resolveu; se não, reaplicar.
3. Recompilar `compare/` e `thorvg-cli/` e repetir a verificação de
   `docs/plano/D01-4-italico-sintetico-thorvg.md`.
