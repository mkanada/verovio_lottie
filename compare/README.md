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

## Pegadinha do `resvg`: texto comum centralizado/à direita com `<title>`
## aninhado mede a largura errado (achado em D01)

Descoberta ao validar `docs/plano/D01-texto-comum.md` (texto comum embutido
no Lottie): o SVG do Verovio marca elementos com `@label` (títulos de
página, nome do compositor, etc.) assim —
`<tspan x=".." text-anchor="middle|end"><title class="labelAttr">rótulo</title>
<tspan>...texto de verdade...</tspan></tspan>` — e o `resvg` (via
`svg-to-png`), ao medir a largura do texto para aplicar o `text-anchor`,
**inclui erroneamente o `<title>` aninhado na medição**, produzindo uma
largura muito maior que a real e jogando a maior parte do texto pra fora da
página (cortado à esquerda, no caso de `middle`/`end`). Reproduzido de forma
isolada e mínima (com e sem o `<title>` aninhado, mesmo `text-anchor`) —
não é um bug de posicionamento do exportador dotLottie: o `.lottie` gerado
está corretamente centralizado/alinhado à direita na mesma coordenada `x`
que o próprio SVG declara; é o PNG de *referência* que sai errado para
esse elemento específico.

Efeito prático: qualquer `compare diff`/`compare-page.sh`/`compare-corpus.sh`
envolvendo texto comum centralizado ou alinhado à direita com `@label`
(basicamente todo título de página e nome de compositor do corpus) vai
mostrar uma divergência grande e enganosa ali, mesmo quando o Lottie está
visualmente correto — teve impacto mensurável real no corpus completo em
D01 (ver "Notas de execução" daquele passo). Ainda **sem workaround**
implementado aqui; se for preciso medir esse texto com precisão no futuro,
os caminhos mais óbvios são (a) contornar o bug pré-processando o SVG antes
do `svg-to-png` (remover o `<title>` aninhado desses `tspan`s), ou
(b) investigar/reportar o bug no próprio `resvg`.

## Pegadinha adicional: `--font` não força o `resvg` a trocar a fonte de
## "Times, serif" (achado em D01)

Também descoberto em D01: `fc-match "Times, serif"` neste ambiente resolve
para **Nimbus Roman**, não para a Liberation Serif que o exportador dotLottie
embute de verdade no pacote (T1, `docs/plano/decisoes/B03-texto.md`). Tentar
`compare svg-to-png --font <LiberationSerif-*.ttf>` para igualar as fontes
**não teve efeito nenhum** no PNG gerado (byte a byte idêntico com e sem a
flag) — o fontconfig/`resvg` deste ambiente continua preferindo a Nimbus
Roman já registrada no sistema para a família genérica "Times, serif",
independente de quais arquivos são carregados via `--font`. Consequência:
mesmo com posição/tamanho/estilo perfeitos, texto comum vai sempre comparar
contornos de **fontes fisicamente diferentes** entre SVG e Lottie (ao
contrário de glifos SMuFL, que usam a mesma geometria "assada" nos dois
lados) — isso por si só já produz uma faixa de divergência de pixels maior
que ruído simples de antialiasing. Não investigado mais a fundo (precisaria
mexer na resolução de fontes do `resvg`/numa config de fontconfig isolada
para este binário `compare`).
