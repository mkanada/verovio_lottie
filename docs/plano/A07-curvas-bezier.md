# A07 — Curvas Bézier (ligaduras, beams curvos)

**Depende de:** A06 · **Decisão:** nenhuma

## Objetivo

Implementar as primitivas curvas, convertendo-as para subpaths Lottie (vértices
com tangentes **relativas**, como definido em A02).

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L686-L775 (`DrawQuadBezierPath`,
  `DrawCubicBezierPath`, `DrawCubicBezierPathFilled`, `DrawBentParallelogramFilled`).
- `verovio/include/vrv/lottiegeometry.h`.

## Arquivos

- Modificar: `verovio/src/lottiedevicecontext.cpp`.

## O que fazer

1. `DrawQuadBezierPath(P0, P1, P2)` → cúbica: `C1 = P0 + 2/3·(P1 − P0)`,
   `C2 = P2 + 2/3·(P1 − P2)`. Subpath aberto: `v = [P0, P2]`, `o[0] = C1 − P0`,
   `i[1] = C2 − P2`, demais tangentes `(0,0)`. Só stroke (o SVG usa
   `fill="none"`), linecap e linejoin **round** (fixos no SVG), dash da caneta.
2. `DrawCubicBezierPath(P0..P3)`: `v = [P0, P3]`, `o[0] = P1 − P0`,
   `i[1] = P2 − P3`. Só stroke, round/round, dash.
3. `DrawCubicBezierPathFilled(b1, b2)`: o SVG faz
   `M b1[0] C b1[1] b1[2] b1[3] C b2[2] b2[1] b2[0]` (sem `Z`; o fill fecha
   sozinho). Subpath fechado com `v = [b1[0], b1[3], b2[0]]`:
   `o[0] = b1[1] − b1[0]`, `i[1] = b1[2] − b1[3]`, `o[1] = b2[2] − b1[3]`,
   `i[2] = b2[1] − b2[0]`, `i[0] = o[2] = (0,0)`. Fill (herdado) + stroke
   (largura da caneta), round/round.
   - Diferença conhecida: no Lottie o contorno também percorre o segmento de
     fechamento `b2[0] → b1[0]`; no SVG não. As extremidades quase coincidem,
     então deve ser desprezível — registrar se aparecer no diff.
4. `DrawBentParallelogramFilled(side[4], height)`: o SVG faz
   `M s0 C s1 s2 s3 L s3+h C (s2+h) (s1+h) (s0+h) Z` ("+h" soma `height` ao y).
   Subpath fechado com `v = [s0, s3, s3+h, s0+h]`: `o[0] = s1 − s0`,
   `i[1] = s2 − s3`, `o[1] = i[2] = (0,0)` (reta), `o[2] = s2 − s3`,
   `i[3] = s1 − s0`, `o[3] = i[0] = (0,0)` (reta de fechamento). Fill + stroke,
   round/round.
5. `DrawEllipticArc` e `DrawSpline`: não há chamadas no `View` — manter vazios.

## Fora de escopo

Glifos (A09), texto (A10).

## Critérios de aceite

- `compare/scripts/compare-page.sh corpus/mei/Chopin_Mazurka_Op6_No1.mei 1` e
  `... corpus/mei/Grieg_Butterfly_Op43_No1.mei 1`: ligaduras de expressão e de
  prolongamento aparecem e coincidem; diff restante essencialmente em glifos e texto.

## Notas de execução

- Implementado em `lottiedevicecontext.cpp`, reaproveitando os helpers de A06
  (`ToVec`, `MakeStraightBezier`, `ApplyStrokeFromPen`, `ApplyDashFromPen`,
  `ApplyFillFromBrush`) e acrescentando `Sub`/`Scale` para tangentes.
- Simplificação algébrica em `DrawQuadBezierPath`: como a tangente é relativa
  ao vértice, `C1 − P0` e `C2 − P2` reduzem para `2/3·(P1 − P0)` e
  `2/3·(P1 − P2)` sem precisar calcular `C1`/`C2` em coordenadas absolutas.
- `DrawQuadBezierPath`/`DrawCubicBezierPath`: `ApplyStrokeFromPen` +
  `ApplyDashFromPen`, mas com `lineCap`/`lineJoin` sobrescritos para
  `LINECAP_ROUND`/`LINEJOIN_ROUND` (fixos no SVG independente da caneta,
  diferente do que essas mesmas funções fazem para linhas/polígonos em A06).
  Sem `ApplyFillFromBrush` (`fill="none"` no SVG).
- `DrawCubicBezierPathFilled`/`DrawBentParallelogramFilled`: mesma
  sobrescrita de round/round, mais `ApplyFillFromBrush`; sem
  `ApplyDashFromPen` (o SVG não chama `AppendStrokeDashArray` nesses dois).
- `DrawEllipticArc` e `DrawSpline` continuam vazios — confirmado que `View`
  não os chama.
- Critérios de aceite verificados com `compare/scripts/compare-page.sh`:
  Chopin Mazurka Op.6 No.1 p.1 (2,88% de diff) e Grieg Butterfly Op.43 No.1
  p.1 (3,76% de diff). Nas imagens de diff, ligaduras de expressão e de
  prolongamento aparecem corretamente nos dois PNGs (nenhum traço de curva
  sobra em vermelho); o vermelho restante se concentra em cabeças de nota,
  acidentes, claves, dinâmicas e texto — fora de escopo (A09/A10).
- Build feita em `verovio/tools` (`make -j$(nproc)`, cmake cache já
  configurada por execução anterior).
