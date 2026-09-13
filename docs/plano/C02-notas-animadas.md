# C02 — Propriedades animadas das notas

**Depende de:** A12 (pacote `.lottie`), C01 (writer de state machine) · **Decisão
necessária:** nenhuma nova — D-DESTAQUE já foi decidido em B02 (M2 no modo
automático). Este passo só define detalhes de implementação (layout de
frames, nomes internos de estado, constantes de cor/duração), não mecanismo.

## Objetivo

Dar ao Verovio a capacidade de gerar, para uma página real (não mais os
círculos sintéticos de B01), um pacote `.lottie` **funcional**: cada nota (ou
grupo de notas do mesmo instante do timemap, agrupamento M2) tem uma cor
keyframada (preto → cor de destaque → preto) dentro de um marker próprio na
animação `score`, endereçável via a state machine em estrela já construída em
C01 (`GlobalState` com uma transição por `xml:id`, todas mirando o mesmo
`toState` quando fazem parte do mesmo grupo — mecanismo já validado
empiricamente pelo teste do Risco 1 de C01).

Isto fecha o ciclo: `compare sm-render --script "0:fire <xml:id>"` num pacote
gerado por este passo deve mostrar a nota (ou o acorde inteiro, se o
`xml:id` disparado pertencer a um grupo) piscando na cor de destaque e
esmaecendo de volta ao preto.

## Decisão de escopo tomada aqui (documentada, não é uma decisão de arquitetura)

**Este passo cobre só uma página por vez, não a partitura inteira.** Motivo:

- A timeline hoje ("página k+1 no frame k", provisória desde A03/A12,
  explicitamente marcada para ser substituída em C04) usa 1 frame por página
  — não há espaço para os ~20 frames de um fade de destaque sem estourar o
  frame da página seguinte.
- Esticar a janela (`ip`/`op`) de uma página para caber os markers de
  destaque das suas notas empurraria o frame de início de todas as páginas
  seguintes, quebrando a suposição `frame == índice da página` que
  `compare-page.sh`/a varredura de corpus (A13) já usam — regressão real de
  paridade em qualquer peça multi-página com notas destacáveis antes da
  última página.
- Em vez de resolver isso agora (retrabalho que C04 vai fazer de qualquer
  forma, ao redesenhar a timeline como trilha horizontal + câmera), este
  passo entrega o pacote `.lottie` de **uma página isolada** (mesmo padrão de
  escopo de `Toolkit::RenderToLottie(pageNo)`, só que como pacote completo
  com state machine, não um JSON cru de depuração).
- Consequência prática: novo método `Toolkit::RenderToDotLottieHighlightFile`
  + novo formato de CLI `dotlottie-highlight` (recebe `--page`, análogo a
  `lottie`), **não** integrado a `RenderToDotLottieFile`/`dotlottie` (que
  continua produzindo exatamente o mesmo pacote de hoje, sem state machine —
  zero mudança de comportamento nesse caminho). Reintegrar isso na
  partitura inteira é trabalho de C04.

## Ler antes (só isto)

- `docs/plano/decisoes/B02-mecanismo-destaque.md` — mecanismo M2 (agrupamento
  por instante do timemap).
- `docs/plano/C01-writer-state-machine.md` — infraestrutura já construída
  (`LottieStateMachine`, `LottieWriter::WriteStateMachine/WriteManifest`) e o
  teste do Risco 1 (transições múltiplas, mesmo `toState`).
- `compare/fixtures/b01/gen.py` (`color_property`, `MARKERS`, `NOTE_SLOT`) —
  formato JSON real de cor keyframada e do array `markers`, validado
  empiricamente pelo `dotlottie-rs` em B01. **Ler com atenção o comentário
  sobre `NOTE_SLOT = NOTE_LEN + 1`**: cada nota reserva 1 frame de folga além
  da própria duração porque o fim de um marker (frame `tm+dr`) coincide com o
  início do próximo se não houver folga, e aí a cor keyframada da *próxima*
  nota passa a valer sem ninguém ter disparado seu evento (achado do E2 em
  `docs/plano/spikes/B01-resultado.md`) — este passo replica a mesma folga.
- `verovio/include/vrv/lottiegeometry.h`, `verovio/include/vrv/lottiewriter.h`,
  `verovio/src/lottiewriter.cpp` (`WriteFill`, `WriteStroke`, `WriteNodeGroup`,
  `AppendChildrenReversed`, `WriteLayerShapes`, `WriteAnimation` — é aqui que
  a cor é resolvida/escrita hoje, de forma estática).
- `verovio/include/vrv/lottiestatemachine.h` (IR de C01).
- `verovio/include/vrv/timemap.h`, `verovio/src/timemap.cpp` — `TimemapEntry`
  já agrupa `notesOn` por instante exato (`Fraction` como chave do `m_map`);
  isso **já é** o agrupamento M2, não precisa reimplementar.
- `verovio/src/midifunctor.cpp` `GenerateTimemapFunctor::AddTimemapEntry`
  (L1237-1327) — confirma que cada `Note` (mesmo dentro de um `Chord`) gera
  sua própria entrada em `notesOn` na mesma `Fraction` de onset; um acorde já
  cai naturalmente no mesmo grupo, sem tratamento especial.
- `verovio/src/doc.cpp` `Doc::ExportTimemap` (L635-654) — padrão exato para
  construir um `Timemap` populado (`CalculateTimemap()` se necessário +
  `GenerateTimemapFunctor` + `doc.Process(...)`), reaproveitado aqui mas
  chamado sobre `m_doc` diretamente (não `m_midiDoc`) — ver "Fora de escopo"
  sobre por quê.
- `verovio/src/toolkit.cpp` `RenderToLottie`/`RenderToLottieAnimation`/
  `RenderToDotLottieFile` (L1826-1859) e `tools/main.cpp` L284-409 (CLI dos
  formatos `lottie`/`dotlottie`, para espelhar o novo formato).

## Arquivos

- Criar: `verovio/include/vrv/lottiehighlight.h`,
  `verovio/src/lottiehighlight.cpp`.
- Modificar: `verovio/include/vrv/timemap.h` (novo getter),
  `verovio/include/vrv/lottiewriter.h`, `verovio/src/lottiewriter.cpp`
  (cor keyframada + markers), `verovio/include/vrv/toolkit.h`,
  `verovio/src/toolkit.cpp` (novo método), `verovio/include/vrv/toolkitdef.h`,
  `verovio/src/options.cpp`, `verovio/tools/main.cpp` (novo formato de CLI).

## O que fazer

1. **`Timemap::GetMap()`** (`timemap.h`): getter const trivial
   (`const std::map<Fraction, TimemapEntry> &GetMap() const { return m_map; }`)
   — o `m_map` já existe e já é ordenado por instante; só faltava expô-lo
   (hoje só `GetEntry`/`ToJson` são públicos).

2. **`LottieHighlightGroup`** (`lottiehighlight.h`): um grupo M2 pronto pra
   serializar —

   ```cpp
   struct LottieHighlightGroup {
       std::string name;                   // nome interno do marker/estado (opaco; endereço real = memberIds)
       int startFrame = 0;
       int durationFrames = 0;
       std::vector<std::string> memberIds; // xml:ids que disparam este grupo (M2)
   };
   ```

3. **`LottieHighlightBuilder::BuildGroups`** (`lottiehighlight.cpp`):

   ```cpp
   static std::vector<LottieHighlightGroup> BuildGroups(Doc &doc,
       const std::unordered_set<std::string> &ids, int firstFrame, int durationFrames, int gapFrames);
   ```

   - Se `!doc.HasTimemap()`, chama `doc.CalculateTimemap()` (mesma guarda de
     `Doc::ExportTimemap`).
   - Monta um `Timemap timemap; GenerateTimemapFunctor generateTimemap(&timemap);
     generateTimemap.SetNoCue(doc.GetOptions()->m_midiNoCue.GetValue());
     doc.Process(generateTimemap);` — idêntico ao que `Doc::ExportTimemap` faz,
     só que sobre `doc` (parâmetro) em vez de um documento MIDI separado.
   - Itera `timemap.GetMap()` em ordem (já é a ordem de `Fraction`); para cada
     entrada, filtra `entry.notesOn` pelos ids presentes em `ids` (os ids da
     página sendo exportada); se a interseção não for vazia, cria um
     `LottieHighlightGroup` com slot sequencial: `startFrame = firstFrame +
     índice * (durationFrames + gapFrames)`, `name = "hl" + índice`.
   - Não usa `restsOn`/`notesOff`/`measureOn` — fora de escopo (nem o
     agrupamento M2 nem o MVP de destaque dependem disso).

4. **`LottieHighlightBuilder::CollectIds`** (utilitário): caminha uma
   `LottieNode` recursivamente coletando `id`s não vazios num
   `std::unordered_set<std::string>` — usado pelo `Toolkit` pra restringir
   `BuildGroups` aos ids que realmente aparecem na página sendo exportada
   (sem isso, notas de outras páginas da mesma partitura entrariam nos
   grupos e inflariam o pacote à toa — como o instante do timemap é global à
   partitura inteira, isso nunca muda o agrupamento em si, só filtra).

5. **`LottieHighlightBuilder::BuildStateMachine`**:

   ```cpp
   static LottieStateMachine BuildStateMachine(
       const std::vector<LottieHighlightGroup> &groups, const std::string &animationId, const std::string &smId);
   ```

   - `initial = "idle"`. Estado `idle`: `PlaybackState`, `segment:"idle"`,
     `autoplay:false` (o frame 0 — o mesmo frame já usado hoje pra
     `compare-page.sh` — funciona como repouso; não precisa de um marker
     dedicado em outro lugar da timeline, como B01 fez, porque aqui o frame 0
     já está livre por construção: os grupos de destaque começam em
     `firstFrame >= 1`).
   - Um `PlaybackState` por grupo (`segment` = `group.name`, `autoplay:true`,
     `loop:false`) — sem transições próprias (topologia em estrela de C01).
   - Um único `GlobalState` com uma `LottieSMTransition{toState:group.name,
     eventInput:memberId}` **para cada** `memberId` de **cada** grupo — é
     exatamente o padrão validado pelo teste do Risco 1 de C01 (várias
     transições, mesmo alvo, eventos diferentes), só que com os `xml:id`
     reais em vez dos nomes sintéticos `n2`/`n2b` do teste.
   - Sem transição de volta a `idle`: ao terminar um segmento de destaque, o
     estado fica "parado" nele (mesmo comportamento não-corrigido do B01/E1;
     religar a state machine a `idle` fica pra quando isso importar de
     verdade — não é isso que o MVP pede).

6. **`LottieWriter::WriteAnimation`** (`lottiewriter.h/.cpp`) ganha parâmetros
   novos com default (compila e produz saída **idêntica** para todo chamador
   existente que não os passa):

   ```cpp
   static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name,
       const std::vector<LottieHighlightGroup> &highlightGroups = {}, int highlightColor = 0xE53935);
   ```

   - Constantes do MVP ("cor e duração como constantes", conforme o esboço
     C00): `highlightColor` com default acima (vermelho, mesmo tom de
     destaque já usado nos testes de B01/C01 — não há decisão de produto
     ainda sobre a cor final; ajustável via C06 se algum dia virar opção de
     CLI). A duração (`durationFrames`) é constante do lado de quem monta os
     `LottieHighlightGroup` (`Toolkit`, passo 8), não do writer.
   - `op` da composição passa a ser `max(pageCount, maior startFrame+duration
     de highlightGroups)`; a última página da lista tem seu próprio `op`
     esticado até esse mesmo valor (as anteriores mantêm `ip:i,op:i+1` como
     hoje — só a última pode ter notas destacáveis, dado o escopo de uma
     página por vez deste passo; ver "Decisão de escopo" acima).
   - `markers`: se `highlightGroups` não for vazio, emite
     `{"cm":"idle","tm":0,"dr":1}` seguido de um marker por grupo
     (`{"cm":group.name,"tm":group.startFrame,"dr":group.durationFrames}`).
     Se vazio, continua `[]` como hoje.
   - Cor por shape: constrói uma vez
     `std::unordered_map<std::string, ActiveHighlight> highlightsById` (struct
     interna `{startFrame, durationFrames}`) a partir de `highlightGroups`
     (expandindo `memberIds`). Encaminha isso, mais um ponteiro "highlight
     ativo no momento" (começa `nullptr`), por `WriteLayerShapes` →
     `AppendChildrenReversed` → `WriteNodeGroup` → `WriteShapeGroup` →
     `WriteFill`/`WriteStroke` — o mesmo padrão de propagação que
     `inheritedColor` já usa. Em `WriteNodeGroup`, se `node.id` bater no
     mapa, o ponteiro "ativo" passa a apontar pro highlight daquele nó pro
     resto da subárvore (do jeito que a cor herdada já se propaga hoje).
   - `WriteFill`/`WriteStroke`: se há highlight ativo, emitem
     `{"a":1,"k":[...]}` com o formato de `color_property` do `gen.py`
     (hold em preto até `startFrame` se `startFrame>0`, salto pra
     `highlightColor` em `startFrame`, `s` volta a preto em
     `startFrame+durationFrames`, sem `i`/`o` no último keyframe); sem
     highlight ativo, comportamento **inalterado** (`{"a":0,"k":[...]}`).
     A cor "preta" da curva na verdade é a cor já resolvida do shape/nó
     (`ResolveColor`/`shape.fillColor`), não preto cravado — assim uma nota
     que por algum motivo já não fosse preta (ex.: marcação editorial colorida)
     não é sobrescrita, só ganha o pico de destaque no meio.

7. **`Toolkit::RenderToDotLottieHighlightFile(const std::string &filename, int
   pageNo)`** (novo método, `toolkit.h/.cpp`, mesmo estilo de
   `RenderToDotLottieFile`):
   - Renderiza a página pedida com um `LottieDeviceContext` novo (igual
     `RenderToLottie(pageNo)`).
   - `LottieHighlightBuilder::CollectIds` na página renderizada.
   - Constantes do MVP aqui (não em `lottiewriter.cpp`, que é infraestrutura
     genérica): `kFirstHighlightFrame = 1`, `kHighlightDurationFrames = 20`
     (~0.66 s a 30 fps), `kHighlightGapFrames = 1` (a folga do achado E2).
   - `LottieHighlightBuilder::BuildGroups(m_doc, pageIds, 1, 20, 1)` — **usa
     `m_doc` diretamente, não `m_midiDoc`/`SetMidiDoc()`** (ver "Fora de
     escopo" sobre a divergência de ids em peças com expansão de repetição).
   - Serializa animação (`LottieWriter::WriteAnimation`), state machine
     (`LottieHighlightBuilder::BuildStateMachine` + `WriteStateMachine`), e
     `manifest.json` (`WriteManifest` já aceita `stateMachineIds`/
     `initialStateMachine` desde C01) — empacota com `ZipFileWriter` como
     `RenderToDotLottieFile` já faz.

8. **CLI**: novo formato `dotlottie-highlight`, espelhando `lottie` (usa
   `--page`, default página 1, sufixo `_NNN` se `--all-pages`, extensão
   `.lottie`, não pode ir pra stdout — mesma restrição de `dotlottie`).
   - `toolkitdef.h`: `DOTLOTTIE_HIGHLIGHT` no enum `FileFormat`.
   - `options.cpp` `SetOutputTo`: mapeia `"dotlottie-highlight"` →
     `DOTLOTTIE_HIGHLIGHT`.
   - `main.cpp`: adiciona à lista `outformats`/mensagem de erro, e um bloco
     `else if (outformat == "dotlottie-highlight")` chamando
     `toolkit.RenderToDotLottieHighlightFile(curOutfile + ".lottie", p)` no
     mesmo laço `from`/`to` de `lottie`.

## Fora de escopo

- Partitura inteira / múltiplas páginas com destaque (a "Decisão de escopo"
  acima explica por quê) — trabalho de C04, quando a timeline virar trilha
  horizontal e todas as páginas ficarem visíveis ao mesmo tempo (o conflito
  de frames desaparece por construção nesse desenho).
- Validação/normalização de caracteres em nomes de evento (`eventInput` usa
  o `xml:id` cru) e garantia de unicidade de ids — C03.
- Modo interativo (M3, slots de cor `set_color_slot`) — nenhum `"sid"` é
  emitido nas propriedades de cor deste passo; a integração/handoff entre os
  dois modos é C03.
- Virada de página (C04) — nenhuma mudança aqui na semântica de página além
  de esticar o `op` da última página.
- Cor/duração configuráveis por opção de CLI (`svg*`-style) — C06, só se
  pedido.
- Transição automática de volta a `idle` ao fim de um destaque — não pedido
  pelo MVP; religa manualmente via `fire idle`... espera, `idle` não é um
  evento — hoje só é alcançável como estado inicial. Se isso for necessário
  depois, é questão de acrescentar `{toState:"idle", eventInput:"idle"}` ao
  `GlobalState`, mudança pequena e não decidida aqui.
- Peças com expansão de repetição ativa (`m_doc.m_expansionMap` processado):
  como este passo roda o timemap sobre `m_doc` diretamente (não
  `m_midiDoc`/`SetMidiDoc()`, que nesses casos usa uma *cópia* re-parseada
  do documento), o comportamento pra essas peças é o mesmo de nunca ter
  havido expansão — os grupos refletem a partitura **não expandida**. Isso é
  aceitável para o MVP (o corpus de teste atual não usa expansão) mas fica
  registrado como limitação conhecida, não silenciosa.
- `restsOn`/`measureOn` do timemap — o destaque de nota não usa pausas nem
  fronteiras de compasso.

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`).
- `LottieWriter::WriteAnimation` chamado **sem** os novos parâmetros (como
  `RenderToLottieAnimation`/`RenderToLottie` já fazem) produz saída
  **byte-idêntica** à de antes deste passo — testar regenerando
  `compare/out/c02/*.lottie` a partir de 2-3 arquivos do corpus com o CLI
  `dotlottie` e comparando com uma cópia gerada antes das mudanças
  (`unzip -p ... a/score.json` diff).
- `python3 -m json.tool` valida `a/score.json` e `s/sm_highlight.json` de um
  pacote gerado por `dotlottie-highlight`.
- `unzip -t` no pacote gerado passa.
- Frame 0 (`idle`) do pacote `dotlottie-highlight` de uma página do corpus
  é visualmente idêntico (diff de pixel, mesma tolerância já usada por
  `compare-page.sh`) ao frame 0 do `lottie`/`dotlottie` de hoje pra mesma
  página — o destaque não pode alterar a aparência em repouso.
- `compare sm-render` no pacote de uma peça do corpus com acorde
  (`Chopin_Mazurka_Op6_No1.mei`, `Scarlatti_Sonata_in_C-major.mei` ou outra
  com `<chord>` — ver contagem no README de decisões), `--script "0:fire
  <xml:id de uma nota do acorde>"`, mostra **todas** as notas do acorde (mesmo
  instante) na cor de destaque num snapshot dentro do intervalo do marker, e
  de volta à cor original num snapshot após `startFrame+durationFrames`.
- `compare sm-render` disparando o `xml:id` de uma nota isolada (não em
  acorde) destaca só ela, não as vizinhas.

## Notas de execução

- Implementado exatamente como planejado: `Timemap::GetMap()` (getter trivial),
  `LottieHighlightGroup`/`LottieHighlightBuilder` novos
  (`lottiehighlight.h/.cpp`), `LottieWriter::WriteAnimation` com os dois
  parâmetros novos (default vazio/vermelho), `Toolkit::RenderToDotLottieHighlightFile`,
  e o formato de CLI `dotlottie-highlight` (`toolkitdef.h`, `options.cpp`,
  `main.cpp`), espelhando o bloco `lottie`/`dotlottie` já existentes.
- Build limpo (`cmake ../cmake && make -j4`); os únicos warnings do log são
  pré-existentes em `iohumdrum.cpp`/`atts_shared.h` (`-Warray-bounds`, nada
  relacionado a este passo).
- **Não-regressão confirmada byte-a-byte**: `git stash` das mudanças deste
  passo, rebuild, gerar `dotlottie`/`lottie --page 1` com `--xml-id-seed 42`
  pros 5 arquivos do corpus, `git stash pop`, rebuild, regerar com o mesmo
  seed — `a/score.json` e o JSON de `lottie` ficaram **idênticos byte a
  byte**; só `manifest.json` do `dotlottie` divergiu, e só no sufixo
  `-dirty` da string de versão (reflete o estado sujo do git no momento do
  build, não uma mudança de comportamento).
- **Validação visual** com `Scarlatti_Sonata_in_C-major.mei` (página 1,
  `--xml-id-seed 42`, grupo `hl24` = acorde com `d1e510`/`d1e539` em
  claves diferentes, grupo `hl0` = nota isolada `d1e134`):
  - `compare lottie-to-png --frame 0` do pacote `dotlottie-highlight` vs. do
    `lottie` de depuração de hoje: **0 pixels diferentes** (tolerância 0,
    693000 pixels comparados) — o estado `idle` não altera a aparência em
    repouso.
  - `compare sm-render --script "0:fire d1e510"` (`hl24`, acorde entre duas
    claves): as duas notas do grupo acendem **juntas** em vermelho no
    snapshot `t=0`, ambas já esmaecidas de volta ao preto em `t=600` (~90%
    do fade de 20 frames/666ms) — confirma o agrupamento M2 funcionando
    através de vozes/claves diferentes com o mesmo instante do timemap.
  - `compare sm-render --script "0:fire d1e134"` (`hl0`, nota isolada):
    só ela acende; as vizinhas permanecem pretas — confirma que o
    agrupamento não vaza pra notas de instantes diferentes.
  - `unzip -t` e `python3 -m json.tool` em `a/score.json`/`s/sm_highlight.json`
    passaram para os pacotes gerados.
- **Escopo confirmado na prática**: o pacote gerado para
  `Chopin_Etude_Op10_No9.mei` (página 1, a mais densa do corpus) produziu
  267 grupos de destaque / 353 eventos, `op` esticado de 4 pra 5607 frames —
  nenhum problema de escala percebido (build e `sm-render` seguiram rápidos,
  consistente com o E5 de B01 sobre custo de estados).
- Arquivos de teste (`compare/out/c02/*`) ficaram no diretório já ignorado
  pelo git (`compare/out/`), não foram versionados.
- Nenhum desvio do plano original; a única coisa deixada propositalmente
  incompleta é o que já estava em "Fora de escopo" (partitura inteira,
  validação de nomes de evento, modo interativo, opções de CLI).
