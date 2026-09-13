# D05 — Casos de borda de estilo (opacidade, tracejado, visibilidade, cue)

**Depende de:** A06 (cor/opacidade/dash), A09/A10 (glifos/texto) ·
**Decisão necessária:** nenhuma — passo de validação e, só onde a validação
achar um bug real, correção pontual (mesmo espírito de D04).

## Objetivo

Confirmar visualmente 4 casos de borda de estilo que a implementação de A06/
A02 já suporta na **teoria** (os campos existem na IR e o writer já os
serializa), mas que **nunca foram exercitados** pelos critérios de aceite
dos passos A — porque as peças usadas até agora nesses passos não tinham
esses casos. Diferente de D02-D04, aqui a maior parte já tem cobertura real
no corpus atual (achada nesta sessão), então o trabalho é principalmente
"rodar e olhar", com correção pontual só se algo divergir.

## Ler antes (só isto)

- `verovio/include/vrv/lottiegeometry.h` L42-58 (`LottieShape`:
  `fillOpacity`, `strokeOpacity`, `dashLength`, `gapLength` já existem),
  L71-80 (`LottieNode::hidden` já existe).
- `verovio/src/lottiedevicecontext.cpp` L67-96 (`ApplyStrokeFromPen`/
  `ApplyFillFromBrush`, namespace anônimo — já leem `pen.GetOpacity()`/
  `brush.GetOpacity()` quando `HasOpacity()`), L595-621 (`StartGraphic` —
  já marca `node->hidden = true` a partir de `AttVisibility`).
- `verovio/src/lottiewriter.cpp`, `WriteFill`/`WriteStroke` (L317-344 — já
  emitem `"o"` de opacidade e `"d"` de dash), `AppendChildrenReversed`
  (L467-483 — já pula subtrees com `hidden==true`).
- Onde cada caso é **de fato acionado** por dados reais (achado nesta
  sessão, não suposição):
  - **Opacidade**: `verovio/src/view_control.cpp` L504,517,534,549
    (`dc->SetBrush(0.5, COLOR_RED)`, dentro do desenho de `<annot>`
    analítico — caixa vermelha semitransparente), L783 e
    `verovio/src/view_graph.cpp` L93,112,180,183 (`SetBrush(0.0)`/
    `SetBrush(1.0)`, opacidade 0/1 explícita em vez de `hasFill=false`).
    Corpus real: `corpus/mei/Grieg_Butterfly_Op43_No1.mei` tem `<annot>`
    (confirmar se ele renderiza por padrão ou exige uma opção de CLI —
    checar antes de assumir).
  - **Tracejado**: linha de oitava (`<octave>`) na página 4 de
    `corpus/mei/Chopin_Etude_Op10_No9.mei` — confirmado no SVG já gerado
    (`stroke-dasharray="36 72"` num `<path>` de linha horizontal,
    `compare/out/Chopin_Etude_Op10_No9-p4.svg`). Outros gatilhos de dash no
    código (`verovio/src/view_control.cpp` L623-627, L758-759, L893-935,
    L1081-1082, L3237-3239; `verovio/src/view_slur.cpp` L55-56): `<dir>`
    com linha, `lform="dashed"/"dotted"`, pedal tracejado.
  - **Visibilidade**: `corpus/mei/Chopin_Etude_Op10_No9.mei` já tem
    `visible="false"` em pelo menos um elemento (confirmado por grep).
  - **Cue**: `verovio/include/vrv/layerelement.h` L145
    (`GetDrawingCueSize`), `verovio/src/doc.cpp` L2114-2121 (`GetCueSize`/
    `GetCueScaling`, aplicado por `m_options->m_graceFactor`) — **nenhum
    código específico do `LottieDeviceContext` precisa mudar**: o
    escalonamento de notas cue/grace já acontece **antes** de chegar no
    `DeviceContext` (mesma camada de "unidades de definição" que o
    `README.md` já documenta como uniforme para todas as primitivas), então
    isto é puramente uma checagem visual, não uma lacuna de implementação
    conhecida. Corpus real: notas grace (`grace="unacc"`/`"unknown"`) em
    `Chopin_Etude_Op10_No9.mei`, `Chopin_Mazurka_Op6_No1.mei` e
    `Scarlatti_Sonata_in_C-major.mei` (confirmado por grep).

## Arquivos

Nenhum criado por padrão — só se a validação de algum item achar um bug
real, aí modificar `verovio/src/lottiedevicecontext.cpp`/`lottiewriter.cpp`
pontualmente (mesmo padrão de D04: mudança isolada, não refatoração).

## O que fazer

Para cada um dos 4 casos, gerar o `.lottie`/pacote da peça relevante,
comparar visualmente (e por `compare diff`) contra o SVG:

1. **Opacidade**: `compare/scripts/compare-page.sh
   corpus/mei/Grieg_Butterfly_Op43_No1.mei <página do annot>`. Se o corpus
   não renderizar o `<annot>` por padrão (opção de CLI necessária, ex.
   `--annotation`), criar um MEI mínimo em `compare/out/` com um `<annot
   type="score">` explícito sobre 2-3 notas, mesmo padrão de "casos de
   borda sem corpus real" dos outros passos D.
2. **Tracejado**: `compare/scripts/compare-page.sh
   corpus/mei/Chopin_Etude_Op10_No9.mei 4` — conferir visualmente que a
   linha de oitava aparece tracejada (não sólida) e com o mesmo padrão
   traço/vão do SVG.
3. **Visibilidade**: idem, na página do elemento `visible="false"` (achar a
   página certa por grep no MEI/SVG) — conferir que o elemento **não**
   aparece em nenhum dos dois PNGs (SVG já não desenha; Lottie precisa
   bater).
4. **Cue**: qualquer página com notas grace das 3 peças listadas acima —
   conferir visualmente que ficam menores que as notas normais, na mesma
   proporção do SVG (crop lado a lado).
5. Rodar `compare/scripts/compare-corpus.sh 32` de novo no fim: os 4 casos
   já estão dentro do corpus padrão (exceto talvez opacidade), então a
   varredura completa já serve como confirmação adicional sem trabalho
   extra.

## Fora de escopo

Qualquer estilo CSS não coberto por A06 (gradientes, `filter`, `clip-path`)
— fora de escopo do exportador inteiro, não só deste passo. Opções de CLI
para desenhar `<annot>` (se não vierem por padrão, é escopo de outra
decisão, não deste passo — só criar o MEI de teste mínimo é suficiente
aqui).

## Critérios de aceite

- Os 4 itens acima verificados visualmente, com nota explícita (nas notas
  de execução) de "bateu" ou "divergiu + correção aplicada" para cada um.
- Nenhuma correção, se houver, quebra os critérios de aceite já registrados
  de A06/A09/A10 (rodar de novo `compare-page.sh` nas peças de referência
  desses passos, mesmo padrão de não-regressão dos passos D anteriores).
- `compare/scripts/compare-corpus.sh 32` final sem crash, médias por página
  não pioram em relação ao que D01 já deve ter estabelecido (se D01 já
  tiver rodado antes deste passo) ou em relação a `relatorio-paridade.md`
  (se D05 rodar antes de D01).
