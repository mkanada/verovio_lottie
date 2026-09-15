# D01-6 — Desenhar glifos SMuFL embutidos em texto comum como vetor

**Depende de:** D01 (texto comum), A08/A09/A10 (parser/desenho de glifo
SMuFL como caminho vetorial) · **Decisão necessária:** nenhuma nova — usa um
mecanismo (glifo SMuFL como vetor) já decidido e implementado desde A08-A10,
só estendendo onde ele é acionado.

## Problema

Usuário reportou, olhando Clair de Lune: "tem duas 'mãozinhas' no primeiro
compasso, que é renderizada no SVG mas não é no lottie". No SVG de
referência (`compare svg-to-png`), o compasso 1 mostra dois pequenos ícones
antes de "con sordina"; no `.lottie`, esse espaço fica em branco.

## Causa

O MusicXML de origem (`corpus/musicxml/Clair_de_Lune__Debussy.mxl`) tem, em
dois lugares, um `<words font-family="Leland Text">` cujo conteúdo é
**literalmente** dois caracteres da área de uso privado do SMuFL
(`U+E520 U+E520`), não texto comum. O `<dir>` resultante fica ancestral de
um nó `Text` com esses dois caracteres; `View::DrawText` (`view_text.cpp`)
manda isso para `DrawDirString`, que só converte Unicode musical → SMuFL
**se a fonte corrente já estiver marcada como fonte SMuFL** — não é o caso
aqui (contexto de `<dir>`, fonte comum) — então os dois caracteres, já em
SMuFL, seguem sem conversão para `DrawTextString` → `dc->DrawText`.

`LottieDeviceContext::DrawText` (`lottiedevicecontext.cpp`) só tem dois
caminhos: fonte SMuFL → desenha como vetor; senão → embute como texto
comum (`ty:5`, fontes Liberation Serif). U+E520 cai no segundo caminho, mas
a Liberation Serif não tem glifo ali — o texto fica presente no JSON
(`"t":""`) mas invisível.

`SvgDeviceContext` tem exatamente a mesma lacuna: também emite o texto cru
com `font-family="Times, serif"`, sem tratamento especial. O SVG de
referência só mostra *alguma coisa* ali porque `compare svg-to-png` chama
`fontdb_mut().load_system_fonts()` (`compare/src/main.rs`) — o `resvg`
então cai de volta para **qualquer fonte do sistema operacional** que tenha
um glifo nesse código, não necessariamente uma das fontes do projeto.

**Achado importante, verificado nesta sessão:** o glifo que aparece no SVG
de referência **não é** o que as quatro fontes musicais que o Verovio
vendoriza dizem que U+E520 é. Testado isoladamente (uma fonte por vez,
mesmo SVG mínimo, mesma via de renderização do `compare`):

| Fonte | O que U+E520 desenha |
| --- | --- |
| `Leipzig.ttf` (default do Verovio) | "p" (glifo `dynamicPiano`, confirmado em `verovio/data/Leipzig.xml`) |
| `Bravura.otf` | "p" |
| `Gootville.otf` | "p" |
| `Leland.otf` | "p" |
| *(alguma fonte do sistema, via `load_system_fonts`)* | um ícone de mão apontando |

As quatro fontes musicais que o Verovio de fato suporta concordam entre si
(`dynamicPiano`, ou seja "pp" — pianíssimo, uma leitura musicalmente
sensata antes de "con sordina"). O ícone de mão vem de uma quinta fonte,
não vendorizada, não relacionada ao projeto, cuja identidade não foi
determinada e que **não é garantida em nenhuma outra máquina** — só existe
"por acaso" nesta, e não tem relação com a fonte real declarada no
MusicXML (`"Leland Text"`, que o `compare` nunca carrega — só carrega
`Leland.otf`, a fonte musical, não a companheira de texto). Ou seja, o PNG
de referência que motivou o relato do usuário já não era, ele mesmo, um
alvo estável — é um artefato do conjunto de fontes desta máquina.

## Correção

Em vez de perseguir esse alvo não-reprodutível (que exigiria vendorizar
"Leland Text", uma fonte nova, fora do que B03 decidiu, e cujo resultado
nem seria consistente com o que as próprias fontes musicais do Verovio
dizem sobre esse código), `LottieDeviceContext::DrawText` (branch de texto
comum) passa a: se **todos** os caracteres do run forem SMuFL (`>= U+E000`)
**e** tiverem um glifo nos recursos musicais já carregados (mesma tabela
que `Resources::GetGlyph` usa para tudo mais no Verovio, portanteiramente
sob controle do projeto, sem depender do sistema operacional), desenhar o
run inteiro como vetor — mesmo mecanismo (`MakeGlyphShape`/
`GetGlyphAdvance`) já usado pelo branch SMuFL logo acima, sem duplicar
lógica de parsing/escala.

Guarda explícita (`c < SMUFL_E000_brace`) evita acionar esse caminho para
um run que seja só espaço (a tabela de glifos do Leipzig também define
`U+0020`, para espaçamento entre símbolos musicais) ou os sinais soltos de
bemol/natural/sustenido (`U+266D`-`U+266F`, também na tabela) — só códigos
genuinamente na área de uso privado (onde a Liberation Serif nunca tem
glifo) entram nesse branch.

`SvgDeviceContext`/o núcleo do Verovio (`view_text.cpp`) não foram tocados
— a mesma lacuna na saída SVG "real" (fora do `compare`) continua existindo
se alguém um dia rodar `verovio -t svg` neste MEI num navegador sem uma
fonte de sistema por acaso cobrindo esse código. Corrigir isso é fora do
escopo deste passo (mudaria o núcleo compartilhado, usado por todo `View`,
não só pelo exportador dotLottie).

## Verificação

- Chopin p.1, Nocturne, Mazurka etc.: nenhuma mudança de pixel (confirmado
  pela varredura completa do corpus — só Clair de Lune mudou).
- Clair de Lune, dois pontos onde o `<dir>` aparece (`` antes de
  "con sordina", duas ocorrências na peça): "pp" agora desenhado, com
  tamanho visualmente consistente com o texto vizinho ("con sordina", mesmo
  `pointSize` nominal) — conferido lado a lado.
- Diff pixel a pixel contra o SVG de referência **não** cai a zero nesse
  glifo especificamente (compara formas diferentes: "p" vetorial vs. o
  ícone de mão do artefato de fonte do sistema) — esperado e aceito, não é
  regressão: ver "Achado importante" acima.
- Nenhum run de espaço/acidente solto passou a ser desenhado como vetor
  (varredura do corpus sem regressão confirma isso indiretamente; a guarda
  `c < SMUFL_E000_brace` cobre isso por construção).

## Achado adicional (não corrigido aqui): `load_system_fonts()` torna
## `compare svg-to-png` não-reprodutível para qualquer codepoint fora das
## fontes do projeto

Pegadinha nova do `compare`, prima da de D01-2 (substituição de fonte) mas
mais ampla: `compare/src/main.rs` chama
`opt.fontdb_mut().load_system_fonts()` incondicionalmente em
`svg_to_png`, então **qualquer** caractere não coberto pelas fontes
explicitamente carregadas via `--font` recebe o glifo de alguma fonte do
sistema operacional local — resultado dependente de máquina, não do
projeto. `--pin-serif-family` (D01-2) resolve isso só para o genérico CSS
`serif`; não protege contra este caso (um codepoint fora da faixa normal
de texto, coberto por acaso por uma fonte de sistema qualquer). Não
corrigido aqui (mudaria o comportamento de referência do `compare` para
casos além deste, precisa de decisão própria se algum dia importar) — só
documentado como achado, junto da pegadinha irmã em `compare/README.md`.

## Notas de execução

Implementado como descrito. `cmake ../cmake && make -j4` limpo (mesma build
de D01-5, arquivos alterados juntos: só `lottiedevicecontext.cpp` aqui).
Corpus completo (`compare-corpus.sh 32`, D01-5 e D01-6 juntos numa só
rodada — as duas correções vieram do mesmo pedido do usuário, na mesma
sessão):

| | mín | máx | média |
| --- | --- | --- | --- |
| Antes (D01-4, sem D01-5/D01-6) | 0,0135% | 0,4153% | 0,1318% |
| Depois (D01-5 + D01-6) | 0,0135% | 0,3946% | **0,1251%** |

Nenhuma página piorou; só as 5 páginas de Clair de Lune mudaram (única peça
do corpus com os dois padrões — Bold Italic e SMuFL-em-texto-comum):

| Página | Antes | Depois |
| --- | --- | --- |
| 1 | 0,3963% | 0,2952% |
| 2 | 0,3551% | 0,3114% |
| 3 | 0,2923% | 0,2923% (sem mudança — não tem nenhum dos dois padrões) |
| 4 | 0,4153% | 0,3946% |
| 5 | 0,1301% | 0,0684% |

Matriz de layout (`compare-layout-matrix.sh`, Chopin Étude — não tem nenhum
dos dois padrões): sem mudança, min 0,0318%/max 0,0751%/média 0,0480% nas
duas rodadas — confirma não-regressão.

`corpus/lottie/*.lottie` (as 10 peças) e `docs/mesa-de-prova/
Clair_de_Lune__Debussy/` (15 PNGs) regenerados; o artefato "Mesa de Prova"
(claude.ai) republicado com as novas imagens/porcentagens de Clair de Lune.
`docs/exemplos/` (Scarlatti) e `docs/matriz-layout/` (Chopin Étude) não
precisaram regenerar — nenhuma das duas peças usa os padrões corrigidos
aqui.
