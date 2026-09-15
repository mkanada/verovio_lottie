# thorvg-cli

Ferramenta de linha de comando (`thorvg-render`) que renderiza tanto o SVG
quanto o `.lottie` gerados pelo `verovio_lottie` **através do mesmo motor**,
o [ThorVG local](../thorvg/VEROVIO_LOTTIE.md) do repositório, compilado com o
loader de SVG ligado. O `compare/` compila esse mesmo código via
`dotlottie-rs/`, mas só com o loader de Lottie (o `dotlottie-rs` nunca precisa
carregar SVG). Objetivo: comparar como o ThorVG lida com o mesmo conteúdo
(texto comum, notação) vindo dos dois formatos de saída do Verovio.

## Build

```sh
pip install --user meson   # ninja já costuma estar disponível
thorvg-cli/build.sh
```

`build.sh` configura via `meson` um build do `thorvg/` da raiz **fora da
árvore de código** (em `thorvg-cli/build/`, git-ignorado), com os loaders
`svg,lottie,ttf,otf,png` habilitados (o build do `compare/` só tem
`tvg,tvg-cpu,tvg-ttf,tvg-otf,tvg-png` — sem `svg`). Depois builda com `ninja`
e compila `src/render.cpp` contra o `libthorvg-1.so` resultante. Binário final
em `thorvg-cli/bin/thorvg-render`.

Como o código-fonte é o mesmo do `compare/`, as correções locais do ThorVG
valem para os dois. Renderizar o mesmo `.lottie` pelas duas ferramentas dá o
mesmo PNG (Chopin Étude p.1: 0 pixels diferentes a tolerância 32).

## Uso

```sh
# SVG (mesmo arquivo que `compare svg-to-png` usaria, mas via ThorVG em vez
# de resvg) - --font é necessário pelo mesmo motivo do --font do compare
# (as fontes do projeto não vêm instaladas no sistema, ver compare/README.md)
thorvg-cli/bin/thorvg-render svg partitura.svg saida.png --width 2100 --height 2970 \
  --font verovio/data/text/LiberationSerif-Regular.ttf \
  --font verovio/data/text/LiberationSerif-Italic.ttf \
  --font verovio/data/text/LiberationSerif-Bold.ttf

# .lottie (extrai a página/animação principal e as fontes embutidas do zip
# sozinho, via `unzip` - mesmo mecanismo usado em compare/scripts/*.sh)
thorvg-cli/bin/thorvg-render lottie partitura.lottie saida.png --width 2100 --height 2970 --frame 0
```

Fundo sempre branco opaco no PNG de saída (mesma convenção de
`compare svg-to-png`/`lottie-to-png`, ver `docs/matriz-layout/README.md`).
Pra medir a diferença pixel a pixel entre uma saída deste tool e uma do
`compare`, reuse `compare diff` normalmente - o `diff` não liga pra como o
PNG foi gerado.

## Por que isto existe

Criado para investigar uma divergência visual encontrada ao inspecionar
`docs/matriz-layout/` a olho nu: texto comum em itálico (ex. "cresc.", "con
forza", "legatissimo") saía com **letras grudadas/sobrepostas** no `.lottie`
renderizado (via `compare lottie-to-png`, que usa `dotlottie-rs`/ThorVG),
enquanto o mesmo texto no SVG de referência (via `compare svg-to-png`, que
usa `resvg`) saía com o espaçamento normal. Medido de forma robusta (perfil
de tinta por coluna de pixel, em vários limiares): texto reto (ex. o título)
saía **pixel-idêntico** entre os dois; só o itálico divergia.

Hipótese avaliada na época: os loaders de SVG e de Lottie do ThorVG
compartilham a mesma classe `Text` (`tvgSvgBuilder.cpp` e
`tvgLottieBuilder.cpp` ambos chamam `Text::gen()`). Se o "grudamento" também
aparecesse renderizando o SVG pelo próprio ThorVG, seria uma característica
do motor de texto em si, e não algo específico de como o exportador monta o
layer `ty:5`.

**Resolvido em D01-4** (`docs/plano/D01-4-italico-sintetico-thorvg.md`): a
causa não era o motor de texto compartilhado nem o exportador. Era uma
heurística **só do loader de Lottie**, que aplicava itálico sintético por
cima de uma fonte que já é itálica. Ver "Achados".

## Achados

- **O SVG do Verovio aninha um `<svg class="definition-scale"
  viewBox="0 0 19200 10800">` dentro do `<svg>` raiz** (a forma como o
  Verovio escala as "unidades de definição" - `DEFINITION_FACTOR=10`, ver
  `docs/plano/README.md` "Unidades" - pro tamanho final de página em px). O
  loader de SVG do ThorVG **não suporta `<svg>` aninhado** ("Nested `<svg>`
  element is not supported") e **dropa todo o conteúdo em silêncio** (sem
  erro - o PNG de saída simplesmente sai 100% branco). `render.cpp`
  contorna isso substituindo o `<svg>` aninhado por um `<g
  transform="scale(...)">` matematicamente equivalente antes de carregar
  (`FlattenNestedSvg`), preservando os demais atributos da tag.
- **O loader de SVG do ThorVG não aplica `font-style: italic`** - nem como
  atributo de apresentação (`font-style="italic"` direto no elemento) nem
  via a regra de CSS por classe que o Verovio usa de verdade (`g.dir,
  g.dynam, g.mNum {font-style:italic;}`, injetada num `<style>` interno -
  ThorVG loga "Unsupported elements used in the internal CSS style sheets"
  pra essas regras). Confirmado visualmente: renderizando o SVG real via
  este tool, "cresc." e "con forza" saem **retos** (Liberation Serif
  Regular), não itálicos. Continua valendo com o ThorVG local (a correção de
  D01-4 não mexe no loader de SVG).
- **Consequência**: a comparação planejada (SVG-via-ThorVG vs.
  Lottie-via-ThorVG, ambos em itálico) não foi possível por este caminho, já
  que o loader de SVG do ThorVG não desenha itálico nenhum para comparar.
- **Causa do grudamento (achada lendo o loader de Lottie, D01-4):**
  `tvgLottieBuilder.cpp` chamava `Text::italic()` (cisalhamento de 0,18 ≈
  10°) sempre que o `fStyle` da fonte contém `"Italic"`, sem checar se a
  face carregada já é itálica. A `LiberationSerif-Italic.ttf` embutida já
  tem 16,3° de inclinação, então o texto saía inclinado duas vezes e ainda
  deslocado na horizontal (`SfntLoader::transform` soma
  `cisalhamento × largura do primeiro glifo`). As pistas de que o exportador
  estava certo se confirmaram: `run.origin` usa o mesmo `x`/`y` do SVG, a
  fonte no JSON é a certa (`"f":"LiberationSerif-Italic"`), e texto reto sai
  idêntico. Mas a conclusão de que o grudamento era "uma característica do
  motor de texto do ThorVG em si" **estava errada**: o motor desenha a face
  itálica corretamente, e o erro era o cisalhamento extra pedido pelo
  loader de Lottie. Corrigido no `thorvg/` local, e o `.lottie` passa a bater
  com o SVG (Chopin Étude p.1: 0,4520% → 0,0465%).

## Arquivos

- `build.sh` - configura (`meson`) e builda (`ninja`) o `thorvg/` da raiz em
  `thorvg-cli/build/`, depois compila `src/render.cpp` contra ele.
- `src/render.cpp` - a ferramenta `thorvg-render` em si (dois modos: `svg` e
  `lottie`).
