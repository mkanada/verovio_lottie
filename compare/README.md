# compare

Ferramenta de linha de comando para validar visualmente a exportação dotLottie
do `verovio_lottie` contra a saída SVG já existente do Verovio (ver
`docs/descricao-do-projeto.md` na raiz do repositório para o contexto do
projeto).

Stack: [resvg](https://github.com/linebender/resvg) para renderizar SVG em
PNG, e [dotlottie-rs](https://github.com/LottieFiles/dotlottie-rs) (runtime
oficial da LottieFiles, o mesmo que implementa a State Machine v2 do
dotLottie) para renderizar Lottie/dotLottie em PNG via software rendering
(ThorVG), sem precisar de navegador headless. O `dotlottie-rs` e o ThorVG são
**cópias locais** do repositório (`../dotlottie-rs`, `../thorvg`), com
correções próprias no ThorVG — ver "ThorVG local" abaixo.

## Build

```sh
cd compare
cargo build --release
```

A primeira build compila o ThorVG local (`../thorvg`, C++, que o
`../dotlottie-rs` enxerga por um symlink em `deps/thorvg`) a partir do
código-fonte — leva mais tempo que uma build Rust pura. Builds seguintes são
incrementais; mexer em `../thorvg` recompila o ThorVG na próxima build.

## Uso

### 1. Renderizar o SVG do Verovio em PNG

```sh
verovio -f mei -t svg partitura.mei -o partitura.svg --resource-path <verovio>/data
compare svg-to-png partitura.svg partitura-svg.png \
  --font verovio/data/text/LiberationSerif-Regular.ttf \
  --font verovio/data/text/LiberationSerif-Italic.ttf \
  --font verovio/data/text/LiberationSerif-Bold.ttf \
  --pin-serif-family "Liberation Serif"
```

Renderiza com `resvg` sobre fundo branco opaco. Antes de desenhar, remove
todo `<title>` do SVG (ver a pegadinha de D01-3 abaixo).

- `--font <arquivo>` (repetível): carrega uma fonte a mais no `fontdb`.
- `--pin-serif-family <nome>`: faz o genérico CSS `serif` resolver para essa
  família, independente das fontes instaladas no sistema. O texto comum do
  Verovio sai como `font-family="Times, serif"`; sem esta flag o `resvg` usa a
  substituta do sistema (aqui, Nimbus Roman) em vez da Liberation Serif que o
  `.lottie` embute. O nome precisa ser de uma fonte carregada via `--font`.

### 2. Renderizar um frame de um Lottie/dotLottie em PNG

```sh
compare lottie-to-png animacao.lottie animacao.png --width 2100 --height 2970 --frame 0
```

Aceita `.lottie` (pacote dotLottie) ou `.json` (Lottie puro). `--width`/
`--height` devem bater com o tamanho usado na comparação (ex.: o mesmo
`width`/`height` do SVG gerado pelo Verovio). Renderiza com o dotlottie-rs e o
ThorVG locais e compõe o resultado sobre fundo branco opaco.

- `--frame <n>`: frame a renderizar. Num pacote `-t dotlottie` com várias
  páginas, a página N fica em repouso no frame do marker `page<N-1>` de
  `a/score.json` (câmera de C04), não no frame N-1.
- `--slot <id>:r,g,b` (repetível, 0-1 cada): sobrescreve um slot de cor antes
  de renderizar (slots por `xml:id` do modo interativo, C03).
- `--sample <x>,<y>` (repetível): imprime o RGBA desse pixel depois de
  renderizar.

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
`verovio -t svg`, `verovio -t lottie`, `svg-to-png` (com as fontes e
`--pin-serif-family`, ver nota abaixo), lê a resolução do PNG do SVG para
passar a `lottie-to-png`, e roda `diff` — e grava tudo em `compare/out/`
(ignorado pelo git).

**Atenção:** a partir de uma partitura, o lado Lottie é o formato de
depuração `-t lottie`, que **não embute texto comum** (títulos, indicações,
dedilhados). O diff dessa página inclui esse texto como divergência. Para
comparar o pacote de produção, rode primeiro sobre a partitura (gera o PNG do
SVG) e depois passe o `.lottie` gerado com `-t dotlottie` com o mesmo nome
base: `compare-page.sh saida/Peca.lottie <página>`. Nesse modo o script
reaproveita `compare/out/<nome>-p<N>-svg.png` e renderiza o frame de repouso
da página (marker `page<N-1>`). Para o corpus inteiro já no formato de
produção, use `compare-corpus.sh`.

Saídas:

```
compare/out/<nome>-p<N>.svg
compare/out/<nome>-p<N>.json
compare/out/<nome>-p<N>-svg.png
compare/out/<nome>-p<N>-lottie.png
compare/out/<nome>-p<N>-diff.png
```

Exemplo: `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`.

**Fontes na comparação**: o SVG do Verovio desenha os glifos SMuFL
(dinâmicas, articulações etc.) como `<path>` em `<defs>` referenciados por
`<use>`, nunca como texto em fonte musical. Carregar Leipzig/Bravura via
`--font` não muda nada no corpus atual; os scripts carregam essas fontes só
por segurança. O que importa é o texto comum (`font-family="Times, serif"`):
os scripts carregam as três `LiberationSerif-*.ttf` que o exportador embute e
passam `--pin-serif-family "Liberation Serif"`, para que o PNG de referência
use a mesma fonte do `.lottie` (ver a pegadinha de D01-2 abaixo).

### 5. Simular um host: `sm-render`

```sh
compare sm-render pacote.lottie saida/ --sm sm_highlight --width 2100 --height 2970 \
  --script "0:fire d1e134;2750:fire d1e252" --snap "0,2750,3417" --prefix playback
```

Carrega a animação e uma state machine do pacote (`--sm <id>`, ou
`--sm-file <json>` para uma state machine avulsa), executa o roteiro no tempo
e salva `<prefixo>-t<ms>.png` em `saida/` para cada instante de `--snap`,
sobre fundo branco. As ações agendadas para um instante rodam **antes** do
snapshot desse mesmo instante.

- `--script "ms:ação;ms:ação"`: `fire <evento>` (state machine),
  `slot <id>:r,g,b`, `clearslot <id>` e `clearslots` (slots interativos,
  C03).
- `--sample <x>,<y>` (repetível): imprime o RGBA do pixel a cada snapshot.
- `--measure-load`: só carrega a state machine e imprime o tempo de carga.

### 6. Varreduras e roteiros prontos

| Script | O que faz | Saída |
| --- | --- | --- |
| `compare/scripts/compare-corpus.sh [tolerância]` | Corpus inteiro (`corpus/mei` + `corpus/musicxml`): SVG de cada página, um pacote `-t dotlottie` por peça, e PNGs + diff por página (frame de repouso de cada página lido dos markers) | `compare/out/corpus/<peça>/`, `resultado.csv`, `tamanhos.txt` |
| `compare/scripts/compare-layout-matrix.sh [arquivo] [tolerância]` | 16 combinações de tamanho, orientação, cabeçalho e rodapé para uma peça, página 1 | `docs/matriz-layout/` (versionado, ver o README de lá) |
| `compare/scripts/sm-playback.sh <arquivo> [máx-eventos] [seed]` | Gera pacote e timemap com a mesma seed e dispara o destaque nos onsets reais do timemap via `sm-render` (C05) | `compare/out/c05/<peça>/` |

`compare/out/` é ignorado pelo git.

## Limitações atuais / decisões conhecidas

- **Critério de comparação é visual/manual** — `diff` dá um número e uma
  imagem para inspeção humana; não há um limiar de "passou/falhou"
  automático definido ainda (ver `docs/descricao-do-projeto.md`).
- **O PNG do Lottie mostra o que o ThorVG local desenha**, não o que os
  players oficiais de dotLottie desenham (ver "ThorVG local" abaixo).

## Pegadinha do dotlottie-rs: `set_frame`/`render` podem "falhar" sem problema

`Player::load_animation_data`/`load_dotlottie_data` já renderizam o frame
inicial internamente durante o load (ThorVG/dotlottie-rs ignora esse
resultado de propósito — ver `Player::load_animation_common` no código-fonte
da lib). Por causa disso:

- Chamar `set_frame(n)` quando `n` já é o frame corrente é tratado como
  no-op pelo ThorVG e retorna erro (`"unknown error"`, que na real é
  `Result::InsufficientCondition` do lado do ThorVG, convertido para
  `Error::Unknown` no dotlottie-rs por um `match` que não distingue esse
  caso).
- Chamar `render()` sem nada ter mudado desde o último render (`updated ==
  false` internamente) também retorna erro pelo mesmo motivo.

Isso é **inofensivo**: nos dois casos o buffer de pixels já está correto. Por
isso `lottie_to_png` (`src/main.rs`) só *avisa* (stderr) quando essas duas
chamadas retornam erro, em vez de abortar — replicando o padrão usado no
próprio exemplo oficial da lib (`examples/simple_player.rs`, que usa `let _ =
player.set_frame(...)`). Se você mexer nesse código, não troque os avisos por
`?`/`.unwrap()` sem reler esta seção.

## Pegadinha do dotlottie-rs (resolvida em D05): `ARGB8888` é premultiplicado,
## desaturando qualquer preenchimento com opacidade fracionária no PNG

Achado ao validar `docs/plano/D05-casos-de-borda-estilo.md` (opacidade da
caixa de `<annot type="score">`, `fillOpacity`/`strokeOpacity` em geral): o
`lottie-to-png`/`sm-render` chamavam `set_sw_target(..., ColorSpace::ARGB8888)`
e escreviam os canais R/G/B do buffer direto no PNG de saída, como se fossem
alpha reto (straight). Mas o ThorVG documenta `ARGB8888` como
**alpha-premultiplicado** (`thorvg.h`: "Colors are alpha-premultiplied") — ou
seja, cada canal de cor já vem multiplicado pelo próprio alpha. Para uma
forma vermelha pura (`#FF0000`) a 50% de opacidade, o buffer devolvia
`(127, 0, 0, 127)` (premultiplicado) em vez de `(255, 0, 0, 128)` (reto, o que
o `resvg`/SVG de referência produz). Gravar esse RGB premultiplicado como se
fosse reto no PNG dessatura a cor — o pixel final, quando composto sobre
fundo branco por qualquer visualizador/ferramenta que assume alpha reto, sai
mais escuro/acinzentado que o SVG de referência (achado visualmente como um
rosa "empoeirado" em vez do rosa-salmão vivo do lado SVG).

**Não era um bug do exportador**: o `LottieDeviceContext`/`LottieWriter` já
escreviam `fillOpacity`/`strokeOpacity` corretos no JSON; o ThorVG também
renderizava a opacidade certa — só a extração do buffer de pixels do
`compare` estava incompatível com o color space pedido.

**Resolvido em D05** trocando `ColorSpace::ARGB8888` por
`ColorSpace::ARGB8888S` (variante "S" = *straight*, alpha reto, disponível na
mesma revisão do `dotlottie-rs` já vendorizada — usada inclusive nos
exemplos oficiais da lib) nos dois pontos de `src/main.rs` que chamam
`set_sw_target` para gerar PNGs de comparação (`lottie_to_png` e
`sm_render`). Efeito confirmado com um MEI de teste (`<annot type="score">`
com `fill-opacity="0.5"`): pixel do PNG do Lottie foi de `(127,0,0,127)`
(premultiplicado, errado) para `(255,0,0,127)` (reto, igual ao SVG a menos
de arredondamento do alpha). Corpus completo (`compare-corpus.sh 32`) não
muda — nenhuma peça do corpus tem preenchimento com opacidade fracionária
fora deste caso, então o efeito só aparece em casos de borda como este.

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

## Pegadinha do `resvg` (achada em D01-6, não corrigida): `load_system_fonts()`
## torna a referência não-reprodutível para qualquer codepoint fora das
## fontes do projeto

`svg_to_png` chama `opt.fontdb_mut().load_system_fonts()` incondicionalmente
— então qualquer caractere do SVG não coberto por uma das fontes carregadas
via `--font` recebe o glifo de **alguma fonte instalada no sistema
operacional local**, escolhida pelo `resvg` por critério próprio, não pelo
projeto. Achado real: um MusicXML com `<words font-family="Leland Text">`
contendo literalmente dois caracteres SMuFL de uso privado (U+E520) — o SVG
de referência mostrava um ícone de mão apontando, mas nenhuma das quatro
fontes musicais que o projeto vendoriza (Leipzig, Bravura, Gootville,
Leland) desenha isso nesse código — todas concordam que é "p" (glifo
`dynamicPiano`). O ícone vinha de uma quinta fonte, não identificada, só
presente por acaso na máquina que gerou o PNG — não relacionada ao
`"Leland Text"` real (nunca carregado) nem a nenhuma fonte do projeto, e
não reprodutível em outra máquina. Detalhes em
[`docs/plano/D01-6-glifo-smufl-em-texto-comum.md`](../docs/plano/D01-6-glifo-smufl-em-texto-comum.md)
("Achado adicional").

**Não corrigida** (ao contrário das pegadinhas acima) — mudar o
comportamento de `svg_to_png` para qualquer codepoint fora do already-fixed
"serif" genérico é uma decisão maior, sem caso de uso além deste até agora.
Quem for comparar texto comum com um codepoint fora do Unicode padrão
(fora da faixa coberta por Liberation Serif) deve considerar o PNG de
referência **não confiável** para esse trecho específico, até isso ser
revisitado.

## ThorVG local (D01-4): itálico sintético aplicado por cima de fonte já itálica

O `compare` não usa mais o dotlottie-rs/ThorVG originais. Usa as cópias
vendorizadas `../dotlottie-rs` (sem mudança de código; `deps/thorvg` é symlink)
e `../thorvg` (com correções próprias, listadas em
[`../thorvg/VEROVIO_LOTTIE.md`](../thorvg/VEROVIO_LOTTIE.md)).

Motivo: o loader de Lottie do ThorVG original aplica um itálico sintético
(cisalhamento de 0,18) sempre que o `fStyle` da fonte contém `"Italic"`, sem
checar se a face já é itálica. Com a `LiberationSerif-Italic.ttf` embutida
pelo exportador, todo texto comum em itálico saía **inclinado duas vezes e
deslocado**, com as letras grudadas. O exportador estava certo; a divergência
era do motor. Antes de D01-4 isso aparecia nos números como se fosse
"ruído de antialiasing em itálico" — não era.

A correção local só aplica o itálico sintético quando a face resolvida não é
itálica por desenho (`head.macStyle`/`OS/2.fsSelection`). Efeito medido
(`compare-corpus.sh 32`): corpus de 0,0196%–0,5219%/média 0,2293% para
**0,0135%–0,4153%/média 0,1318%**, as 34 páginas melhorando; Chopin Étude p.1
de 0,4520% para 0,0465%. Detalhes em
[`docs/plano/D01-4-italico-sintetico-thorvg.md`](../docs/plano/D01-4-italico-sintetico-thorvg.md).

**Atenção:** os players oficiais de dotLottie não têm essa correção. Um PNG
do `compare` mostra o que um player compilado contra `../thorvg` desenha, não
o que o dotlottie-rs original desenha.
