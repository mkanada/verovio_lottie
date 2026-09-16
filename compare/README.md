# compare

Ferramenta de linha de comando para validar visualmente a exportação
dotLottie do `verovio_lottie` contra a saída SVG já existente do Verovio (ver
`docs/descricao-do-projeto.md` na raiz do repositório para o contexto do
projeto).

Stack híbrida, cada lado com o mecanismo mais adequado:

- **SVG → PNG**: `svg_render/`, um binário Rust dedicado usando
  [resvg](https://github.com/RazrFalcon/resvg)/`usvg`/`tiny-skia`. Ver
  "Por que resvg, não Flutter" abaixo.
- **Lottie/dotLottie → PNG, diff, `sm-render`**: `compare` em Dart/Flutter,
  usando o [dotlottie_flutter](https://pub.dev/packages/dotlottie_flutter)
  (mesmo motor `dotlottie-rs`/ThorVG que o widget `DotLottieView` usa no
  Linux, acessado aqui via FFI direto) para renderizar Lottie/dotLottie em
  PNG por software, sem precisar de navegador headless.

O diff é pixel a pixel, com imagem de diferença e estatísticas — a decisão
de "passou/falhou" continua sendo visual/manual, sem limiar automático
definido ainda (ver `docs/descricao-do-projeto.md`).

## Por que resvg, não Flutter, para o lado SVG

A ferramenta já passou por duas fases: uma versão inicial toda em Rust
(`resvg` + `dotlottie-rs` vendorizado), depois reescrita para usar
`flutter_svg`/Impeller nos dois lados (por uniformidade de toolchain). O
lado SVG voltou a ser Rust/`resvg` porque o `flutter_svg`/Impeller se
mostrou limitado demais como *renderizador de referência* — três problemas
reais, não ruído de antialiasing:

1. Não suporta `<svg>` aninhado (dropa o conteúdo em silêncio), exigindo
   pré-processamento pra achatar a estrutura que o Verovio emite.
2. Errava posição/tamanho de texto sob um ancestral com `transform`
   (contornado por realocação de texto, que por sua vez escondeu um bug
   real: perdia a translação do `page-margin`, fazendo título/rodapé
   renderizar ~50px fora do lugar no PNG de *referência* — achado e
   corrigido nesta fase, mas sintomático da fragilidade do workaround).
3. Achado real, não corrigível: numa página do corpus
   (`Maple_Leaf_Rag_Scott_Joplin`, p.1), o Impeller desenhava um glifo
   `<use>` repetido gigante e fora de lugar — confirmado por bisseção (some
   ao remover a *primeira* referência ao glifo reutilizado) e pelo mesmo SVG
   renderizando perfeitamente no Chrome headless, ou seja, bug do
   `flutter_svg`/Impeller, não do Verovio.

`resvg` não tem nenhuma dessas limitações (suporta `<svg>` aninhado
nativamente, sem pré-processamento nenhum) — só as pegadinhas mais restritas
documentadas em "Pegadinhas do resvg" abaixo, já resolvidas.

O lado Lottie continua em Flutter: FFI direto no `.so` do
`dotlottie_flutter`, que não tem nada a ver com o Impeller/`flutter_svg` (ver
"Arquitetura: FFI direto" abaixo) — só o lado SVG trocou.

## Build

Dois binários, em toolchains diferentes:

```sh
cd compare
flutter build linux --release
```

Binário Dart/Flutter (lado Lottie/diff/sm-render) em
`compare/build/linux/x64/release/bundle/compare` (git-ignorado).

```sh
cd compare/svg_render
cargo build --release
```

Binário Rust (lado SVG) em
`compare/svg_render/target/release/svg_render` (git-ignorado; `Cargo.lock`
fica versionado, mesma convenção do `pubspec.lock`). Os scripts em
`scripts/` já apontam para os dois caminhos.

## Execução: modo batch sob xvfb (só o binário Lottie)

O binário Dart/Flutter é um app Flutter/Linux: mesmo sem abrir janela útil,
o embedder exige um display para inicializar o motor do `dotlottie_flutter`.
Em máquina sem display, rode sob `xvfb-run`:

```sh
xvfb-run -a compare/build/linux/x64/release/bundle/compare lottie-to-png ...
```

Os scripts em `scripts/` fazem isso sozinhos quando `DISPLAY` está vazio (e
`xvfb-run` existe). Logs do motor na inicialização (`Impeller rendering
backend`, `Gdk ... cursor theme`) vão para o stderr e são ruído inofensivo —
o resultado dos comandos sai no stdout como antes. **`svg_render` não
precisa de `xvfb-run`** — é um renderizador de software puro, sem
dependência de display.

## Uso

Os exemplos abaixo assumem `SVG_RENDER=compare/svg_render/target/release/svg_render`
e `COMPARE=compare/build/linux/x64/release/bundle/compare` (e `xvfb-run -a`
na frente de `$COMPARE`, se necessário) e partem da raiz do repositório.

### 1. Renderizar o SVG do Verovio em PNG

```sh
verovio -f mei -t svg partitura.mei -o partitura.svg --resource-path <verovio>/data
$SVG_RENDER partitura.svg partitura-svg.png \
  --font verovio/data/text/LiberationSerif-Regular.ttf \
  --pin-serif-family "Liberation Serif"
```

Renderiza com `resvg` sobre fundo branco opaco, no tamanho declarado em
`width`/`height` do `<svg>` raiz. Suporta o `<svg>` aninhado do Verovio
nativamente — nenhum pré-processamento de estrutura ou texto é necessário.
Antes do `usvg` processar, todo `<title>` é removido (ver "Pegadinhas do
resvg" abaixo).

- `--font <arquivo>` (repetível): carrega o arquivo de fonte via
  `fontdb::load_font_file` — necessário pra qualquer `font-family`
  referenciado pelo SVG que não esteja instalado no sistema (Liberation
  Serif, a mesma que o `.lottie` embute). Os glifos SMuFL da notação são
  `<path>` em `<defs>`, nunca texto — carregar Leipzig/Bravura/etc. não
  muda nada visualmente, mas não custa nada e mantém paridade com o que o
  `.lottie` também referenciaria se algum dia desenhasse texto SMuFL como
  fonte (hoje não desenha, D01-6).
- `--pin-serif-family <nome>`: fixa pra qual família o genérico CSS `serif`
  do Verovio (`font-family="Times, serif"`) resolve, via
  `fontdb::set_serif_family`, independente do que o SO tem instalado (ver
  `docs/plano/D01-2-controle-de-fonte-na-comparacao.md`) — precisa bater o
  nome de família de uma fonte já carregada via `--font`.

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
- `--preload-font nome:caminho.ttf` (repetível): registra uma fonte no
  motor via `dotlottie_load_font` antes de carregar o pacote — ver
  "Fonte comum sem serifa" abaixo. Não usado pelos scripts hoje (a decisão
  de tirar a fonte embutida do exportador não foi tomada), mas já
  disponível para retomar o assunto.

### 3. Comparar dois PNGs

```sh
$COMPARE diff partitura-svg.png animacao.png diff.png
```

Gera `diff.png` (fundo em tons de cinza esmaecidos + pixels divergentes em
vermelho) e imprime no terminal quantos pixels diferem e a maior diferença de
canal observada. `--tolerance N` (0-255) permite ignorar diferenças pequenas
de antialiasing.

### 4. Script: comparar uma página inteira de uma vez

```sh
compare/scripts/compare-page.sh <arquivo> <página> [tolerância]
```

Resolve a raiz do repositório pela própria localização do script; grava tudo
em `compare/out/`, ignorado pelo git. Para o pacote de produção
(`-t dotlottie`), o script lê o frame de repouso da página dos markers
`page<N-1>` e reaproveita o PNG do SVG, como antes.

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
Relógio manual de 1ms (`state_machine_tick`), ações do instante `t` rodando
**antes** do snapshot desse instante, flush antes de cada snapshot,
`state_machine_load` + `start()` medidos com `--measure-load`.

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

## Arquitetura: FFI direto em vez do widget `DotLottieView`

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
   software renderer (ThorVG) diretamente — só o display para inicializar o
   embedder (daí o `xvfb-run`, só para este binário).

Assinaturas C verificadas contra o crate
[`dotlottie-rs`](https://github.com/LottieFiles/dotlottie-rs) (commit
`eb44c991`, v0.1.58, `dotlottie-rs/src/c_api/mod.rs`) e contra os bindings do
próprio `dotlottie_flutter` (v0.1.7). O buffer usa `ARGB8888S` (alpha reto),
pelo mesmo motivo documentado em "Alpha reto (D05)" abaixo.

## Pegadinhas do resvg (`svg_render/src/main.rs`)

Duas normalizações antes de entregar o SVG ao `usvg`/`resvg`:

1. **Remoção de `<title>` (D01-3).** O resvg 0.48 inclui erroneamente o
   texto de `<title>` aninhado ao medir a largura para `text-anchor`, mesmo
   esse elemento nunca sendo desenhado por nenhum renderizador conforme a
   spec. `strip_title_elements` remove todo nó `<title>` (via `roxmltree`)
   antes do `usvg::Tree::from_data`.
2. **Fontes explícitas + `--pin-serif-family` (D01-2).** `fontdb::set_serif_family`
   fixa pra qual família o genérico CSS `serif` resolve, independente do
   que o sistema operacional tem instalado — sem isso, `fc-match "Times,
   serif"` varia por ambiente (nesta máquina resolve pra Nimbus Roman, não
   Liberation Serif).
3. **`load_system_fonts()` ainda roda** (pra não falhar em texto fora das
   fontes explicitamente carregadas), então qualquer codepoint não coberto
   pelas fontes de `--font` continua não-reprodutível entre máquinas (achado
   real: D01-6, um caractere SMuFL dentro de texto comum que dependia de
   qual fonte do sistema local tinha aquele glifo). Não é um problema
   introduzido agora — já existia na versão original do `compare` em Rust.

Ao contrário da versão `flutter_svg`, **não há nenhum pré-processamento de
estrutura ou posição de texto** — `resvg` suporta `<svg>` aninhado
nativamente e posiciona texto sob `transform` corretamente, então o
`<svg class="definition-scale">` que o Verovio emite é entregue como está.

## Testes

```sh
cd compare && flutter test
cd compare/svg_render && cargo test
```

`test/script_test.dart`, `test/diff_test.dart`, `test/lottie_package_test.dart`:
núcleo puro, determinísticos, sem dependência do motor Lottie.

## Alpha reto (D05, mantido)

O destino de software usa `ARGB8888S` (alpha reto/straight), não `ARGB8888`
(premultiplicado): com premultiplicado, um preenchimento com opacidade <
100% (ex. a caixa de `<annot type="score">`) sai do buffer já escurecido
(RGB × alpha), mas o PNG precisa de alpha reto — gravar direto dessatura a
cor. A composição sobre branco (`pixelsToWhitePng`) assume alpha reto.

## Baseline de comparação (stack híbrida)

`compare-corpus.sh 32` no corpus inteiro (10 peças, 34 páginas), medido
2026-09-16 com `svg_render`/resvg: **0,047%–1,158%, média 0,359%** — bem
abaixo da fase totalmente-Flutter (Impeller como referência: 0,79%–7,04%,
média 4,55%) e de volta à ordem de grandeza da ferramenta original em Rust
(antes de D01, sem texto comum, 0,11%–0,77%). A página com maior divergência
hoje é `Clair_de_Lune__Debussy` p.1 (1,158%) — inspecionada visualmente: as
notas/pautas praticamente coincidem (ruído residual de antialiasing), quase
todo o vermelho no diff é texto comum duplicado, causa já conhecida (ver
"Fonte comum sem serifa" abaixo, não bug do exportador nem do `svg_render`).

## Fonte comum sem serifa no `.so` empacotado

O `.so` que o pacote `dotlottie_flutter` empacota (pub.dev, hoje `0.1.7`,
sem versão mais nova publicada) não honra fonte local/embutida do Lottie
(`fonts.list` com `origin:3`/`fPath`) — texto comum sempre cai numa sans do
sistema, mesmo com o pacote gerado corretamente (confirmado por spike
isolado: um `.lottie` mínimo com só uma camada `ty:5` + TTF embutido, exatamente
conforme a spec, ainda sai em sans). Também tem o bug de itálico sintético
duplicado sobre fonte já itálica (D01-4, ver
`docs/plano/D01-4-italico-sintetico-thorvg.md`) — não corrigido: o `thorvg/`
vendorizado que trazia esse patch foi removido do projeto por estar sem uso
ativo. **Não é bug do exportador.**

Mecanismo alternativo confirmado por spike, que contorna sem trocar o
`.so`: `dotlottie_load_font(nome, bytes)` (símbolo exportado no próprio
`.so` pub.dev, ver
[`dotlottie-rs/src/c_api/mod.rs:134-149`](https://github.com/LottieFiles/dotlottie-rs/blob/eb44c991e5e2bc08daa5081caf750d1324f31d62/dotlottie-rs/src/c_api/mod.rs#L134-L149))
registra uma
fonte globalmente no motor antes de renderizar, e o carregador de Lottie a
resolve por nome mesmo sem `fPath`/dado embutido no pacote —
`lottie-to-png --preload-font nome:caminho.ttf` já expõe isso. **Não é**
"o player acha a fonte do sistema pelo nome" — o ThorVG não tem nenhuma
integração com fontconfig/fonte do SO (confirmado por leitura de código); é
o host que precisa
localizar os bytes e chamar `load_font` explicitamente. Ver a seção "Achado
(2026-09-16)" em `docs/plano/decisoes/B03-texto.md` para o contexto
completo (isso motivaria remover a fonte embutida do exportador — decisão
ainda não tomada, `--preload-font` fica pronto pra quando for retomada).

Caminho alternativo não explorado, que também resolveria D01-4 (itálico):
linkar o FFI (`lib/src/lottie_native.dart`) contra um `libdotlottie_rs.so`
compilado localmente a partir do crate
[`dotlottie-rs`](https://github.com/LottieFiles/dotlottie-rs) + um ThorVG com
o patch de D01-4 reaplicado, em vez do `.so` que o pacote `dotlottie_flutter`
empacota. Exigiria vendorizar `dotlottie-rs` e `thorvg/` de novo (ambos
removidos do projeto por estarem sem uso ativo — ver histórico do git e
`docs/plano/D01-4-italico-sintetico-thorvg.md` para o patch original) só
para esse fim.

## Limitações atuais / decisões conhecidas

- **Critério de comparação é visual/manual** — `diff` dá um número e uma
  imagem para inspeção humana; não há um limiar de "passou/falhou"
  automático definido ainda (ver `docs/descricao-do-projeto.md`).
- **O PNG do Lottie mostra o que o `dotlottie_flutter` (pub.dev) desenha**,
  o mesmo que os players oficiais de dotLottie desenham — inclusive o bug de
  itálico sintético duplicado (D01-4) e a fonte embutida não honrada — ver
  "Fonte comum sem serifa" acima.
- **O PNG do SVG mostra o que o `resvg` 0.48 desenha** — não é
  necessariamente pixel-idêntico a um browser, mas não tem as limitações
  estruturais que o `flutter_svg`/Impeller tinha (ver "Por que resvg" acima).
- `load_system_fonts()` no lado SVG significa que qualquer glifo fora das
  fontes explicitamente carregadas via `--font` ainda depende do ambiente
  local — ver "Pegadinhas do resvg" acima (D01-6).
