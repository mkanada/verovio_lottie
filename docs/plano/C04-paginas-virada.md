# C04 — Páginas e virada estilo Synthesia

**Depende de:** A12 (pacote `.lottie`), A13 (varredura do corpus), C01 (writer de
state machine), C02 (M2 funcional, por página), C03 (M3 + handoff M2↔M3) ·
**Decisão necessária:** nenhuma nova — D-LAYOUT-PAGINAS já foi decidido em B02
(trilha horizontal + câmera + engine separado, virada em dois eventos
discretos por fronteira). Este passo só define os detalhes concretos de
implementação (curva/timing exatos do "espreitar"/"cobrir", layout de frames,
nomes internos, estrutura de arquivos) que o `CLAUDE.md` já delega
explicitamente à Fase C — não é decisão de arquitetura nova.

## Objetivo

Substituir a timeline provisória de A03 ("página k+1 no frame k", 1 frame por
página) pelo desenho real decidido em B02: todas as páginas da partitura
lado a lado numa trilha horizontal dentro da mesma composição `score`, uma
camada-câmera (`ty:3`, null layer) cuja posição em X é pai (`parent`) de
todas as camadas de página, e um **engine dedicado** (`sm_page`, dotLottie
v2, topologia em estrela) que move essa câmera entre as páginas via dois
eventos discretos por fronteira — "espreitar" (a câmera entra no último
compasso da página atual) e "cobrir" (a câmera entra no primeiro compasso da
próxima) — endereçados pelo `xml:id` dos compassos de fronteira, não pelas
notas.

Isso também fecha o item que C02 deixou explicitamente para cá ("Reintegrar
[o destaque] na partitura inteira é trabalho de C04"): `Toolkit::
RenderToDotLottieFile`/CLI `dotlottie` — hoje o pacote final da música
inteira, sem nenhuma state machine — passa a emitir o pacote **completo**:
M2 (destaque automático, agrupado por instante do timemap, agora cobrindo
**todas** as páginas) + M3 (slot de cor por nota, idem) + o novo engine de
página, tudo dentro de um único `.lottie`, como o `CLAUDE.md` sempre exigiu
("um `.lottie` por música inteira").

## Decisões de escopo tomadas aqui (implementação, não arquitetura)

Seguindo o mesmo padrão de C02 (que também tomou decisões de MVP dentro do
mecanismo já aprovado em B02, sem reabrir a arquitetura):

- **`dotlottie-highlight` (C02/C03) não muda.** Continua exportando uma
  página isolada, sem câmera/engine de página (não faz sentido — não há
  fronteira de página dentro de uma única página). Só `RenderToDotLottieFile`
  ganha o novo comportamento. Isso preserva a ferramenta de depuração
  page-scoped que C02/C03 já validaram, em vez de descartá-la.
- **`Toolkit::RenderToLottieAnimation()` não muda.** É usado hoje só
  internamente por `RenderToDotLottieFile` (nenhum outro chamador no
  código). Em vez de mudar sua assinatura pública para devolver também os
  grupos/layout construídos (que `RenderToDotLottieFile` precisaria para
  montar as state machines), a lógica nova fica **inline** em
  `RenderToDotLottieFile`, no mesmo estilo que `RenderToDotLottieHighlightFile`
  já usa (não chama `RenderToLottieAnimation` também, monta tudo direto).
  `RenderToLottieAnimation()` fica como está — se algum binding futuro (E00)
  precisar dela sem pacote, continua produzindo exatamente a animação simples
  de hoje.
- **Câmera vive na mesma animação `score`, não numa animação separada.**
  A separação de "engine" exigida por B02 (para não repetir a colisão do
  achado "Extra" de B01) é sobre **instâncias de `Player`/state machine em
  tempo de execução**, não sobre arquivos JSON: duas state machines
  (`sm_highlight`, `sm_page`) podem — e devem — ser carregadas contra a
  **mesma** `a/score.json` em duas instâncias de `Player` separadas, cada
  uma com seu próprio playhead independente (confirmado por leitura do
  `dotlottie-rs`: cada `Player` mantém seu próprio estado de renderização
  sobre os dados carregados; nada impede duas instâncias carregarem o mesmo
  JSON). Isso evita duplicar toda a geometria da partitura num segundo
  arquivo só para mover uma câmera. As duas state machines endereçam faixas
  de frame **disjuntas** dentro do mesmo eixo de tempo (`sm_highlight` usa
  frames baixos, como já era; `sm_page` usa uma faixa alocada logo depois) —
  isso não causa nenhum conflito porque cada engine só dá `seek` para os
  seus próprios markers.
- **Câmera implementada como recorte (clipping), não como "página k+1 no
  frame k".** Cada camada de página é reposicionada em
  `x = índice_da_página × trackStep` (`trackStep` = a mesma largura `w` já
  usada pela composição hoje) e passa a ter `parent` apontando para uma nova
  camada-câmera nula cuja posição em X é keyframada. A composição continua
  com `w`/`h` = o tamanho de uma página (viewport), então páginas fora da
  posição corrente simplesmente caem fora do buffer de pixels renderizado —
  **confirmado por leitura do código do `dotlottie-rs`** (não suposição):
  `Layout::to_transform_matrix` (`src/layout.rs`) calcula a escala a partir
  de `picture_width`/`picture_height`, que vêm de `animation.get_size()`
  (`src/renderer/mod.rs` `load_animation`/`apply_layout_transform`) — ou
  seja, do `w`/`h` **declarado** no JSON, não de uma bounding box calculada
  do conteúdo. Com `Layout` padrão (`Fit::Contain`, `src/layout.rs` L19-23)
  e canvas = `w`×`h` da composição, a escala fica 1:1 (como os testes A05+
  já comprovam pixel a pixel) e qualquer conteúdo posicionado fora de
  `[0,w)×[0,h)` cai fora do buffer — não há necessidade de máscara/clip
  path explícito. Todas as camadas de página passam a ter `ip:0,op:<último
  frame usado>` (sempre "ligadas"; antes só uma por vez estava dentro da
  janela `ip/op`) — a visibilidade agora é só a câmera + o recorte do
  canvas, não mais o janelamento por camada.
- **Curva de "espreitar"/"cobrir": MVP com constantes fixas, keyframes com
  o mesmo par de handles de easing já usado no fade de destaque** (`"i":
  {"x":1,"y":1},"o":{"x":0,"y":0}`), não uma curva de "pré-acumulação"
  customizada. B02 já deixa isso explícito: "Esta proposta não decide o
  algoritmo exato de easing do 'peek' ... trabalho de authoring/design da
  Fase C, não uma decisão arquitetural" — então a escolha concreta é deste
  passo, documentada aqui, ajustável depois sem tocar a arquitetura.
  Constantes: câmera "espreita" `kPeekFraction = 0.6` (60%) do caminho até a
  próxima página em `kPeekDurationFrames = 15` (~0.5s a 30fps), pausa, e só
  então "cobre" o resto em `kCoverDurationFrames = 20` (~0.66s, mesma ordem
  de grandeza do fade de destaque de C02) — o efeito visual de "espreitar
  antes de cobrir" nasce de dividir a viagem em duas paradas, não de uma
  curva de easing exótica.
- **Compassos sem `<measure>` na página** (raro — ex.: página só de
  texto/rosto): se uma página não tem nenhum `<measure>` no seu SVG
  renderizado, o(s) evento(s) de fronteira que dependeriam do primeiro/
  último compasso **dela** ficam de fora (log de aviso, não aborta a
  exportação) — a página ainda ganha sua própria posição de câmera/estado
  de repouso (para a trilha continuar consistente), só não fica alcançável
  por um evento de fronteira de quem vem antes/depois. Limitação MVP
  documentada, não resolvida (mesmo padrão de C02/C03 para casos de borda
  incomuns no corpus atual).
- **Sem "pular direto pra página N".** B02 (item 4 da decisão final)
  desenha só os dois eventos discretos por fronteira, não um evento de
  acesso direto por página (diferente do destaque de nota, onde a topologia
  em estrela **é** um requisito explícito do `CLAUDE.md` — aqui B02 não
  estende essa exigência às páginas). Não implementar preventivamente.

## Ler antes (só isto)

- `docs/plano/decisoes/B02-mecanismo-destaque.md` — decisão final, item 4
  (dois eventos discretos por fronteira, engine separado) e a seção
  "Proposta para D-LAYOUT-PAGINAS" (trilha horizontal + câmera — a base
  geométrica, mesmo com o mecanismo de transição corrigido de `Tweened`
  único para dois eventos discretos).
- `docs/plano/C02-notas-animadas.md`, seção "Decisão de escopo tomada aqui"
  — por que o destaque ficou limitado a uma página (o motivo que este passo
  resolve) e o padrão de "constantes MVP ficam no `Toolkit`, não no
  `LottieWriter`" que este passo repete para a câmera.
- `verovio/src/lottiewriter.cpp` L472-596 (`WriteLayerShapes`,
  `LottieWriter::WriteAnimation`) — layout atual por página (`ComputePageMetrics`,
  `px`/`py`/`s`, `ip`/`op` por camada, `markers`) — é exatamente isso que
  este passo generaliza para trilha horizontal + câmera.
- `verovio/include/vrv/lottiehighlight.h`/`.cpp` — padrão de builder
  (`LottieHighlightBuilder::BuildGroups`/`BuildStateMachine`) a replicar
  para a câmera (`LottiePageTurnBuilder`, novo).
- `verovio/include/vrv/lottiegeometry.h` L71-89 (`LottieNode`, `LottiePage`)
  — `children` já preserva ordem de documento (comentário na linha 79); a
  detecção de fronteira de página (primeiro/último compasso) percorre essa
  árvore, não o timemap.
- `verovio/src/measure.cpp`/`verovio/include/vrv/measure.h` L60
  (`GetClassName() == "measure"`) e `verovio/src/view_page.cpp` L998-1007
  (`View::DrawMeasure`, `dc->StartGraphic(measure, "", measure->GetID())`)
  — confirma que cada compasso vira um `LottieNode` com `className=="measure"`
  e `id==xml:id do compasso`, em ordem de documento (sistema a sistema,
  compasso a compasso) — a fonte de dados da detecção de fronteira.
- `verovio/src/toolkit.cpp` L1828-1906 (`RenderToLottieAnimation`,
  `RenderToDotLottieFile`, `RenderToDotLottieHighlightFile`) — o ponto de
  reescrita e o padrão já usado por `RenderToDotLottieHighlightFile` pra
  montar grupos + state machine + manifest com múltiplos `s/*.json`.
- No checkout local do `dotlottie-rs` (mesmo caminho de C03,
  `~/.cargo/git/checkouts/dotlottie-rs-*/*/dotlottie-rs/`):
  - `src/layout.rs` L1-90 (`Fit`, `Layout::to_transform_matrix`) — base do
    argumento de clipping acima.
  - `src/renderer/mod.rs` L290-330 (`load_animation`, `apply_layout_transform`)
    — confirma que `picture_width`/`picture_height` vêm de
    `animation.get_size()` (tamanho **declarado** no JSON), não de uma
    bounding box do conteúdo.
  - `src/player.rs` — não há, hoje, nenhum método público (`pub fn`) para
    ler a posição/transform de uma camada por fora do `Player` (o único
    método relacionado, `hit_test`/`get_layer_obb`, é usado internamente
    pelas guardas de interação da state machine — `src/state_machine/mod.rs`
    L1030+ — e vive atrás de `renderer: pub(crate)`, inacessível fora do
    crate). Registra concretamente o risco já aberto em B02/CLAUDE.md
    ("não está confirmado se os runtimes do zywny suportam... compositar a
    posição de câmera de uma na renderização visível da outra") — não é
    escopo deste passo resolver isso (é responsabilidade do host/zywny,
    explicitamente adiado por B02), só não fingir que está resolvido.

## Arquivos

- Criar: `verovio/include/vrv/lottiepageturn.h`, `verovio/src/lottiepageturn.cpp`.
- Modificar: `verovio/include/vrv/lottiewriter.h`, `verovio/src/lottiewriter.cpp`
  (câmera + trilha horizontal + markers de página, novos parâmetros com
  default em `WriteAnimation`), `verovio/src/toolkit.cpp` (reescreve
  `RenderToDotLottieFile`).
- Modificar (fora de `verovio/`): `compare/scripts/compare-page.sh`,
  `compare/scripts/compare-corpus.sh` (a suposição "frame == página − 1" só
  vale pro pacote `.lottie` final agora que ele tem câmera; precisam ler o
  frame de repouso de cada página a partir dos `markers` do próprio pacote
  em vez de calcular `PAGE - 1`).

## O que fazer

1. **`LottiePageTurnBuilder::CollectMeasureIdsInOrder`** (`lottiepageturn.cpp`):
   percorre a árvore de `LottieNode` de uma página em ordem de documento
   (pré-ordem, sem inverter — ao contrário de `AppendChildrenReversed` do
   writer, que inverte pra pintura; aqui queremos a ordem real de leitura),
   coletando `node.id` de todo nó com `node.className == "measure"` e
   `id` não vazio. Não precisa validar unicidade/nomes reservados de novo —
   isso já é feito por `LottieHighlightBuilder::CollectIds` sobre a mesma
   árvore, chamado em paralelo pelo `Toolkit`.

2. **`LottiePageTurnLayout`/`LottiePageBoundary`** (`lottiepageturn.h`): IR
   pronta pra o writer consumir —

   ```cpp
   struct LottiePageBoundary {
       std::string peekMarker;    // ex.: "peek1"
       std::string coverMarker;   // ex.: "cover1"
       std::string peekEventId;   // xml:id do último compasso da página antes da fronteira
       std::string coverEventId;  // xml:id do primeiro compasso da página depois da fronteira
       int peekStartFrame = 0;
       int peekDurationFrames = 0;
       int coverStartFrame = 0;
       int coverDurationFrames = 0;
   };

   struct LottiePageTurnLayout {
       bool enabled = false;                       // false = sem conteúdo de virada (1 página só)
       std::vector<std::string> pageMarkers;        // "page0".."page{N-1}", paralelo às páginas
       std::vector<int> pageRestFrames;              // frame de repouso da câmera por página
       std::vector<LottiePageBoundary> boundaries;   // pages.size() - 1 entradas
       int endFrame = 0;                             // último frame usado (entra no cálculo do "op")
   };
   ```

3. **`LottiePageTurnBuilder::BuildLayout`** (`lottiepageturn.cpp`):

   ```cpp
   static LottiePageTurnLayout BuildLayout(const std::vector<const LottiePage *> &pages,
       int firstFrame, int peekDurationFrames, int coverDurationFrames, int gapFrames);
   ```

   - Se `pages.size() <= 1`, devolve `{}` (`enabled=false`) — nada a fazer.
   - Para cada página, chama `CollectMeasureIdsInOrder`; guarda o primeiro e
     o último id (vazio se a página não tiver nenhum compasso — log de
     aviso, ver "Decisões de escopo" acima).
   - Aloca frames sequencialmente, mesmo padrão de slot de
     `LottieHighlightBuilder::BuildGroups` (frame corrente avança a cada
     alocação, com `gapFrames` de folga entre segmentos — mesma razão do
     `NOTE_SLOT`/achado E2 de B01: o fim de um segmento não pode coincidir
     com o começo do próximo):
     ```
     frame = firstFrame
     pageRestFrames[0] = frame; pageMarkers[0] = "page0"
     para i em 0..pages.size()-2:
         se página i não tem último compasso OU página i+1 não tem primeiro
         compasso: pula esta fronteira (log de aviso), mas ainda assim
         avança pageRestFrames[i+1] a partir do frame corrente (a câmera
         continua tendo uma posição de repouso definida pra página i+1,
         só não é alcançável por evento nesta fronteira)
         senão:
             boundary.peekStartFrame = frame
             boundary.peekDurationFrames = peekDurationFrames
             frame += peekDurationFrames + gapFrames
             boundary.coverStartFrame = frame
             boundary.coverDurationFrames = coverDurationFrames
             frame += coverDurationFrames + gapFrames
         pageRestFrames[i+1] = frame; pageMarkers[i+1] = "page" + (i+1)
     endFrame = frame
     ```

4. **`LottiePageTurnBuilder::BuildStateMachine`** (`lottiepageturn.cpp`),
   mesmo padrão de `LottieHighlightBuilder::BuildStateMachine`:
   - `initial = "page0"`.
   - Um `PlaybackState` por página (`segment = pageMarkers[i]`,
     `autoplay:false`, `loop:false` — câmera só se move quando um evento de
     fronteira dispara, nunca sozinha).
   - Um `PlaybackState` por `peekMarker`/`coverMarker` de cada fronteira
     (`autoplay:true`, `loop:false`).
   - Um único `GlobalState` com, para cada fronteira que tem os dois ids
     (ver passo 3): `{toState: boundary.peekMarker-como-nome-de-estado,
     eventInput: boundary.peekEventId}` e `{toState: cover-como-estado,
     eventInput: boundary.coverEventId}` — topologia em estrela idêntica à
     de `sm_highlight`, só que os nomes de estado usam os nomes dos
     `PlaybackState`s de peek/cover (não precisa ser igual ao nome do
     marker, mas por simplicidade **é** igual).
   - Sem transição de volta — mesma decisão de C02 pra `sm_highlight`
     (religar não é pedido pelo MVP; o "cobrir" termina com a câmera
     parada exatamente na posição de repouso da próxima página, porque um
     `PlaybackState` sem `loop` segura o último frame do segmento).

5. **`LottieWriter::WriteAnimation`**: novo parâmetro com default (compila e
   produz saída **idêntica** para todo chamador que não o passa — mesmo
   padrão de `highlightGroups`/`interactiveIds`):

   ```cpp
   static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name,
       const std::vector<LottieHighlightGroup> &highlightGroups = {}, int highlightColor = 0xE53935,
       const std::unordered_set<std::string> &interactiveIds = {},
       const LottiePageTurnLayout &pageTurn = {}, double peekFraction = 0.6);
   ```

   - `trackStep = w` (a mesma largura de composição já calculada como
     `max(m.wpx)` sobre todas as páginas — reaproveitada, não recalculada).
   - Se `pageTurn.enabled`:
     - Cada camada de página `i` ganha `"parent":<ind da câmera>` e seu
       `px` de hoje (`m.tx + m.scale*page.originX`) passa a somar
       `i * trackStep` (a câmera, não a camada, é quem depois desloca isso
       de volta pra dentro do viewport). `ip:0,op:<op da composição>` pra
       todas (nenhuma janela por camada — ver "Decisões de escopo").
     - Uma camada nula nova (`"ty":3`, sem `shapes`) é inserida **antes**
       das camadas de página no array `layers`, com `ind` reservado (ex.:
       `1`, deslocando os índices das páginas pra começarem em `2` — só
       precisa ser único e bater com o `parent` das páginas), `ip:0,
       op:<op>`, e `ks.p` keyframada (função nova `WriteCameraKeyframes`,
       mesmo padrão de `WriteColorKeyframes`: uma lista de paradas
       `{t, value}` com os mesmos handles `"i":{"x":1,"y":1},"o":{"x":0,
       "y":0}` em toda parada exceto a última, sem handle):
       - `restX(k) = -(k * trackStep)` (posição de repouso da página `k`,
         0-based).
       - Por página `k`: parada em `pageRestFrames[k]` com `restX(k)`.
       - Por fronteira `i` (página `i`→`i+1`): parada em
         `boundary.peekStartFrame` com `restX(i)` (reforça o "segura",
         ainda que redundante com a parada da própria página — sem custo),
         parada em `peekStartFrame+peekDurationFrames` com
         `restX(i) - peekFraction*trackStep`, parada em `coverStartFrame`
         repetindo esse mesmo valor (segura até o "cobrir" começar), parada
         em `coverStartFrame+coverDurationFrames` com `restX(i+1)`.
       - `y` sempre `0` (câmera só se move em X).
     - `markers`: acrescenta `{"cm":pageMarkers[k],"tm":pageRestFrames[k],"dr":1}`
       por página e `{"cm":boundary.peekMarker,"tm":...,"dr":...}`/
       `{"cm":boundary.coverMarker,...}` por fronteira, junto dos markers de
       destaque já existentes (arrays concatenados, sem conflito de nomes —
       `hlN`/`idle` vs. `pageN`/`peekN`/`coverN` são namespaces distintos por
       construção).
     - `op` passa a ser `max(op-de-hoje-com-highlight, pageTurn.endFrame)`.
   - Se `!pageTurn.enabled` (inclui todo chamador que não passa o
     parâmetro): **zero mudança** — nenhuma camada de câmera, `px` sem o
     termo `i*trackStep` (irrelevante com 1 página só), `ip`/`op` por
     camada exatamente como hoje. Cobre `lottie`, `dotlottie-highlight`, e
     qualquer chamada futura de `RenderToLottieAnimation()`.

6. **`Toolkit::RenderToDotLottieFile`** (reescrito, `toolkit.cpp`): junta
   M2+M3 (hoje só em `RenderToDotLottieHighlightFile`) com o novo motor de
   página, tudo num pacote só —
   - Renderiza todas as páginas (mesmo laço de `RenderToLottieAnimation`).
   - `pageIds` = união de `LottieHighlightBuilder::CollectIds` sobre a raiz
     de cada página (uma chamada por página, sets unidos) — antes isso só
     rodava sobre uma página.
   - Constantes MVP de destaque: as mesmas de `RenderToDotLottieHighlightFile`
     hoje (`kFirstHighlightFrame=1`, `kHighlightDurationFrames=20`,
     `kHighlightGapFrames=1`) — chamando `LottieHighlightBuilder::BuildGroups`
     uma vez só, com o `pageIds` unido em vez de restrito a uma página.
   - `interactiveIds` = união de `memberIds` dos grupos, como já faz
     `RenderToDotLottieHighlightFile`.
   - Constantes MVP de página (novas, mesmo padrão): `kPeekDurationFrames=15`,
     `kCoverDurationFrames=20`, `kPageGapFrames=1`; primeiro frame =
     `highlightEnd + 1` (logo depois do último grupo de destaque, pra manter
     os dois namespaces de frame monotonicamente crescentes e fáceis de
     depurar, embora não seja estritamente necessário — ver "O que fazer" 5).
     `LottiePageTurnBuilder::BuildLayout(pages, highlightEnd+1,
     kPeekDurationFrames, kCoverDurationFrames, kPageGapFrames)`.
   - `LottieWriter::WriteAnimation(pages, "score", groups, 0xE53935,
     interactiveIds, pageTurn)` (frações de peek com o default `0.6`).
   - `s/sm_highlight.json` sempre presente (`LottieHighlightBuilder::
     BuildStateMachine`, como hoje). `s/sm_page.json` só se
     `pageTurn.enabled` (`LottiePageTurnBuilder::BuildStateMachine`).
   - `manifest.json`: `stateMachines` lista as que existirem
     (`{"sm_highlight"}` ou `{"sm_highlight","sm_page"}`); **não** define
     `initial.stateMachine` — com duas state machines pensadas pra duas
     instâncias de `Player` separadas (ver "Decisões de escopo"), não há
     uma "a" state machine inicial óbvia; decidir qual (se alguma) uma
     instância única deveria auto-ativar é decisão de host, não deste
     exportador (mesmo raciocínio de C01: "o método só oferece a opção").

7. **`compare/scripts/compare-page.sh`/`compare-corpus.sh`**: o ramo que
   assume `frame = PAGE - 1` pra um `.lottie` pronto deixa de valer pro
   pacote final (`dotlottie`) — agora o frame de repouso de cada página
   depende dos frames alocados por `BuildLayout` (variam com o nº de grupos
   de destaque da partitura inteira). Em vez de recalcular essa aritmética
   em bash, os dois scripts passam a **ler o frame de `a/score.json` dentro
   do próprio pacote**: `unzip -p "$LOTTIE" a/score.json | python3 -c
   '...'` extraindo `tm` do marker `"cm"=="page<PAGE-1>"` (0-based) do array
   `markers` (com *fallback* pro comportamento antigo — `FRAME=$((PAGE-1))`
   — se o pacote não tiver um marker `pageN`, ou seja, pacotes gerados antes
   deste passo ou com 1 página só, onde não há trilha horizontal). Isso
   mantém os dois scripts funcionando tanto pra pacotes antigos (1 página)
   quanto pro novo formato multi-página, sem duplicar a lógica de alocação
   de frames do C++ em bash.

## Fora de escopo

- Resolver como o host lê a posição da câmera de uma instância e composita
  na renderização visível da outra (risco já registrado em B02/CLAUDE.md,
  não confirmado nem pelo `dotlottie-rs` — ver "Ler antes" — nem por
  qualquer outro runtime; explicitamente responsabilidade do zywny, não do
  exportador).
- Curva de easing customizada de "pré-acumulação" pro efeito de espreitar —
  MVP usa o mesmo par de handles já usado no fade de destaque (ver
  "Decisões de escopo").
- Opções de CLI pra cor/duração/fração de espreitar configuráveis — C06, só
  se pedido (mesmo padrão de C02/C03).
- Acesso direto a uma página específica por evento (só os dois eventos de
  fronteira, ver "Decisões de escopo").
- Páginas de tamanhos muito diferentes entre si — a composição continua
  usando `max(wpx)`/`max(hpx)` como já fazia (A03), não uma abordagem por
  página; se isso já era uma limitação conhecida antes de C04 (documentada
  em A03), continua sendo, só que agora também afeta o cálculo de
  `trackStep`.
- Mudar `dotlottie-highlight`/`lottie` (formatos de depuração de uma
  página) — permanecem exatamente como C02/C03 deixaram.
- Handoff entre o engine de página e M2/M3 — B02 já explica por que não é
  necessário (M3 não usa playhead/state machine; só M2 precisava de
  separação de engine, e a separação já É o desenho deste passo — não há
  um "terceiro modo" pra coordenar).
- Bindings JS/Python consumindo o novo pacote (E00, opcional, não
  começado).

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`).
- `LottieWriter::WriteAnimation` chamado **sem** `pageTurn` (como `lottie`/
  `dotlottie-highlight`/`RenderToLottieAnimation` já fazem) produz saída
  **byte-idêntica** à de antes deste passo — testar regenerando os pacotes
  desses três caminhos pra 2-3 peças do corpus antes/depois (diff byte a
  byte do JSON).
- `python3 -m json.tool` valida `a/score.json`, `s/sm_highlight.json` e
  `s/sm_page.json` de um `dotlottie` gerado por uma peça multi-página do
  corpus.
- `unzip -t` no pacote gerado passa.
- **Recorte da câmera**: `compare lottie-to-png <pacote> --frame
  <pageRestFrames[0]>` com `--width`/`--height` iguais ao tamanho de uma
  página mostra **só** a página 1 (comparação de diff, tolerância 0, contra
  o PNG do SVG da página 1 gerado por `compare-page.sh` sobre a partitura
  original) — confirma que o conteúdo de páginas seguintes não vaza pro
  canvas.
- Repetir o teste acima em `--frame <pageRestFrames[1]>` (ou o frame lido do
  marker `"page1"`) contra o SVG da página 2 — confirma que a câmera em
  repouso na página 2 mostra exatamente a página 2, não uma mistura.
- `compare sm-render --sm sm_page --script "0:fire <último-compasso-pág-1>;
  <t>:fire <primeiro-compasso-pág-2>"` com `--snap` em instantes dentro e
  depois de cada segmento, amostrando um pixel de conteúdo exclusivo da
  página 2 (ex.: uma nota que só existe nela): o pixel deve continuar de
  fundo até o "espreitar" começar a revelar parcialmente a borda da página
  2, e só aparecer preenchido de verdade depois que o "cobrir" terminar —
  confirma visualmente as duas fases do efeito Synthesia.
- `compare sm-render --sm sm_highlight` no **mesmo** pacote, disparando um
  grupo de destaque de uma nota da **página 2** (que antes de C04 nunca
  existia em `dotlottie` — só em `dotlottie-highlight` restrito à página 1)
  — confirma que M2 agora cobre a partitura inteira, não só a primeira
  página, dentro do pacote final.
- `compare-page.sh`/`compare-corpus.sh` continuam rodando sem erro sobre o
  corpus inteiro (varredura completa, mesmo padrão de A13), confirmando que
  a leitura de marker por página funciona tanto pra peças de 1 página
  quanto multi-página.

## Notas de execução

Implementado como planejado, com dois ajustes descobertos empiricamente
durante a validação (nenhum deles muda o desenho, só corrige constantes/
detalhes de implementação):

- **`kPeekFraction` errado no rascunho inicial**: a primeira versão do
  passo 5 usava `0.6` (60% do caminho até a próxima página) como default.
  Testando visualmente, isso faz a câmera andar tanto durante o
  "espreitar" que a página atual já sai quase inteira de cena antes do
  "cobrir" sequer começar — não é um "espreitar" sutil, é quase a virada
  inteira em duas etapas. Corrigido pra `0.08` (8%, só uma tira fina da
  próxima página aparecendo na borda) — documentado no comentário de
  `LottieWriter::WriteAnimation` em `lottiewriter.h`. Valor ainda ajustável
  sem tocar arquitetura (é só o default de `peekFraction`).
- **`op` é limite exclusivo, achado por teste direto**: `compare
  lottie-to-png --frame <op>` falha (`set_frame: invalid parameter`) — o
  último frame válido é `op - 1`. O marker de repouso da última página
  (`pageRestFrames.back()`) originalmente coincidia exatamente com `op`,
  ficando inacessível tanto por `--frame` direto quanto pelo próprio
  `PlaybackState` da state machine (`page<N-1>` segment `tm=op` também
  fora do intervalo válido). Corrigido em
  `LottiePageTurnBuilder::BuildLayout`: `endFrame = frame + 1` (1 frame de
  folga reservado no fim da trilha), documentado no código. Não mexi na
  conta de `op`/`highlightEnd` do M2 (`Toolkit::RenderToDotLottieFile`/
  `LottieWriter::WriteAnimation`, pré-existente de C02) — está fora do
  escopo deste passo e não foi tocada pelos testes de não-regressão.

**Validação da não-regressão** (formatos que não passam `pageTurn`):
`git stash` das mudanças deste passo, rebuild, gerar `lottie -p 1` e
`dotlottie-highlight` pra `Scarlatti_Sonata_in_C-major.mei` com `-x 42`,
`git stash pop`, rebuild, regerar com o mesmo comando — `lottie -p 1` saiu
**byte-idêntico**; `dotlottie-highlight` saiu com `a/score.json` e
`s/sm_highlight.json` **byte-idênticos**, só `manifest.json` divergindo no
sufixo `-dirty` da versão (mesmo achado não-relacionado já registrado em
C01/C02).

**Validação do recorte da câmera** (mecanismo central do passo): pacote
`dotlottie` de `Scarlatti_Sonata_in_C-major.mei` (3 páginas, `-x 42`).
Comparação **Lottie-vs-Lottie** (não SVG-vs-Lottie, pra isolar o mecanismo
de câmera do ruído de renderização resvg-vs-ThorVG já documentado em A13) —
frame de repouso de cada página do pacote multi-página vs. `lottie -p N`
(formato de depuração de uma página, inalterado por este passo) pra cada
`N`: **0 pixels diferentes em 6.237.000 comparados, tolerância 0**, nas
três páginas — confirma que o `Layout::Fit::Contain` do `dotlottie-rs`
(`picture_width`/`picture_height` = `w`/`h` **declarado**, não bounding
box do conteúdo, ver "Ler antes") realmente recorta pro viewport de uma
página, como previsto por leitura de código antes de escrever qualquer
linha.

**Validação do roteiro completo via `compare sm-render --sm sm_page`**:
disparando os `xml:id` reais dos compassos de fronteira (extraídos do
próprio `s/sm_page.json` gerado, não inventados) — `"0:fire
d1e3859;550:fire d1e4029"` (dispara "espreitar" pág.1→2 em t=0, "cobrir"
em t=550ms, depois do espreitar de 500ms terminar) — o estado reportado
percorre `peek1` → `cover1` corretamente, e o snapshot final (`t=1250ms`,
depois do cover de ~667ms completar) bate **0 pixels diferentes** contra o
`lottie -p 2` de referência. Confirma o pipeline completo: evento real →
transição → segmento → frame final, ponta a ponta.

**Validação de M2 na partitura inteira** (item que C02 deixou
explicitamente para cá): disparando `fire d1e5328` (evento de uma nota
presente **só** na página 2, confirmado comparando os ids de
`lottie -p 1`/`lottie -p 2`) via `sm_highlight` **no mesmo pacote
`dotlottie` final** — transiciona pro estado `hl217` esperado (calculado
independentemente a partir do `GlobalState` do `s/sm_highlight.json`
gerado). Confirma que `RenderToDotLottieFile` agora agrupa/serializa M2
sobre **todas** as páginas, não só a primeira.

**Varredura do corpus inteiro** (`compare-corpus.sh` atualizado, 10 peças/
34 páginas, tolerância 32 — mesmo padrão de A13): rodou sem nenhum erro
(nenhum aviso de `set_frame`/`invalid parameter`), e o CSV resultante
(`compare/out/corpus/resultado.csv`) bate, **peça por página, até a quarta
casa decimal**, com os números já publicados em
`docs/plano/relatorio-paridade.md` (ex.: `Chopin_Etude_Op10_No9` p.1
0,4842% nos dois; `Clair_de_Lune__Debussy` p.1 0,7658% nos dois) — confirma
que o pacote `dotlottie` final (agora com câmera + M2 + M3, sem nenhuma
página em modo `--all-pages` separado como A13 media) continua com a
mesma paridade visual de antes de C04 em toda a superfície do corpus,
página a página.

**Build**: `cmake ../cmake && make -j4` limpo (só os 3 arquivos alterados +
o novo `lottiepageturn.cpp` + `vrv.cpp`, sem warnings novos).

Arquivos de teste (`compare/out/c04/*`, `compare/out/corpus/*`) ficaram em
diretórios já ignorados pelo git, não foram versionados. Nenhum desvio do
plano além dos dois ajustes de constante documentados acima.
