# compare (Flutter)

Ferramenta de linha de comando para validar visualmente a exportação
dotLottie do `verovio_lottie` contra a saída SVG já existente do Verovio (ver
`docs/descricao-do-projeto.md` na raiz do repositório para o contexto do
projeto).

Stack: [flutter_svg](https://pub.dev/packages/flutter_svg) (via
[Impeller](https://docs.flutter.dev/perf/impeller)) para renderizar SVG em
PNG, e o [dotlottie_flutter](https://pub.dev/packages/dotlottie_flutter)
(mesmo motor `dotlottie-rs`/ThorVG que o widget `DotLottieView` usa no
Linux, acessado aqui via FFI direto) para renderizar Lottie/dotLottie em PNG
por software, sem precisar de navegador headless. O diff é pixel a pixel, com
imagem de diferença e estatísticas — a decisão de "passou/falhou" continua
sendo visual/manual, sem limiar automático definido ainda (ver
`docs/descricao-do-projeto.md`).

Esta ferramenta substitui a versão anterior em Rust (`resvg` +
`dotlottie-rs` vendorizado). A superfície de CLI é a mesma, de propósito:
os scripts em `scripts/` continuam funcionando trocando só o caminho do
binário.

## Build

```sh
cd compare
flutter build linux --release
```

O binário sai em `compare/build/linux/x64/release/bundle/compare`
(git-ignorado). Os scripts em `scripts/` já apontam para lá.

## Execução: modo batch sob xvfb

O binário é um app Flutter/Linux: mesmo sem abrir janela útil, o embedder
exige um display para inicializar o motor. Em máquina sem display, rode sob
`xvfb-run`:

```sh
xvfb-run -a compare/build/linux/x64/release/bundle/compare svg-to-png ...
```

Os scripts em `scripts/` fazem isso sozinhos quando `DISPLAY` está vazio (e
`xvfb-run` existe). Logs do motor na inicialização (`Impeller rendering
backend`, `Gdk ... cursor theme`) vão para o stderr e são ruído inofensivo —
o resultado dos comandos sai no stdout como antes.

## Uso

Os exemplos abaixo assumem `COMPARE=compare/build/linux/x64/release/bundle/compare`
(e `xvfb-run -a` na frente se necessário) e partem da raiz do repositório.

### 1. Renderizar o SVG do Verovio em PNG

```sh
verovio -f mei -t svg partitura.mei -o partitura.svg --resource-path <verovio>/data
$COMPARE svg-to-png partitura.svg partitura-svg.png \
  --font verovio/data/text/LiberationSerif-Regular.ttf \
  --pin-serif-family "Liberation Serif"
```

Renderiza com o `flutter_svg`/Impeller sobre fundo branco opaco, no tamanho
declarado em `width`/`height` do `<svg>` raiz. Antes de desenhar, o SVG passa
por três normalizações (ver "Pré-processamento do SVG" abaixo): achatar o
`<svg>` aninhado do Verovio, realocar os textos para fora da escala e remover
todo `<title>`.

- `--font <arquivo>` (repetível, compatibilidade de CLI): só verifica se o
  arquivo existe (avisa se não). As fontes que importam para a comparação
  (Liberation Serif, a mesma que o `.lottie` embute) já vão empacotadas no
  app (ver `assets/fonts/`); os glifos SMuFL da notação são `<path>` em
  `<defs>`, nunca texto — carregar Leipzig/Bravura não muda nada, como
  na versão anterior.
- `--pin-serif-family <nome>`: troca o valor inteiro de
  `font-family="Times, serif"` pelo nome informado, para que o genérico CSS
  `serif` do Verovio resolva na fonte empacotada em vez de uma substituta do
  sistema. Detalhe de implementação: o valor precisa ser o nome puro da
  família (sem `, serif`), porque o `flutter_svg` repassa a string ao motor,
  que só resolve nomes exatos.

### 2. Renderizar um frame de um Lottie/dotLottie em PNG

```sh
$COMPARE lottie-to-png animacao.lottie animacao.png --width 2100 --height 2970 --frame 0
```

Aceita `.lottie` (pacote dotLottie, com fontes embutidas e state machines)
ou `.json` (Lottie puro). `--width`/`--height` devem bater com o tamanho
usado na comparação (ex.: o mesmo `width`/`height` do SVG gerado pelo
Verovio). Compõe o resultado sobre fundo branco opaco.

- `--frame <n>`: frame a renderizar. Num pacote `-t dotlottie` com várias
  páginas, a página N fica em repouso no frame do marker `page<N-1>` de
  `a/score.json` (câmera de C04), não no frame N-1.
- `--slot <id>:r,g,b` (repetível, 0-1 cada): sobrescreve um slot de cor antes
  de renderizar (slots por `xml:id` do modo interativo, C03). Implementado
  via `dotlottie_set_color_slot` do `.so` empacotado (o widget
  `DotLottieView` não expõe slots no desktop — ver "Arquitetura" abaixo).
- `--sample <x>,<y>` (repetível): imprime o RGBA desse pixel depois de
  renderizar (lido do buffer nativo, antes da composição com o branco).

### 3. Comparar dois PNGs

```sh
$COMPARE diff partitura-svg.png animacao.png diff.png
```

Gera `diff.png` (fundo em tons de cinza esmaecidos + pixels divergentes em
vermelho) e imprime no terminal quantos pixels diferem e a maior diferença de
canal observada. `--tolerance N` (0-255) permite ignorar diferenças pequenas
de antialiasing. Formato de saída idêntico ao da versão anterior (os scripts
extraem os números com `grep`).

### 4. Script: comparar uma página inteira de uma vez

```sh
compare/scripts/compare-page.sh <arquivo> <página> [tolerância]
```

Igual à versão anterior (resolve a raiz do repositório pela própria
localização do script; grava tudo em `compare/out/`, ignorado pelo git).
Para o pacote de produção (`-t dotlottie`), o script lê o frame de repouso
da página dos markers `page<N-1>` e reaproveita o PNG do SVG, como antes.

Saídas:

```
compare/out/<nome>-p<N>.svg
compare/out/<nome>-p<N>.json
compare/out/<nome>-p<N>-svg.png
compare/out/<nome>-p<N>-lottie.png
compare/out/<nome>-p<N>-diff.png
```

Exemplo: `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`.

### 5. Simular um host: `sm-render`

```sh
$COMPARE sm-render pacote.lottie saida/ --sm sm_highlight --width 2100 --height 2970 \
  --script "0:fire d1e134;2750:fire d1e252" --snap "0,2750,3417" --prefix playback
```

Carrega a animação e uma state machine do pacote (`--sm <id>`, ou
`--sm-file <json>` para uma state machine avulsa), executa o roteiro no tempo
e salva `<prefixo>-t<ms>.png` em `saida/` para cada instante de `--snap`.
Semântica idêntica à da versão anterior: relógio manual de 1ms
(`state_machine_tick`), ações do instante `t` rodando **antes** do snapshot
desse instante, flush antes de cada snapshot, `state_machine_load` +
`start()` medidos com `--measure-load`.

- `--script "ms:ação;ms:ação"`: `fire <evento>` (state machine),
  `slot <id:r,g,b>`, `clearslot <id>` e `clearslots` (slots interativos,
  C03). Falhas de ação individual só avisam (stderr), sem abortar.
- `--sample <x>,<y>` (repetível): imprime o RGBA do pixel a cada snapshot.
- `--measure-load`: só carrega a state machine e imprime o tempo de carga.

### 6. Varreduras e roteiros prontos

| Script | O que faz | Saída |
| --- | --- | --- |
| `compare/scripts/compare-corpus.sh [tolerância]` | Corpus inteiro (`corpus/mei` + `corpus/musicxml`): SVG de cada página, um pacote `-t dotlottie` por peça, e PNGs + diff por página (frame de repouso de cada página lido dos markers) | `compare/out/corpus/<peça>/`, `resultado.csv`, `tamanhos.txt` |
| `compare/scripts/compare-layout-matrix.sh [arquivo] [tolerância]` | 16 combinações de tamanho, orientação, cabeçalho e rodapé para uma peça, página 1 | `docs/matriz-layout/` (versionado, ver o README de lá) |
| `compare/scripts/sm-playback.sh <arquivo> [máx-eventos] [seed]` | Gera pacote e timemap com a mesma seed e dispara o destaque nos onsets reais do timemap via `sm-render` (C05) | `compare/out/c05/<peça>/` |

`compare/out/` é ignorado pelo git.

## Arquitetura: por que FFI direto em vez do widget `DotLottieView`?

O lado Lottie usa o `libdotlottie_rs.so` que o próprio pacote
`dotlottie_flutter` empacota no build Linux — o mesmo motor que o widget
usa nessa plataforma — mas acessado via FFI próprio
(`lib/src/lottie_native.dart`) em vez do widget, por três motivos:

1. O widget no desktop só carrega de `url`/`asset`/`json`: um `.lottie`
   arbitrário do disco (com fontes embutidas e state machines) não é
   carregável por ele; aqui usamos `loadBytes`, preservando o pacote todo.
2. O widget não expõe slots de cor (modo interativo M3) nem avanço manual
   de relógio (`tick` de 1ms) no desktop — ambos necessários para replicar
   `lottie-to-png --slot` e `sm-render`.
3. Sem widget não há janela/textura nem captura de tela: os pixels saem do
   software renderer (ThorVG) como na versão anterior — só o display para
   inicializar o embedder (daí o `xvfb-run`).

Assinaturas C verificadas contra o `dotlottie-rs` vendorizado em
`dotlottie-rs/src/c_api/mod.rs`. O buffer usa `ARGB8888S` (alpha reto),
pelo mesmo motivo da versão anterior (ver "Alpha reto (D05)" abaixo).

O lado SVG usa o motor de verdade (`flutter_svg`, ou seja, Impeller) via
`lib/src/render_jobs.dart` — sem widget também (decodificação direta com
`SvgStringLoader` + raster em `PictureRecorder`).

## Pré-processamento do SVG (`lib/src/svg_preprocess.dart`)

Três normalizações antes de entregar o SVG ao `flutter_svg`, todas com
testes de unidade em `test/svg_preprocess_test.dart`:

1. **`flattenNestedSvg` — achatar o `<svg>` aninhado.** O SVG do Verovio
   aninha um `<svg class="definition-scale" viewBox="0 0 19200 10800">`
   dentro do `<svg>` raiz (a forma como o Verovio escala as "unidades de
   definição" para o tamanho final de página). O `flutter_svg` não suporta
   `<svg>` aninhado e dropa todo o conteúdo em silêncio (PNG 100% branco),
   como o loader de SVG do ThorVG já fazia (ver `thorvg-cli/README.md`).
   A função troca a tag interna por `<g transform="scale(...)">`
   matematicamente equivalente, preservando os demais atributos por herança.
   (Içar o `viewBox` para a raiz, a alternativa "natural", foi tentada e
   descartada: quebra os `<use>` com `transform` do Verovio — todos os
   glifos SMuFL somem.)
2. **Realocação dos textos (dentro de `flattenNestedSvg`).** Sob um ancestral
   com `transform`, o `vector_graphics` erra o texto de dois jeitos
   complementares (ambos confirmados contra o binário release): texto plano
   com `x`/`text-anchor` sai no tamanho sem escala (título gigante), e texto
   com `tspan` aninhado sai na posição errada (título colado à esquerda).
   Sem ancestral com `transform`, todos os padrões do Verovio saem corretos
   — então cada bloco `text` é movido para depois do `<g>` (sempre no topo,
   ordem relativa preservada) com `x`, `y` e `font-size` já multiplicados
   pela escala. O layout interno de cada run (avanços, kerning, âncora)
   continua por conta do motor, agora no tamanho final. O texto do Verovio
   não usa `dx`/`dy`/`rotate`/`transform` próprio (verificado no corpus).
3. **`stripTitleElements` (D01-3).** Remove todo `<title>` antes do parse —
   `<title>` nunca é desenhado, e alguns medidores de `text-anchor` incluem
   erroneamente o texto do `<title>` aninhado na largura.

## Testes

```sh
cd compare
flutter test
```

- `test/svg_preprocess_test.dart`, `test/script_test.dart`,
  `test/diff_test.dart`, `test/lottie_package_test.dart`: núcleo puro,
  determinísticos.
- `test/svg_render_test.dart`: rasterização de formas/`use`/dimensões.
  Limitação conhecida: o raster de **texto** no ambiente de teste (fonte
  Ahem, sem GPU) não é fiel ao release — posicionamento de texto é coberto
  por testes de string do pré-processamento + inspeção visual do PNG do
  binário release (como acima).

## Alpha reto (D05, mantido)

Como na versão anterior, o destino de software usa `ARGB8888S` (alpha
reto/straight), não `ARGB8888` (premultiplicado): com premultiplicado, um
preenchimento com opacidade < 100% (ex. a caixa de
`<annot type="score">`) sai do buffer já escurecido (RGB × alpha), mas o
PNG precisa de alpha reto — gravar direto dessatura a cor. A composição
sobre branco (`pixelsToWhitePng`, mesma fórmula de antes) assume alpha reto.

## Baseline de comparação (nova stack)

Os números absolutos mudaram em relação à versão `resvg`-vs-ThorVG, por
construção: o lado de referência agora é o Impeller, não o `resvg`.
Exemplo (`Grieg_Little_bird_Op43_No4`, tolerância 32): ~5,6% (antes,
~0,1–0,4%). A causa dominante são hairlines (linhas de pauta com 1,3 px):
saem na mesma fileira dos dois lados, mas com cobertura diferente
(ex.: cinza 82 no SVG vs. 15 no Lottie, diff 67 > 32). Comparações continuam
válidas em termos relativos (o diff aumenta/diminui conforme o exportador
muda); só não compare números novos com números antigos.

## Divergências conhecidas (lado exportador/motor, não da ferramenta)

Observadas validando esta versão; ficam registradas aqui como ponto de
partida para o trabalho no exportador:

- **Título ~47 px abaixo no Lottie** (`Little bird`: fileiras 0–42 no SVG
  vs. 47–92 no Lottie, mesmo tamanho): o layer de texto sai com
  `p=[1050, 91.7]` para um `y=417` do SVG (×0,1 = 41,7) — ver o `ks.p` do
  layer `ty:5` em `a/score.json`. A investigar no exportador.
- **Texto comum do Lottie cai em sans no renderer empacotado** (título sai
  sem serifa apesar de `f/LiberationSerif-Regular.ttf` embutido e
  `fonts.list` correto): o `.so` pré-compilado que o `dotlottie_flutter`
  empacota é o ThorVG upstream, **sem** as correções locais de
  `thorvg/VEROVIO_LOTTIE.md` (notadamente D01-4, itálico sintético sobre
  fonte já itálica). Texto em itálico no PNG do Lottie tende a sair com
  inclinação dupla pelo mesmo motivo. Revalidar D01-4 contra esta stack.

## Limitações atuais / decisões conhecidas

- **Critério de comparação é visual/manual** — `diff` dá um número e uma
  imagem para inspeção humana; não há um limiar de "passou/falhou"
  automático definido ainda (ver `docs/descricao-do-projeto.md`).
- **O PNG do Lottie mostra o que o `dotlottie-rs`/ThorVG empacotado
  desenha**, não o que os players oficiais de dotLottie desenham (e, nesta
  versão, sem as correções locais de `thorvg/` — ver acima).
- **O PNG do SVG mostra o que o Impeller desenha**, não mais o que o
  `resvg` desenhava — ver "Baseline" acima.
- `--font` é aceito por compatibilidade de CLI mas só verifica a
  existência do arquivo: as fontes relevantes já vão empacotadas
  (`assets/fonts/`) e o motor resolve o resto pelo sistema (mesma
  ressalva de reprodutibilidade de D01-6 da versão anterior).
