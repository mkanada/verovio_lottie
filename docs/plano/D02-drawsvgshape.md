# D02 — `DrawSvgShape` (SVG embutido no MEI)

**Depende de:** A08 (parser de path SVG) · **Decisão necessária:** nenhuma —
escopo mínimo (só `<path>`) já é a leitura direta de "casos simples antes de
complexos" do `CLAUDE.md`; ver "Fora de escopo".

## Objetivo

Implementar `LottieDeviceContext::DrawSvgShape` (hoje um stub vazio,
`verovio/src/lottiedevicecontext.cpp:528`), que recebe um nó `<svg>`
embutido no MEI (elemento `<svg>` do módulo `visualization` do MEI — texto/
notação customizada colada como XML SVG cru) e precisa "assar" seu conteúdo
como `LottieShape`s, do mesmo jeito que todo o resto do exportador já faz.

**Correção (achada via inspeção visual na "Mesa de Prova", ver
`docs/plano/D01-3-titulo-aninhado-resvg.md`): a premissa abaixo estava
errada.** `grep -l "<svg" corpus/mei/*.mei` de fato não bate nada nos
arquivos de *entrada* do corpus, mas isso não é o quadro completo:
`Doc::GenerateFooter()` (`verovio/src/doc.cpp:245`) insere automaticamente,
em **toda página de toda peça** (a menos que `--footer none`), um
`Fig`+`Svg` carregando `verovio/data/footer.svg` (o logo vetorial "MEI
engraved with Verovio" — puros `<path>`, sem texto de fonte nenhuma) —
independente do que está no MEI/MusicXML de origem. Confirmado visualmente:
esse rodapé aparece no PNG de referência (SVG) e **nunca** aparece no
`.lottie` (stub vazio), em todas as 34 páginas do corpus. Ou seja, este
passo já tem cobertura real e onipresente hoje — não depende de um MEI
mínimo sintético pra ter algo pra testar (embora ainda possa valer a pena
criar um caso sintético à parte pra cobrir `fill`/`stroke` explícitos, que
o rodapé não exercita — ver "Critérios de aceite").

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L1229-1241 (`SvgDeviceContext::DrawSvgShape` —
  a implementação de referência: aplica `translate(x,y) scale(scale·10,
  scale·10)` ao nó corrente e copia os filhos do `<svg>` recebido verbatim).
- `verovio/src/view_text.cpp` L547-573 (`View::DrawSvg`, único chamador) —
  `width`/`height`/`scale` já vêm ajustados por `staffSize`/`graceFactor`
  antes de chegar no `DeviceContext`; `svg.Get()` devolve o `pugi::xml_node`
  cru do elemento `<svg>` do MEI.
- `verovio/include/vrv/svgpathparser.h` L29 (`ParseSvgPathData(const
  std::string &d, std::vector<LottieBezier> &paths)`) — já existe e é
  genérico (não específico de glifo; `ParseGlyphXml` em L37 é uma casca em
  cima dele). Este passo usa `ParseSvgPathData` diretamente.
- `verovio/src/lottiedevicecontext.cpp` L385-426 (`MakeGlyphShape`) — padrão
  de como um path vira `LottieShape` com escala aplicada aos vértices (só
  que ali a escala vem do glifo SMuFL; aqui vem do parâmetro `scale` +
  `DEFINITION_FACTOR`, igual ao SVG).
- `verovio/include/vrv/vrvdef.h` L457 (`DEFINITION_FACTOR`) — o SVG usa
  `scale · DEFINITION_FACTOR`; reproduzir a mesma conta.
- `verovio/src/lottiewriter.cpp`, procure `ResolveColor` (parser de cor CSS
  já implementado em A06: hex `#RGB`/`#RRGGBB`, `rgb(r,g,b)`, nomes) — este
  passo **reaproveita**, não reimplementa; ver "O que fazer" item 3 sobre
  onde ele precisa ficar acessível.

## Arquivos

- Modificar: `verovio/src/lottiedevicecontext.cpp` (`DrawSvgShape`).
- Possível extração: se `ResolveColor` (cor CSS) estiver hoje só em
  `lottiewriter.cpp` como função `static`/anônima, mover pra um header
  compartilhado (ex. `verovio/include/vrv/svgpathparser.h` ou um novo
  `csscolor.h` pequeno) pra ser chamável tanto do writer quanto do
  `DrawSvgShape` (que roda em tempo de desenho, não de escrita) — ver item 3.

## O que fazer

1. **Coletar os `<path>` do `svg`**, descendo por `<g>` **sem atributos**
   (nenhum `transform`/`fill`/`stroke`/`style` próprio) — necessário na
   prática: os `<path>` de `data/footer.svg` (única cobertura real e
   onipresente do corpus) ficam um nível dentro de um `<g>`, não como filhos
   diretos do `<svg>` (ver "Armadilhas"). Um `<g transform="...">` é avisado
   uma vez (`LogWarning`) e pulado inteiro; qualquer outro elemento que não
   seja `<path>`/`<g>` (`<rect>`/`<circle>`/`<polygon>` etc., `viewBox`
   custom do `<svg>` filho, gradientes/patterns/`<defs>`) também é avisado
   uma vez por nome de tag e pulado — mesma postura de "aviso + degrade
   graciosamente" de A06/A10, nunca abortar a exportação.
2. **Parsear e transformar**: `ParseSvgPathData(path.attribute("d").value(),
   paths)`; para cada vértice/tangente de cada `LottieBezier` resultante,
   aplicar `v' = (x, y) + v · (scale · DEFINITION_FACTOR)` (tangentes `i`/`o`
   são relativas, então só escalam, não transladam — mesmo tratamento que
   toda tangente já recebe em `MakeGlyphShape`/A08).
3. **Cor** — confirmado empiricamente (ver "Armadilhas") que só `fill` é
   markup verdadeiramente externo aqui; `stroke` acaba seguindo o mesmo
   padrão do resto do exportador por causa do CSS global do Verovio:
   - `fill`: ler do `<path>` (atributo direto ou dentro de `style="..."`, o
     que existir) com `ResolveColor`. Ausente → `COLOR_NONE` (herda,
     resolve pra preto no writer, igual ao resto); presente e não `"none"`
     → cor explícita (`hasFill=true`); `"none"` → `hasFill=false`.
   - `stroke`: **sempre** `hasStroke=true`, `strokeColor=COLOR_NONE`
     (herdado) — o atributo `stroke`/`style` do próprio `<path>` nunca chega
     a se manifestar visualmente, então não é lido. Só `stroke-width` vem do
     atributo do `<path>` (default 1 se ausente), escalado como qualquer
     outra grandeza (`stroke-width · scale · DEFINITION_FACTOR`).
4. **Inserir**: um `LottieShape` (kind `Path`) por `<path>` válido, via
   `AddShape`, no `m_nodeStack.back()` corrente (mesmo padrão de toda outra
   primitiva).

## Fora de escopo

`<rect>`/`<circle>`/`<ellipse>`/`<polygon>` dentro do `<svg>` embutido,
`<g transform="...">` (só `<g>` sem atributos é suportado, para descer até
os `<path>` — ver "O que fazer" item 1), `viewBox` custom do `<svg>` filho,
gradientes/patterns/`<defs>`, qualquer coisa que não seja `<path d="...">`
(direto ou dentro de um `<g>` transparente). Se o corpus real algum dia
exigir mais que isso, expandir então (mesmo princípio de "paridade
incremental" do `CLAUDE.md`) — não implementar preventivamente.

## Critérios de aceite

- Compila.
- Rodar `compare/scripts/compare-corpus.sh 32` no corpus real: o rodapé
  "MEI engraved with Verovio" passa a aparecer no `.lottie`, na mesma
  posição/escala do SVG, nas 34 páginas — confirmar por crop visual em 2-3
  peças. Como isso cobre conteúdo real que hoje falta em toda página, a %
  de divergência deve **cair** de forma mensurável e uniforme (não "0
  páginas a mais" como uma versão anterior deste documento presumia,
  incorretamente — ver "Correção" acima); documentar os números novos
  contra os de D01-3 (0,1002%–0,5997%, média 0,2977%). **Feito:**
  0,0196%–0,5212%, média 0,2290% (ver "Notas de execução").
- Além do rodapé (que só exercita `fill` implícito/preto, sem `stroke`),
  criar um MEI mínimo em `compare/out/d02-svg-teste.mei` (copiar um MEI
  pequeno do corpus e inserir um `<svg>` com 1-2 `<path>` simples, um com
  `fill` explícito e outro sem, um com `stroke` explícito) e comparar
  `compare/scripts/compare-page.sh` (formato `lottie`, já que não depende
  de pacote) contra o SVG de referência: a forma aparece na posição/escala
  certas, cor default preta quando omitida, cor explícita quando presente,
  e **sem contorno indesejado** quando `stroke` não foi especificado no MEI
  de teste.
- `<path>` sem `d` válido ou elemento fora do escopo (`<rect>` de teste, por
  exemplo): não crasha, loga aviso, resto da página renderiza normalmente.

## Armadilhas

- **Correção (achada na implementação real, não suposição): o item 3 acima
  estava errado sobre `stroke`.** Testado diretamente contra o `resvg` real
  (SVG isolado com `<style>path{stroke:red}</style>` e um `<path
  stroke="none" stroke-width="6">`): o retângulo renderiza com contorno
  **vermelho**, apesar do `stroke="none"` explícito. Isso é comportamento
  padrão de cascata CSS (atributos de apresentação têm especificidade zero e
  perdem pra *qualquer* regra de stylesheet, mesmo uma de baixíssima
  especificidade) — não é peculiaridade do `resvg`. Como o SVG do Verovio
  sempre inclui `#<id> path {stroke:currentColor}` no `<style>` global
  (mesma regra que já justifica o "sempre tem stroke" de A06/`ApplyStrokeFromPen`),
  ela também se aplica a todo `<path>` de um `<svg>` embutido — o atributo
  `stroke`/`stroke="none"` do próprio path é **sempre ignorado** na
  renderização real. `fill` não tem regra global equivalente, então esse
  continua sendo lido do atributo normalmente. Resultado prático: `stroke`
  segue **o mesmo padrão do resto do exportador** (sempre
  `hasStroke=true`, cor herdada/`COLOR_NONE`), e não o oposto como este
  documento presumia antes de implementar — só `stroke-width` (não coberto
  por regra alguma) ainda vem do atributo do próprio `<path>`, com default 1.
- `pugi::xml_node::attribute("d").value()` devolve `""` (não nulo) se o
  atributo não existir — checar `path.attribute("d")` (bool) antes de
  chamar `ParseSvgPathData` com string vazia.
- **Achado na implementação (não estava no escopo original):** `footer.svg`
  — o único caso com cobertura real e onipresente no corpus — tem seus
  `<path>` um nível dentro de um `<g>` (`<svg><g><path/><path/><g><path/>
  ×12</g></g></svg>`), não como filhos diretos do `<svg>` como a seção "O
  que fazer" item 1 presumia. Sem descer por esse `<g>`, o rodapé não
  apareceria em lugar nenhum do corpus real, esvaziando o objetivo do passo.
  Implementado como descida transparente por `<g>` **sem atributos**
  (nenhum dos `<g>` do rodapé tem `transform`/`fill`/`stroke`/`style`) — um
  `<g transform="...">` é avisado uma vez e pulado (grau de escopo mínimo
  ainda respeitado: só o suficiente pro caso real, não suporte genérico a
  transform aninhado).

## Notas de execução

- Cor compartilhada: `ResolveColor` (e seus helpers `ParseHexColor`/
  `ParseRgbFunction`/`ParseNamedColor`) foram extraídos de `lottiewriter.cpp`
  para `verovio/include/vrv/csscolor.h` + `verovio/src/csscolor.cpp` (opção
  já prevista em "Arquivos" acima), já que agora são chamados tanto em tempo
  de escrita (`LottieWriter`) quanto em tempo de desenho (`DrawSvgShape`).
  `ColorIntToRgb01` (só usado no writer) permaneceu em `lottiewriter.cpp`.
- Medido `compare/scripts/compare-corpus.sh 32` no corpus real (34 páginas)
  após a implementação (incluindo a correção de `<g>` e de `stroke` acima):
  **0,0196%–0,5212%, média 0,2290%** — queda uniforme frente ao baseline do
  D01-3 (0,1002%–0,5997%, média 0,2977%). Confirmado por crop visual (3x) em
  `Chopin_Etude_Op10_No9` p.1: o rodapé "MEI engraved with Verovio" aparece
  agora no `.lottie`, na mesma posição/escala/estilo do SVG.
- Teste sintético (`compare/out/d02-svg-teste.mei`, não commitado — fica em
  `compare/out/`, ignorado pelo git desde A05): como `<fig>` só é filho
  permitido de `RunningElement`/`<div>` no parser MEI atual (`<dir>` não
  aceita `<fig>` — tentado e rejeitado com "Element <fig> within <dir> is
  not supported"), o teste usa um `<pgFoot func="all">` custom no
  `<scoreDef>` (renderizado com `--use-pg-footer-for-all`, já que sem essa
  opção um `pgFoot func="all"` só aparece a partir da página 2) com 3
  `<path>`: um com `fill` explícito, um com `fill="none"`+`stroke`+
  `stroke-width` explícitos, e um sem nenhum atributo de cor. Resultado
  visual (crop 3x) e diff de pixel confirmam paridade total SVG/`.lottie`
  depois da correção de `stroke` acima (antes dela, o traço diagonal
  renderizava azul no `.lottie` contra preto no SVG de referência).
