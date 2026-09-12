# A10 — Texto: fonte SMuFL via glifos; texto comum contabilizado

**Depende de:** A09 · **Decisão:** nenhuma (o texto comum depende de D-TEXTO, fase D)

## Objetivo

Renderizar trechos de texto cuja fonte é SMuFL (dinâmicas como *p*, *f*, *sfz*;
símbolos de metrônomo) com os contornos dos glifos e o mesmo posicionamento do
SVG. O texto comum (Times) fica **sem** renderizar até D-TEXTO, mas é contado e
reportado.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L1019-L1096 (`StartText`, `MoveTextTo`,
  `MoveTextVerticallyTo`, `EndText`), L1100-L1167 (`DrawText`), L376-L380
  (`StartTextGraphic` cria o `tspan` pai).
- `verovio/include/vrv/devicecontextbase.h` L137-L180 — `FontInfo`
  (`GetSmuflFont`, `GetPointSize`, `GetLetterSpacing`).
- `verovio/include/vrv/devicecontext.h` L178-L180 — `GetTextExtent`,
  `GetSmuflTextExtent` (larguras com as métricas do Verovio).
- `verovio/src/view_text.cpp` L100-L300 — como dinâmicas e textos chamam
  `StartText`/`DrawText` (procure `SetSmuflWithFallback`).

## Arquivos

- Modificar: `verovio/include/vrv/lottiedevicecontext.h`, `verovio/src/lottiedevicecontext.cpp`.

## Modelo de texto (estado no DC, independente da árvore)

1. `StartText(x, y, alignment)`: inicia um *chunk* — caneta em `(x, y)`,
   alinhamento guardado, lista de formas pendentes vazia, largura acumulada 0.
2. `MoveTextTo(x, y, alignment)`: finaliza o chunk atual (item 5) e inicia outro
   em `(x, y)` com o novo alinhamento (no SVG, x/y absolutos iniciam um novo bloco
   ancorado).
3. `MoveTextVerticallyTo(y)`: só muda o y da caneta (sobrescrito/subscrito); não
   inicia chunk.
4. `DrawText(text, wtext, x, y, width, height)`:
   - se `x` e `y` são válidos e não há `width`/`height` (L1163-L1166), mover a caneta para `(x, y)`;
   - fonte SMuFL (`GetSmuflFont() != SMUFL_NONE`): para cada `char32_t` de
     `wtext`, gerar o glifo como em A09 na posição da caneta e avançar com o
     mesmo avanço inteiro; guardar as formas como pendentes;
   - fonte comum: não gerar formas; avançar a caneta com
     `GetTextExtent(wtext, &extend, true)` e incrementar `m_skippedTextRuns`;
   - `letter-spacing` do `FontInfo` (se ≠ 0): somar ao avanço de cada caractere
     (o SVG repassa `letter-spacing` ao visualizador).
5. Finalizar chunk (em `MoveTextTo` e `EndText`): deslocamento horizontal = 0
   (left), −largura/2 (center) ou −largura (right); aplicar às formas pendentes e
   inseri-las com `AddShape` no nó corrente.
6. `EndPage`: se `m_skippedTextRuns > 0`, `LogWarning` informando quantos trechos
   de texto comum não foram renderizados (pendente D-TEXTO) e zerar o contador.

## Fora de escopo

Texto comum (D-TEXTO), `DrawRotatedText` (sem chamadas no `View`).

## Critérios de aceite

- `compare/scripts/compare-page.sh` em `corpus/mei/Chopin_Etude_Op10_No9.mei 1`
  e `corpus/mei/Grieg_Butterfly_Op43_No1.mei 1`: dinâmicas coincidem em posição
  e forma (tolerando antialias).
- Títulos, compositor, andamento e dedilhados **ausentes** no Lottie, e o aviso
  aparece no log.

## Armadilhas

- Dinâmica deslocada horizontalmente costuma ser alinhamento (text-anchor) ou
  caneta não reiniciada no chunk.
- No SVG, `x`/`y` iguais a 0 não são escritos (L1033-L1034); isso não muda a
  posição, mas não trate 0 como "sem posição" no seu código.

## Notas de execução

_(preencher ao executar)_
