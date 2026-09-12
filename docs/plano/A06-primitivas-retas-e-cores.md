# A06 — Linhas, polígonos, retângulos, elipses e cores

**Depende de:** A05 · **Decisão:** nenhuma

## Objetivo

Implementar `DrawLine`, `DrawPolyline`, `DrawPolygon`, `DrawRectangle`,
`DrawRoundedRectangle`, `DrawCircle`, `DrawEllipse` e o parser de cores CSS,
reproduzindo a semântica visual do SVG. Ao final, pautas, hastes, barras de
compasso, linhas suplementares e beams devem coincidir com o SVG.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L777-L811 (`DrawCircle`, `DrawEllipse`),
  L882-L1017 (`DrawLine`, `DrawPolyline`, `DrawPolygon`, `DrawRectangle`,
  `DrawRoundedRectangle`), L618-L647 (linecap, linejoin, dasharray),
  L484-L514 (CSS global), L1295-L1308 (`GetColor`), L313-L320 (cor de grupo).
- `verovio/include/vrv/devicecontextbase.h` L23-L125 (`COLOR_NONE`, enums, `Pen`, `Brush`).
- `verovio/src/devicecontext.cpp` L144-L170 (`SetPen` calcula dash/gap por estilo).

## Arquivos

- Modificar: `verovio/src/lottiedevicecontext.cpp` (e `.h`), `verovio/src/lottiewriter.cpp`.

## Semântica a reproduzir

O SVG ganha várias coisas por CSS e herança; no Lottie tudo precisa ser explícito.

1. **Contorno sempre presente**: o CSS global
   `ellipse, path, polygon, polyline, rect {stroke:currentColor}` (L500) dá
   contorno a toda forma. Largura = `pen.GetWidth()` se > 0, senão **1** (padrão
   do SVG). Cor = cor da caneta se `HasColor()`, senão herdada. Opacidade = a da
   caneta se `HasOpacity()`.
2. **Preenchimento**: o SVG preenche por padrão (fill herdado, preto na raiz),
   exceto onde escreve `fill="none"`. Cor = brush se `HasColor()`, senão
   herdada; opacidade do brush se `HasOpacity()`.
3. Por primitiva:
   - `DrawLine`: subpath aberto de 2 vértices; sem fill; linecap da caneta; dash.
   - `DrawPolyline`: sem fill (`fill="none"` quando n > 2; com n ≤ 2 a área é
     nula); `closed = close`; linecap, linejoin, dash.
   - `DrawPolygon`: subpath fechado; fill + stroke; linejoin; dash.
   - `DrawRectangle` → `DrawRoundedRectangle(x, y, w, h, 0)`: normalizar
     largura/altura negativas como em L1002-L1010; forma `Rect` com
     `center = (x + w/2, y + h/2)`, `size = (w, h)`, `radius`; fill + stroke.
   - `DrawCircle` → `DrawEllipse(x − r, y − r, 2r, 2r)`.
   - `DrawEllipse`: raios com divisão **inteira** (`rw = width / 2`,
     `rh = height / 2`, L790-L797); `center = (x + rw, y + rh)`,
     `size = (2·rw, 2·rh)`; fill + stroke.
4. **Dash**: se `pen.GetDashLength() > 0` (o `SetPen` já preenche conforme o
   estilo), copiar dash/gap (formato em `AppendStrokeDashArray`, L640-L647).
5. **Parser de cor CSS** (em `lottiewriter.cpp`, ao resolver `colorCss`):
   `#RGB`, `#RRGGBB`, `rgb(r,g,b)` e os nomes `black, white, red, green, blue,
   gray, grey, silver, maroon, purple, fuchsia, lime, olive, yellow, navy, teal,
   aqua, orange`. Nome desconhecido → `LogWarning` (uma vez por valor) e preto.

## Fora de escopo

Béziers (A07), glifos (A09), texto (A10).

## Critérios de aceite

- `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`:
  no PNG do Lottie aparecem pautas, hastes, barras, suplementares e beams; no
  diff, o vermelho restante se concentra em glifos, ligaduras e texto.
- Repetir com `corpus/musicxml/Maple_Leaf_Rag_Scott_Joplin.mxl 1`.
- Conferir com zoom que o contorno fica por cima do preenchimento (ordem `st`
  antes de `fl`, definida em A03). Se não, inverter no writer e registrar nas notas.
- Opcional: copiar um MEI do corpus para `compare/out/`, acrescentar
  `color="red"` a uma `<note>` e conferir que só ela fica vermelha nos dois PNGs.

## Notas de execução

_(preencher ao executar)_
