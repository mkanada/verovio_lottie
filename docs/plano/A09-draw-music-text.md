# A09 — `DrawMusicText` (glifos SMuFL)

**Depende de:** A06, A08 · **Decisão:** nenhuma

## Objetivo

Desenhar os glifos das fontes musicais (cabeças de nota, claves, acidentes,
pausas, fórmulas de compasso, colchetes, articulações) com contornos idênticos
aos do SVG.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L1174-L1215 — `DrawMusicText`.
- `verovio/include/vrv/glyph.h` — `GetBoundingBox` L44, `GetUnitsPerEm` L57,
  `GetHorizAdvX` L81 (note que `SetHorizAdvX`, L82, guarda o valor ×10).
- `verovio/include/vrv/resources.h` L94 — `GetGlyph(char32_t)`.
- `verovio/src/view_graph.cpp` L279-L357 — como o `View` chama.
- `verovio/include/vrv/svgpathparser.h`.

## Arquivos

- Modificar: `verovio/include/vrv/lottiedevicecontext.h`, `verovio/src/lottiedevicecontext.cpp`.

## O que fazer

1. Cache `std::map<const Glyph *, std::vector<LottieBezier>>` no DC, preenchido
   com `ParseGlyphXml(glyph->GetXML(), ...)` na primeira vez.
2. Para cada `char32_t c` de `text`: `glyph = GetResources()->GetGlyph(c)`; se
   nulo, pular (como o SVG).
3. Escala (L1202-L1204), com `font = m_fontStack.top()`:
   `sx = sy = (double)font->GetPointSize() / glyph->GetUnitsPerEm() * DEFINITION_FACTOR`;
   se `font->GetWidthToHeightRatio() != 1.0f`, `sx *= ratio`.
4. Copiar os subpaths do cache e transformar: vértice → `(x + sx·gx, y + sy·gy)`;
   tangente → `(sx·tx, sy·ty)`. O SVG faz `translate(x,y) scale(sx,sy)` no
   `<use>`, e o parser já aplicou o `scale(1,-1)` interno.
5. `LottieShape` Path com todos os subpaths do glifo; fill herdado (sem cor
   própria); stroke herdado com largura `sy` — o CSS `path {stroke:currentColor}`
   também atinge o path do glifo, com largura 1 em unidades do glifo, depois escalada.
6. Avanço horizontal: copiar **exatamente** a aritmética inteira do SVG
   (L1208-L1213): se `glyph->GetHorizAdvX() > 0`,
   `x += glyph->GetHorizAdvX() * font->GetPointSize() / glyph->GetUnitsPerEm();`
   senão, usar a largura de `GetBoundingBox`.
7. `setSmuflGlyph` pode ser ignorado (só serve ao BBox).

## Fora de escopo

Texto (A10).

## Critérios de aceite

- `compare/scripts/compare-page.sh` em `corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`,
  `corpus/mei/Chopin_Etude_Op10_No9.mei 1` e `corpus/musicxml/Clair_de_Lune__Debussy.mxl 1`:
  os glifos aparecem e coincidem; o vermelho do diff fica essencialmente no texto.
- Mínimas e semibreves aparecem **vazadas**. Se aparecerem cheias, o problema é a
  regra de preenchimento com vários subpaths: registrar nas notas e testar
  `"r": 2` (evenodd) no fill dos glifos.

## Notas de execução

_(preencher ao executar)_
