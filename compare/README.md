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
