# compare

Ferramenta de linha de comando para validar visualmente a exportação dotLottie
do `verovio_lottie` contra a saída SVG já existente do Verovio (ver
`docs/descricao-do-projeto.md` na raiz do repositório para o contexto do
projeto).

Stack: [resvg](https://github.com/linebender/resvg) para renderizar SVG em
PNG, e [dotlottie-rs](https://github.com/LottieFiles/dotlottie-rs) (runtime
oficial da LottieFiles, o mesmo que implementa a State Machine v2 do
dotLottie) para renderizar Lottie/dotLottie em PNG via software rendering
(ThorVG), sem precisar de navegador headless.

## Build

```sh
cd compare
cargo build --release
```

A primeira build compila o ThorVG (C++ vendorizado dentro de `dotlottie-rs`)
a partir do código-fonte — leva mais tempo que uma build Rust pura. Builds
seguintes são incrementais.

## Uso

### 1. Renderizar o SVG do Verovio em PNG

```sh
verovio -f mei -t svg partitura.mei -o partitura.svg --resource-path <verovio>/data
compare svg-to-png partitura.svg partitura-svg.png
```

### 2. Renderizar um frame de um Lottie/dotLottie em PNG

```sh
compare lottie-to-png animacao.lottie animacao.png --width 2100 --height 2970 --frame 0
```

Aceita `.lottie` (pacote dotLottie) ou `.json` (Lottie puro). `--width`/
`--height` devem bater com o tamanho usado na comparação (ex.: o mesmo
`width`/`height` do SVG gerado pelo Verovio).

### 3. Comparar dois PNGs

```sh
compare diff partitura-svg.png animacao.png diff.png
```

Gera `diff.png` (fundo em tons de cinza esmaecidos + pixels divergentes em
vermelho) e imprime no terminal quantos pixels diferem e a maior diferença de
canal observada. `--tolerance N` (0-255) permite ignorar diferenças pequenas
de antialiasing.

### 4. Script: comparar uma página inteira de uma vez

```sh
compare/scripts/compare-page.sh <arquivo> <página> [tolerância]
```

Funciona a partir de qualquer diretório (resolve a raiz do repositório pela
própria localização do script). Faz os passos 1-3 acima de uma vez só —
`verovio -t svg`, `verovio -t lottie`, `svg-to-png` (com as fontes do
Verovio via `--font`, ver nota abaixo), lê a resolução do PNG do SVG para
passar a `lottie-to-png`, e roda `diff` — e grava tudo em `compare/out/`
(ignorado pelo git):

```
compare/out/<nome>-p<N>.svg
compare/out/<nome>-p<N>.json
compare/out/<nome>-p<N>-svg.png
compare/out/<nome>-p<N>-lottie.png
compare/out/<nome>-p<N>-diff.png
```

Exemplo: `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`.

**Nota sobre `svg-to-png --font`**: a opção existe e funciona (`compare
svg-to-png --font <arquivo.ttf/otf>` chama
`fontdb_mut().load_font_file(...)`), mas, verificado no corpus inteiro, o SVG
gerado pelo Verovio **não** usa `@font-face`/texto com `font-family` de fonte
musical — os glifos SMuFL (dinâmicas, articulações etc.) sempre saem como
`<use xlink:href="#...">` referenciando `<path>` vetorial em `<defs>`, nunca
como `<text font-family="Leipzig">`. O único `font-family` que aparece é
`Times, serif`, para texto comum (títulos, indicações, letra), que as fontes
do sistema já cobrem. Ou seja: carregar as fontes do Verovio não muda nada na
renderização do corpus atual — a opção fica disponível por segurança (caso
algum MEI produza texto solto com fonte SMuFL), mas não é necessária hoje.

## Limitações atuais / decisões conhecidas

- **Ainda não há exportador dotLottie no Verovio** — o subcomando
  `lottie-to-png` já funciona contra qualquer `.lottie`/`.json` válido (testado
  com fixtures do próprio `dotlottie-rs`), mas o fluxo real "Verovio → .lottie
  → PNG" só fecha quando o exportador (`verovio/src`, futuro `IoDotLottie`)
  existir.
- **Critério de comparação é visual/manual** — `diff` dá um número e uma
  imagem para inspeção humana; não há um limiar de "passou/falhou"
  automático definido ainda (ver `docs/descricao-do-projeto.md`).

## Pegadinha do dotlottie-rs: `set_frame`/`render` podem "falhar" sem problema

`Player::load_animation_data`/`load_dotlottie_data` já renderizam o frame
inicial internamente durante o load (ThorVG/dotlottie-rs ignora esse
resultado de propósito — ver `Player::load_animation_common` no código-fonte
da lib). Por causa disso:

- Chamar `set_frame(n)` quando `n` já é o frame corrente é tratado como
  no-op pelo ThorVG e retorna erro (`"unknown error"`, que na real é
  `Result::InsufficientCondition` do lado do ThorVG, mas point remonta para
  `Error::Unknown` no dotlottie-rs por causa de um `match` que não distingue
  esse caso).
- Chamar `render()` sem nada ter mudado desde o último render (`updated ==
  false` internamente) também retorna erro pelo mesmo motivo.

Isso é **inofensivo**: nos dois casos o buffer de pixels já está correto. Por
isso `lottie_to_png` (`src/main.rs`) só *avisa* (stderr) quando essas duas
chamadas retornam erro, em vez de abortar — replicando o padrão usado no
próprio exemplo oficial da lib (`examples/simple_player.rs`, que usa `let _ =
player.set_frame(...)`). Se você mexer nesse código, não troque os avisos por
`?`/`.unwrap()` sem reler esta seção.

## Pegadinha do `resvg` (resolvida em D01-3): texto comum centralizado/à
## direita com `<title>` aninhado media a largura errado (achado em D01)

Descoberta ao validar `docs/plano/D01-texto-comum.md` (texto comum embutido
no Lottie): o SVG do Verovio marca elementos com `@label` (títulos de
página, nome do compositor, etc.) assim —
`<tspan x=".." text-anchor="middle|end"><title class="labelAttr">rótulo</title>
<tspan>...texto de verdade...</tspan></tspan>` — e o `resvg` (via
`svg-to-png`), ao medir a largura do texto para aplicar o `text-anchor`,
**incluía erroneamente o `<title>` aninhado na medição**, produzindo uma
largura muito maior que a real e jogando a maior parte do texto pra fora da
página (cortado à esquerda, no caso de `middle`/`end`). Não era um bug de
posicionamento do exportador dotLottie: o `.lottie` gerado sempre esteve
corretamente centralizado/alinhado à direita na mesma coordenada `x` que o
próprio SVG declara; era o PNG de *referência* que saía errado para esse
elemento específico.

Efeito prático (antes da correção): qualquer `compare diff`/
`compare-page.sh`/`compare-corpus.sh` envolvendo texto comum centralizado
ou alinhado à direita com `@label` mostrava uma divergência grande e
enganosa ali, mesmo quando o Lottie estava visualmente correto — teve
impacto mensurável real no corpus completo em D01 (ver "Notas de execução"
daquele passo).

**Resolvido em D01-3**
(`docs/plano/D01-3-titulo-aninhado-resvg.md`) removendo todo nó `<title>`
do SVG (via `roxmltree`, por range de bytes) **antes** de
`usvg::Tree::from_data` — `<title>` nunca é desenhado por nenhum
renderizador conforme a spec, então a remoção não muda nada visualmente,
só corrige a medição de largura do `resvg`. Sempre ativo em
`svg-to-png`, sem flag nova. Efeito medido: corpus caiu de
0,1002%–0,6646%/média 0,3154% (D01-2) para 0,1002%–0,5997%/média 0,2977%
— melhoria concentrada exatamente nas páginas com `@label` no cabeçalho
(p.1 das 5 peças de `corpus/mei`; as de `corpus/musicxml` não têm `@label`
no cabeçalho e ficaram byte a byte iguais, sem regressão em nenhuma
página).

## Pegadinha adicional (resolvida em D01-2): `--font` sozinho não força o
## `resvg` a trocar a fonte de "Times, serif"

Descoberto em D01: `fc-match "Times, serif"` neste ambiente resolve para
**Nimbus Roman**, não para a Liberation Serif que o exportador dotLottie
embute de verdade no pacote (T1, `docs/plano/decisoes/B03-texto.md`). Tentar
`compare svg-to-png --font <LiberationSerif-*.ttf>` para igualar as fontes
**não tinha efeito nenhum** no PNG gerado (byte a byte idêntico com e sem a
flag) — o fontconfig/`resvg` deste ambiente preferia a Nimbus Roman já
registrada no sistema para a família genérica "Times, serif", independente
de quais arquivos eram carregados via `--font`.

**Resolvido em D01-2** (`docs/plano/D01-2-controle-de-fonte-na-comparacao.md`)
com a flag `--pin-serif-family <NOME>`, que chama `fontdb.set_serif_family()`
— troca pra qual família o genérico CSS `serif` resolve, independente do que
o SO tem instalado. `compare-page.sh`/`compare-corpus.sh` já carregam os 3
`.ttf` de D01 via `--font` e passam `--pin-serif-family "Liberation Serif"`
na chamada de `svg-to-png` que renderiza a partir de uma partitura. Efeito
confirmado (PNG muda 0,4977% dos pixels a tolerância 0 com a flag, contra
zero sem ela) e visualmente (negrito/itálico saem na face certa da
Liberation Serif de verdade, não mais Nimbus Roman). Isso por si só ainda
não elimina toda divergência de texto comum — resta a pegadinha de
`<title>` aninhado acima, ortogonal à fonte — mas isola o efeito de "fonte
fisicamente diferente" da conta.
