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

**Nenhuma peça do corpus atual usa `<svg>`** (confirmado:
`grep -l "<svg" corpus/mei/*.mei` não bate nada). Este passo depende de um
MEI mínimo criado em `compare/out/` pra ter algo pra testar.

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

1. **Percorrer só os filhos diretos de `svg`** que sejam `<path>` com
   atributo `d` (mesmo escopo mínimo do parser de glifo — MVP não cobre
   `<g>` aninhado, `<rect>`/`<circle>`/`<polygon>` dentro do `<svg>`
   embutido, nem `viewBox` custom). Para cada `<path>` fora desse escopo,
   `LogWarning` uma vez por elemento (nome da tag) e pular — mesma postura
   de "aviso + degrade graciosamente" de A06/A10, nunca abortar a
   exportação.
2. **Parsear e transformar**: `ParseSvgPathData(path.attribute("d").value(),
   paths)`; para cada vértice/tangente de cada `LottieBezier` resultante,
   aplicar `v' = (x, y) + v · (scale · DEFINITION_FACTOR)` (tangentes `i`/`o`
   são relativas, então só escalam, não translladam — mesmo tratamento que
   toda tangente já recebe em `MakeGlyphShape`/A08).
3. **Cor**: ao contrário de toda primitiva nativa do exportador (que sempre
   herda `stroke:currentColor` do CSS global do Verovio — ver A06), um
   `<path>` dentro de um `<svg>` embutido é markup **externo**: sem atributo
   `fill`/`stroke` explícito, o padrão SVG puro é `fill:black` (mesmo
   default global, então cai igual) mas **`stroke:none`** (diferente de
   tudo mais no exportador — ver "Armadilhas"). Ler `fill`/`stroke` do
   `<path>` (atributo direto ou dentro de `style="..."`, o que existir) com
   `ResolveColor` e:
   - `fill`: se ausente, `COLOR_NONE` (herda, resolve pra preto no writer,
     igual ao resto); se presente e não for `"none"`, cor explícita
     (`hasFill=true`); se `"none"`, `hasFill=false`.
   - `stroke`: se ausente, `hasStroke=false` (diferente do padrão do resto
     do exportador!); se presente e não for `"none"`, `hasStroke=true` com a
     cor lida (largura: atributo `stroke-width` se presente, senão 1).
4. **Inserir**: um `LottieShape` (kind `Path`) por `<path>` válido, via
   `AddShape`, no `m_nodeStack.back()` corrente (mesmo padrão de toda outra
   primitiva).

## Fora de escopo

`<rect>`/`<circle>`/`<ellipse>`/`<polygon>`/`<g>` aninhado dentro do `<svg>`
embutido, `viewBox` custom do `<svg>` filho, gradientes/patterns/`<defs>`,
qualquer coisa que não seja `<path d="...">` direto. Se o corpus real algum
dia exigir mais que isso, expandir então (mesmo princípio de "paridade
incremental" do `CLAUDE.md`) — não implementar preventivamente.

## Critérios de aceite

- Compila.
- Criar um MEI mínimo em `compare/out/d02-svg-teste.mei` (copiar um MEI
  pequeno do corpus e inserir um `<svg>` com 1-2 `<path>` simples, um com
  `fill` explícito e outro sem, um com `stroke` explícito) e comparar
  `compare/scripts/compare-page.sh` (formato `lottie`, já que não depende
  de pacote) contra o SVG de referência: a forma aparece na posição/escala
  certas, cor default preta quando omitida, cor explícita quando presente,
  e **sem contorno indesejado** quando `stroke` não foi especificado no MEI
  de teste.
- `<path>` sem `d` válido ou elemento fora do escopo (`<rect>` de teste, por
  exemplo): não crasha, loga aviso, resto da página renderiza normalmente.
- Rodar `compare/scripts/compare-corpus.sh` no corpus real de novo (sem
  `<svg>`, então só confirma que este passo não regrediu nada) — 0 páginas
  a mais de diferença nas médias já publicadas em `relatorio-paridade.md`.

## Armadilhas

- **Não copiar o padrão "sempre tem stroke" de A06** — é o oposto do default
  aqui (ver item 3 de "O que fazer"). Esse é o erro mais provável ao
  implementar por analogia com o resto do exportador.
- `pugi::xml_node::attribute("d").value()` devolve `""` (não nulo) se o
  atributo não existir — checar `path.attribute("d")` (bool) antes de
  chamar `ParseSvgPathData` com string vazia.
