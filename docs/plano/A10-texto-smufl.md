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

- Implementado em `lottiedevicecontext.h`/`.cpp`. O bloco de construção de
  shape de glifo (cache + escala + subpaths) e o avanço horizontal, que em
  A09 estavam inline em `DrawMusicText`, foram extraídos para dois
  privados reutilizados por `DrawText`: `MakeGlyphShape` e
  `GetGlyphAdvance` — mesma aritmética exata de antes, sem mudança de
  comportamento em `DrawMusicText`.
- Estado do modelo de texto (caneta, alinhamento do chunk corrente, largura
  acumulada e formas pendentes) vive em membros novos do DC (`m_textPenX/Y`,
  `m_textAlignment`, `m_textChunkWidth`, `m_textChunkShapes`), zerados/realimentados
  em `StartText`. `MoveTextTo` finaliza o chunk corrente (`FinalizeTextChunk`)
  antes de mover a caneta e só troca o alinhamento guardado se o novo não for
  `HORIZONTALALIGNMENT_NONE` (replica o SVG: mesmo nó `<text>`, `text-anchor`
  só é reescrito quando um alinhamento explícito é passado). `MoveTextVerticallyTo`
  só atualiza `m_textPenY`. `EndText` finaliza o chunk.
- `DrawText`: replica a checagem de `SvgDeviceContext::DrawText` L1163-1166
  (x/y válidos sem width/height move a caneta; o caso com width/height —
  `sylTextRect` invisível — não é desenhado nem move a caneta, igual ao SVG
  não escrever x/y no `tspan` nesse ramo). Fonte SMuFL: gera glifo por
  `char32_t` com `MakeGlyphShape`/`GetGlyphAdvance` e empilha em
  `m_textChunkShapes` (não insere direto — a compensação de alinhamento só é
  conhecida ao fechar o chunk). Fonte comum: só avança a caneta via
  `GetTextExtent(chars, &extend, true)` e incrementa `m_skippedTextRuns`, sem
  gerar forma.
- `letter-spacing`: aplicado **entre** caracteres de uma mesma chamada de
  `DrawText` (não antes do primeiro), replicando a mesma regra de
  `DeviceContext::AddGlyphToTextExtend` (`if (letterSpacing != 0 && extend->m_width > 0)`).
  Não é acumulado entre chamadas diferentes dentro do mesmo chunk — no SVG é
  um atributo `letter-spacing` por `<tspan>` (por *run*), então só faz sentido
  dentro da própria chamada.
- `FinalizeTextChunk`: desloca `v.x` de cada vértice das formas pendentes por
  0 (left), `-largura/2` (center) ou `-largura` (right) — tangentes (`i`/`o`)
  não são tocadas por serem relativas — e insere cada forma com `AddShape`.
- `EndPage`: se `m_skippedTextRuns > 0`, emite `LogWarning` com a contagem e
  zera o contador (a mensagem é por página, já que `EndPage` é chamado uma
  vez por página dentro de uma composição multi-página).
- Build limpa (`make -j4` em `verovio/tools`, sem warnings novos).
- Critérios de aceite verificados com `compare/scripts/compare-page.sh`:
  - Chopin Étude Op.10 No.9 p.1: 0,48% de diff. No diff visual, o vermelho
    restante é só texto comum (título, "Allegro molto agitato.", indicações
    italianas como *cresc.*/*con forza*/*ritard.*/*a tempo*/*sotto voce*/
    *legatissimo*/*segue*/*piu cresc.*, dedilhados e números de compasso,
    rodapé "MEI engraved with Verovio") — nenhuma dinâmica SMuFL (`p`, `f`,
    `fz`, `ppp`) aparece em vermelho, ou seja, coincidem pixel a pixel com o
    SVG. Log mostra o aviso "26 common text run(s) not rendered".
  - Grieg *Butterfly* Op.43 No.1 p.1: 0,21% de diff. Mesmo padrão: só título/
    subtítulos ("Butterfly"/"Papillon"/"Sommerfugl"), números de compasso e
    rodapé em vermelho; o aviso reporta 8 trechos. Recorte lado a lado do
    `p` dinâmico do compasso 1 (crop 0,150–700,600 dos PNGs de SVG e Lottie)
    confirma coincidência visual exata do glifo.
  - Confirmado que marcas de pedal (`Ped.`), que usam um glifo SMuFL único
    via `DrawMusicText`/símbolo (não passam por `StartText`/`DrawText`), já
    funcionavam desde A09 e continuam corretas — não é uma regressão
    disfarçada de acerto.
- Varredura extra (fora do critério de aceite, para checar regressão):
  `verovio -t lottie -r verovio/data -o <prefixo> <arquivo>` rodado nos 10
  arquivos de `corpus/` (5 MEI + 5 MusicXML) sem crash/assert; cada um
  reporta sua contagem de `common text run(s)` (6 a 36, proporcional à
  quantidade de texto comum de cada partitura).
