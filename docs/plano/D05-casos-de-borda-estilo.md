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

## Notas de execução

Os 4 casos foram verificados. Um achou um **bug real, mas na ferramenta
`compare`, não no exportador** — corrigido. Os outros 3 bateram sem
correção.

**1. Opacidade — divergiu, corrigido (na ferramenta `compare`, não no
exportador).** A premissa do plano de que `Grieg_Butterfly_Op43_No1.mei`
exercitava isso não se confirmou: o `<annot>` desse arquivo está no
`meiHead` (metadado), não em `<annot type="score">` dentro de `<music>` — o
corpus inteiro tem zero `<annot>` no corpo (confirmado por varredura). Criei
`compare/out/d05-opacidade-teste.mei` com `<annot type="score" startid=...
endid=...>` sobre um acorde, que aciona `View::DrawAnnotScore`
(`dc->SetBrush(0.5, COLOR_RED)`). Comparando os PNGs, a caixa saía visivelmente
mais escura/acinzentada no Lottie que no SVG (rosa "empoeirado" vs.
rosa-salmão vivo). Investigação por pixel bruto (RGBA, não só o crop
composto): SVG dava `(255,0,0,128)` (alpha reto), Lottie dava `(127,0,0,127)`
— um padrão clássico de **alpha premultiplicado** sendo tratado como reto.
Causa raiz: `compare/src/main.rs` (`lottie_to_png`/`sm_render`) pedia o
buffer do ThorVG com `ColorSpace::ARGB8888`, que a própria documentação do
ThorVG (`thorvg.h`) marca como premultiplicado, e escrevia os canais R/G/B
direto no PNG como se fossem retos. Corrigido trocando para
`ColorSpace::ARGB8888S` (variante reta, já disponível na revisão vendorizada
do `dotlottie-rs`) nos dois pontos que chamam `set_sw_target`. Depois da
correção: `(255,0,0,127)` — igual ao SVG a menos de 1 de arredondamento no
alpha. **Não era bug do exportador** — `fillOpacity`/`strokeOpacity` já
saíam corretos do `LottieDeviceContext`/`LottieWriter`; só a extração do
buffer de pixels do `compare` estava incompatível com o color space pedido.
Documentado em `compare/README.md` como nova "pegadinha" (mesmo padrão de
D01-2/D01-3). `compare-corpus.sh 32` não mudou (0,0196%–0,5212%/média
0,2290%, idêntico à baseline de D02) porque nenhuma peça real do corpus tem
preenchimento com opacidade fracionária — o bug só aparece em casos de
borda como este.

**2. Tracejado — bateu, sem correção.** Linha de oitava (`<octave>`) na
página 4 de `Chopin_Etude_Op10_No9.mei`: mesmo padrão traço/vão (`36 72`) e
mesmo gancho vertical no fim da linha, nos dois lados. Diff da página:
0,0728% (dentro da faixa normal do corpus).

**3. Visibilidade — bateu, sem correção, mas a premissa do plano também não
se confirmou.** Os 4 hits de `grep 'visible="false"'` em
`Chopin_Etude_Op10_No9.mei` são todos `bracket.visible="false"` em
`<tupletSpan>` — atributo de `AttTupletVis` (controla só o colchete do
tuplet), não de `AttVisibility`/`@visible`, que é o que
`LottieDeviceContext::StartGraphic` e `SvgDeviceContext::StartGraphic`
realmente leem. O corpus inteiro tem zero ocorrências reais de `@visible` em
elemento que tenha `AttVisibility` (`note`, `chord`, `clef`, `keySig`,
`meterSig`, `stem`, `barLine`, `divLine`, `layer`, `mRest`, `meterSigGrp`,
`staff`). Criei `compare/out/d05-visibilidade-teste.mei` (acorde de 3 notas,
a do meio com `visible="false"`) para exercitar de verdade o código: a nota
do meio não aparece em nenhum dos dois PNGs, diff 0,2311% (ruído de
antialiasing disperso, sem mancha estrutural — inspecionado via imagem de
diff). **Achado colateral, fora de escopo, não corrigido:** o CLI
`--show-hidden` (que faz `Clef`/`KeySig`/`MeterSig` e a visibilidade CSS do
SVG mostrarem elementos ocultos, para depuração) só é conectado à
`SvgDeviceContext` (`toolkit.cpp`, `RenderToSVG`) — nenhum caminho de
`RenderToLottie`/`RenderToDotLottieFile` propaga essa opção pro
`LottieDeviceContext`, que sempre omite elementos com `visible="false"`
independente da flag. Nos testes deste passo (sem `--show-hidden`, que é o
uso normal) o comportamento bate; a lacuna só importa se o zywny algum dia
precisar de um modo de depuração equivalente ao `--show-hidden` do SVG.

**4. Cue — bateu, sem correção.** Notas grace (`grace="unknown"`) em
`Scarlatti_Sonata_in_C-major.mei`, página 1 (localizadas via
`scale(0.54, 0.54)` no SVG — 0,72 × `m_graceFactor` padrão de 0,75):
tamanho, posição e beam idênticos aos do SVG. Diff da página: 0,0967%.

**Não-regressão:** `compare-corpus.sh 32` rodado no fim (após a correção do
`compare`) deu 0,0196%–0,5212%/média 0,2290% em 34 páginas — idêntico à
baseline registrada em D02, sem regressão.
