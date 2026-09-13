# C03 — Slots interativos (M3), handoff M2↔M3 e validação de nomes

**Depende de:** A12 (pacote `.lottie`), C01 (writer de state machine), C02 (M2
funcional — grupos por instante do timemap, `HighlightsById`/`ActiveHighlight`
já propagados pela árvore) · **Decisão necessária:** nenhuma nova — M3 (slot
de cor por nota) e a obrigação de um protocolo de handoff já foram decididos
em B02 (ver `docs/plano/decisoes/B02-mecanismo-destaque.md`, itens 1-3) e
citados como escopo pendente em C01 ("Fora de escopo") e C02 ("Fora de
escopo"). Este passo só define os detalhes de implementação: quais ids
recebem slot, o nome reservado de handoff, e o resultado da checagem de
caracteres/unicidade.

## Objetivo

Fechar a linha D-DESTAQUE do `docs/plano/README.md` (M2 + M3), entregando os
dois itens que C01/C02 deixaram explicitamente para cá:

1. **M3 — slot de cor por nota**: toda nota (ou membro de acorde) da página
   exportada por `dotlottie-highlight` ganha um slot de cor dotLottie v2
   (`"sid"` na propriedade de cor), endereçável diretamente pelo próprio
   `xml:id` via `Player::set_color_slot`, **sem depender do playhead da state
   machine de M2** nem de agrupamento — cada nota tem o seu próprio slot,
   mesmo as que M2 agrupou por estarem no mesmo instante do timemap.
2. **Protocolo de handoff M2↔M3**: um jeito documentado e testável de alternar
   entre o modo automático (M2, state machine) e o modo interativo (M3,
   slots) sem os dois controlarem a mesma nota ao mesmo tempo — requisito
   explícito de B02 e do `CLAUDE.md` ("Trocar de modo exige um protocolo de
   handoff... ainda não desenhado").
3. **Validação de nomes**: confirmar (com evidência de código, não suposição)
   se `eventInput`/nome de estado/`sid` derivados de `xml:id` têm alguma
   restrição de caracteres no `dotlottie-rs`, e garantir que os nomes que
   este exportador reserva para controle (`idle`, `GLOBAL`, `hlN`) não colidem
   com um `xml:id` real.

## Ler antes (só isto)

- `docs/plano/decisoes/B02-mecanismo-destaque.md` — decisão final, em
  especial os itens 1-3 (M3 sem pool, fade não autorado no modo interativo,
  slot nomeado pelo `xml:id`) e a nota final sobre o handoff "não desenhado
  neste memorando... escopo novo para C01/C03".
- `docs/plano/C02-notas-animadas.md` — infraestrutura M2 já construída
  (`ActiveHighlight`, `HighlightsById`, propagação em `WriteNodeGroup`) que
  este passo estende em paralelo (M3 não substitui M2 — os dois convivem no
  mesmo JSON de animação, mutuamente exclusivos só em *uso*, não em
  serialização).
- `docs/plano/spikes/B01-resultado.md`, seção E6 — único teste empírico de
  slot de cor disponível: confirma que um slot **ignora completamente** o
  frame/keyframe nativo (`Player::set_color_slot`, aplicado a qualquer
  frame), mas só testado com propriedades `"a":1` (animadas); este passo
  precisa confirmar que `"sid"` também funciona em propriedades `"a":0"`
  (estáticas), caso não fique claro só por leitura de código — ver critério
  de aceite sobre isso.
- `compare/fixtures/b01/gen.py` (`color_property`) — formato real de uma
  propriedade de cor com `"sid"`, já validado empiricamente.
- No checkout local do `dotlottie-rs` referenciado pelo `Cargo.lock` do
  `compare` (`~/.cargo/git/checkouts/dotlottie-rs-*/*/dotlottie-rs/`):
  - `src/string.rs` (`DotString`) — tipo usado para todo nome de
    estado/input/evento na state machine; a única restrição de conteúdo é
    "sem byte nulo interior" (`InteriorNul`), e só quando a feature `c_api`
    está habilitada. `xml:id`/`NCName` do MEI nunca contém byte nulo — não
    há restrição prática de caracteres a documentar além desta.
  - `src/renderer/mod.rs` (`set_color_slot`/`clear_slot`/`clear_slots`/
    `flush_slots`) — slot é só uma chave de `String` num `HashMap`, mesma
    ausência de restrição; `clear_slots()` chama `apply_slot(0)` (reset para
    o valor nativo/keyframado); `clear_slot(id)` remove só aquele id do mapa
    (os demais slots continuam ativos até a próxima `render()`, que
    reconstrói o lote).
  - `src/player.rs` L1482-1490 (`set_color_slot`), L1574-1581
    (`clear_slots`/`clear_slot`) — assinatura pública usada pelo host.
- `verovio/src/lottiewriter.cpp` L257-461 (`ActiveHighlight`, `WriteFill`,
  `WriteStroke`, `WriteShapeGroup`, `WriteNodeGroup`, `AppendChildrenReversed`,
  `WriteLayerShapes`) — ponto exato onde o `sid` de M3 precisa ser
  encaminhado em paralelo ao `highlight`/`ActiveHighlight` de M2.
- `verovio/include/vrv/lottiehighlight.h`/`.cpp` — `LottieHighlightBuilder::
  BuildGroups`/`BuildStateMachine`, onde entra a transição de handoff.
- `verovio/src/toolkit.cpp` (`Toolkit::RenderToDotLottieHighlightFile`) —
  onde `groups` já é calculado; este passo só acrescenta a derivação do
  conjunto de ids interativos a partir dele.
- `compare/src/main.rs` (`sm_render`, `parse_script`, `lottie_to_png`,
  `parse_color_slots`) — roteiro de teste hoje só entende `ms:fire nome`;
  este passo precisa estendê-lo para também aplicar/limpar slots dentro do
  mesmo roteiro, pra testar o handoff completo num único `sm-render`.

## Arquivos

- Modificar: `verovio/include/vrv/lottiewriter.h`, `verovio/src/lottiewriter.cpp`
  (`sid` por nota, independente de M2 estar ativo), `verovio/include/vrv/lottiehighlight.h`
  (comentário/assinatura, se necessário), `verovio/src/lottiehighlight.cpp`
  (transição de handoff + checagem de nomes reservados), `verovio/src/toolkit.cpp`
  (deriva `interactiveIds` de `groups` e repassa a `WriteAnimation`).
- Modificar (ferramental de teste, fora de `verovio/`): `compare/src/main.rs`
  (`sm-render` ganha ações `slot`/`clearslot`/`clearslots` no roteiro).
- Nenhum arquivo novo — M3 reaproveita a IR e os builders que C01/C02 já
  criaram.

## O que fazer

1. **Validação de nomes (conclusão, não é mais "em aberto")**: conforme a
   leitura de `string.rs`/`renderer/mod.rs` acima, **não há restrição de
   caracteres a codificar** — `eventInput`, nome de estado/segmento e `sid`
   são comparados como `&str`/`String` crus, sem parsing especial (o nome
   `DotString` é só um wrapper de string barato de clonar, não path
   pontilhado). Documentar esta conclusão aqui (feito) em vez de inventar um
   esquema de codificação que a implementação de fato não precisa.
   - **Nomes reservados**: este exportador já usa `"idle"`, `"GLOBAL"` e o
     padrão `"hl" + índice` (`LottieHighlightBuilder::BuildGroups`) como
     nomes de controle. Se um `xml:id` real colidisse com um desses, a
     topologia em estrela ficaria ambígua (duas semânticas para o mesmo
     nome). Adicionar em `LottieHighlightBuilder::BuildGroups` uma checagem
     defensiva: se algum id de `ids` for exatamente `"idle"` ou `"GLOBAL"`,
     ou casar com `^hl[0-9]+$`, emitir `LogWarning` citando o id (não
     abortar a exportação — é uma colisão de probabilidade desprezível dado
     como o Verovio gera `xml:id`, e falhar a build inteira por isso seria
     desproporcional; registrar o aviso é suficiente para não passar em
     silêncio).
   - **Unicidade**: `LottieHighlightBuilder::CollectIds` já usa um
     `unordered_set`, então ids duplicados colapsam silenciosamente. Trocar
     a coleta para também contar repetições (ex.: `std::unordered_map<std::string, int>`
     interno, só nesta função) e emitir `LogWarning` se algum id aparecer
     mais de uma vez na árvore renderizada — sintoma de MEI malformado
     (`xml:id` duplicado entre elementos), não algo o Verovio deveria
     mascarar sem aviso.

2. **`LottieWriter`: `sid` por nota (M3), independente de M2**
   (`lottiewriter.h`/`.cpp`):
   - Novo parâmetro em `WriteAnimation`, com default vazio (preserva saída
     byte-idêntica para todo chamador que não o passa — mesmo padrão de
     `highlightGroups`):
     ```cpp
     static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name,
         const std::vector<LottieHighlightGroup> &highlightGroups = {}, int highlightColor = 0xE53935,
         const std::unordered_set<std::string> &interactiveIds = {});
     ```
   - Encaminhar `interactiveIds` (e um `const std::string *activeSlotId`,
     inicialmente `nullptr`) por `WriteLayerShapes` → `AppendChildrenReversed`
     → `WriteNodeGroup` → `WriteShapeGroup` → `WriteFill`/`WriteStroke`,
     **em paralelo** ao par `highlight`/`ActiveHighlight` já existente (não
     substituindo-o). Em `WriteNodeGroup`, ao lado do `if (!node.id.empty())`
     que já resolve `nodeHighlight` a partir de `highlightsById`, resolver
     também `activeSlotId`: se `interactiveIds.count(node.id)`, aponta pro
     próprio `node.id` (não precisa copiar — `node.id` vive durante toda a
     chamada de `WriteAnimation`, mesmo ciclo de vida que já sustenta os
     ponteiros usados hoje). Continua propagando pros filhos como herança
     (mesmo padrão de `inheritedColor`), então todo shape descendente da
     nota (ex.: subcomponentes do notehead, se houver) ganha o mesmo `sid`.
   - `WriteFill`/`WriteStroke` ganham o parâmetro `const std::string *slotId`
     e emitem `,"sid":"<slotId>"` dentro do objeto de cor **sempre que
     `slotId != nullptr`**, tanto no ramo `"a":1` (M2 ativo) quanto no ramo
     `"a":0` (sem M2 ativo) — os dois ramos continuam existindo por causa de
     M2; `sid` é ortogonal a qual ramo é usado. Sem `slotId` (chamador que
     não passa `interactiveIds`), zero mudança no JSON emitido.
   - **Não** deriva o conjunto de ids sozinho — quem decide quais ids viram
     slot é o chamador (`Toolkit`, passo 3), pelo mesmo motivo de
     `highlightGroups` não ser calculado dentro do writer: `LottieWriter` é
     infraestrutura de serialização, não de política de agrupamento/seleção.

3. **`Toolkit::RenderToDotLottieHighlightFile`**: depois de calcular `groups`
   (já existe desde C02), derivar `interactiveIds` como a união de
   `group.memberIds` de todos os grupos (não precisa recalcular o timemap:
   todo id de `groups` já é, por construção de `BuildGroups`, um id da
   página que também é um onset de nota) e passar para `WriteAnimation`.
   Resultado prático: toda nota da página passa a ter tanto o keyframe M2
   (se fizer parte de um grupo — o que hoje é sempre o caso, já que
   `BuildGroups` só deixa de fora ids que não aparecem em nenhum instante do
   timemap) quanto um `sid` M3 individual, mesmo dentro de um grupo M2 de
   várias notas.

4. **Handoff M2→M3 e M3→M2** (`lottiehighlight.cpp`,
   `LottieHighlightBuilder::BuildStateMachine`):
   - Acrescentar ao `GlobalState` uma transição de retorno ao repouso,
     endereçada por um evento reservado `"idle"`:
     ```cpp
     global.transitions.push_back({ "idle", "idle" }); // toState, eventInput
     ```
     Isso resolve o item deixado em aberto no "Fora de escopo" de C02
     ("`idle` não é um evento — hoje só é alcançável como estado inicial").
     `WriteStateMachine` já deriva `inputs` automaticamente a partir dos
     `eventInput` usados, então `"idle"` aparece na lista de inputs sem
     mudança nenhuma em `LottieWriter`.
   - **Protocolo (documentado aqui, é o contrato que o host/zywny precisa
     seguir — nada disso é imposto pelo formato, é convenção de uso)**:
     - **M2 → M3** (automático para interativo): o host chama
       `stateMachineEngine.fire("idle", true)` (zera qualquer destaque em
       andamento — todas as notas não cobertas por slot voltam à cor de
       repouso, preta) e só então chama `player.set_color_slot(xmlId, cor)`
       para cada nota que quiser acender no modo interativo.
     - **M3 → M2** (interativo para automático): o host chama
       `player.clear_slots()` (remove todos os overrides de slot de uma vez,
       restaurando o valor keyframado nativo de cada nota — ver
       `renderer/mod.rs::clear_slots`, que faz `apply_slot(0)`) antes de
       voltar a disparar eventos (`xml:id`/grupo) na state machine
       automática. `clear_slot(id)` individual também funciona se o host só
       quiser apagar uma nota do modo interativo sem sair dele.
     - Os dois mecanismos nunca escrevem no mesmo "canal" ao mesmo tempo por
       construção: M2 manipula o keyframe nativo (`"k"`) via o playhead da
       state machine; M3 sobrescreve via slot, que sempre vence
       independente do frame (E6). O protocolo acima existe só para deixar
       as notas **não tocadas** pelo modo que está assumindo o controle num
       estado visualmente limpo (preto), não para evitar conflito de
       dados — isso já é garantido pelo mecanismo do `dotlottie-rs`.

5. **`compare sm-render`: roteiro precisa expressar slots** (`compare/src/main.rs`):
   - Generalizar `ScriptAction`/`parse_script` para aceitar, além de
     `ms:fire nome`, `ms:slot id:r,g,b` (chama
     `engine.player.set_color_slot(id, ColorSlot::new([r,g,b]))`),
     `ms:clearslot id` (chama `engine.player.clear_slot(id)`) e
     `ms:clearslots` (chama `engine.player.clear_slots()`) — `engine.player`
     já é acessível (`pub player: &'a mut Player`, usado hoje só para
     `engine.player.render()`). Reaproveitar o parser de `"id:r,g,b"` que já
     existe para `lottie-to-png --slot` (extrair para uma função comum se
     ficar direto, senão duplicar as ~10 linhas — decisão de implementação,
     não de arquitetura).
   - Isso permite testar handoff completo num único comando: por exemplo
     `--script "0:fire hl0;50:slot d1e134:0,1,0;100:fire idle;150:clearslots"`.

## Fora de escopo

- Partitura inteira / múltiplas páginas — ainda C04 (nenhuma mudança de
  escopo de página neste passo; `interactiveIds` é calculado por página
  exportada, igual a `groups`).
- Slots animados (`ColorSlot::with_keyframes`) — B02 já registrou que
  provavelmente reintroduz a dependência de um playhead único e decidiu não
  testar agora; fade no modo interativo continua sendo responsabilidade do
  host (ligar/desligar a cor diretamente), conforme decidido.
- Rejeitar/abortar a exportação por causa de nomes reservados colidindo ou
  ids duplicados — vira só aviso (`LogWarning`), não erro. Se isso incomodar
  na prática (corpus real com colisão), é uma decisão nova a tomar depois,
  não deste passo.
- Opções de CLI para escolher quais notas recebem slot, ou desativar M3 —
  não pedido; M3 é sempre gerado por `dotlottie-highlight` a partir daqui
  (mesmo padrão de M2, que também não é opcional nesse formato).
- Mudar `Toolkit::RenderToDotLottieFile`/formato `dotlottie` (o pacote final
  sem state machine) — continua sem M2 nem M3, como C02 já deixou.
- Slots de qualquer tipo além de cor (`gradient`, `image`, `text`, `scalar`,
  `vector`, `position`) — nenhum decidido em B02.

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`) e
  `compare` recompila (`cargo build --release --manifest-path compare/Cargo.toml`).
- `LottieWriter::WriteAnimation` chamado **sem** `interactiveIds` (como
  `RenderToLottie`/`RenderToDotLottieFile` já fazem) produz saída
  byte-idêntica à de antes deste passo — mesmo teste de regressão de C01/C02
  (gerar `dotlottie`/`lottie` de 2-3 peças do corpus antes/depois, diff
  byte a byte de `a/score.json`/`manifest.json`).
- `python3 -m json.tool` valida o `a/score.json` gerado por
  `dotlottie-highlight` com `sid` presente.
- Frame 0 do pacote `dotlottie-highlight` (sem nenhum slot aplicado) continua
  visualmente idêntico (mesma tolerância de C02) ao frame 0 de antes deste
  passo — `sid` sozinho, sem slot setado, não pode mudar a renderização.
- **M3 isolado**: `compare lottie-to-png <pacote> --frame 0 --slot <xml:id de
  uma nota>:<cor> --sample <x,y da nota>` mostra a cor do slot na nota certa,
  sem afetar as vizinhas — inclusive uma nota que é membro de um grupo M2 de
  acorde (confirma que o slot individual funciona mesmo dentro de um grupo
  agrupado).
- **`sid` em propriedade estática (`"a":0`)**: repetir o teste acima numa
  nota que **não** está em nenhum grupo destacado no frame testado (ou seja,
  seu `"c"` é `{"a":0,...,"sid":...}` fora do ramo M2) — se isso não
  funcionar, é um achado a registrar em "Notas de execução" (mudaria a
  implementação do passo 2, não só a validação).
- **Handoff completo** via `compare sm-render` com o roteiro estendido do
  passo 5: disparar um grupo M2 (`fire hlN`), confirmar a cor de destaque
  num snapshot intermediário, disparar `fire idle` e confirmar que a nota
  volta a preto, depois `slot <xml:id>:<cor>` e confirmar que a nota acende
  via M3 mesmo com a state machine parada em `idle`, e por fim `clearslots`
  seguido de outro `fire <xml:id>` confirmando que M2 volta a funcionar
  normalmente (a cor keyframada, não mais o slot, controla a nota outra
  vez).
- `unzip -t` no pacote gerado passa.

## Notas de execução

Implementado exatamente como planejado, sem decisões de escopo novas durante a
execução. Resumo por item:

- **Validação de nomes (item 1)**: confirmado por leitura de código
  (`dotlottie-rs/src/string.rs::DotString`, `src/renderer/mod.rs`) que não há
  restrição de caracteres além de byte nulo interior (irrelevante pra
  `xml:id`/`NCName`) — nenhuma codificação foi implementada, como já previsto.
  `LottieHighlightBuilder::CollectIds` agora conta repetições internamente
  (`std::unordered_map<std::string, int>`) e emite `LogWarning` por id
  duplicado; `BuildGroups` emite `LogWarning` por id que colida com `"idle"`,
  `"GLOBAL"` ou o padrão `^hl[0-9]+$`. **Nenhum dos dois avisos disparou** ao
  regenerar `Chopin_Etude_Op10_No9.mei` (página 1, a mais densa do corpus) ou
  `Scarlatti_Sonata_in_C-major.mei` (página 1) — evidência negativa de que a
  checagem não é barulhenta em uso normal, mas não há teste automatizado
  dedicado pra confirmar que os avisos *disparam de fato* quando deveriam: um
  harness sintético exigiria linkar `Doc`/`GenerateTimemapFunctor` inteiros
  (grafo grande demais pra um teste avulso tipo `test_risk1.cpp` de C01) só
  pra exercitar `BuildGroups`; ficou como verificação por leitura de código,
  não empírica — mencionar se algum dia a lógica dessas duas checagens for
  alterada sem re-verificar com cuidado.
- **M3: `sid` por nota (itens 2-3)**: `WriteAnimation`/`WriteFill`/
  `WriteStroke` ganharam `interactiveIds`/`slotId` exatamente como desenhado,
  propagados em paralelo a `highlight`/`ActiveHighlight` via um novo
  `WriteColorObject` (extraído do código antes duplicado entre fill/stroke).
  `Toolkit::RenderToDotLottieHighlightFile` deriva `interactiveIds` como a
  união de `group.memberIds` (nenhum recálculo de timemap).
- **Handoff M2→M3/M3→M2 (item 4)**: transição `{"idle","idle"}` acrescentada
  ao `GlobalState` de `LottieHighlightBuilder::BuildStateMachine`.
- **`compare sm-render` (item 5)**: `ScriptAction`/`parse_script` generalizados
  para `fire`/`slot`/`clearslot`/`clearslots` (`ScriptOp`), reaproveitando
  `engine.player` (já público) e o parser de `"id:r,g,b"` extraído para
  `parse_color_slot`.

**Build**: `cmake ../cmake && make -j4` limpo (só recompilou os 3 arquivos
alterados + `vrv.cpp`, sem warnings novos). `cargo build --release` do
`compare` também limpo.

**Não-regressão do caminho `dotlottie` puro** (sem `interactiveIds`):
regenerado `chopin_full.lottie` com `-x 42` e comparado estruturalmente
(Python, ignorando os campos `nm`/sufixo de `mn` que carregam `xml:id`)
contra a cópia de antes de C03 em `compare/out/c02/` — **estruturalmente
idêntico** (mesma árvore, mesmas cores, mesma geometria; as únicas
diferenças de texto eram os próprios `xml:id`/referências a eles em 3
elementos `pageMilestoneEnd`/`systemMilestoneEnd`). `manifest.json`
byte-idêntico. Não foi possível confirmar um diff **byte a byte completo**
como em C01/C02 porque o comando exato usado para gerar o
`compare/out/c02/chopin_full.lottie` (sessão anterior) não ficou registrado
com certeza suficiente sobre a seed usada — mas os pacotes
`dotlottie-highlight` (que *são* comparáveis 1:1, porque a sessão anterior
registrou os nomes de grupo/membro em C02: `hl24`=`d1e510`/`d1e539`,
`hl0`=`d1e134`) regeneraram **exatamente os mesmos `xml:id`** com `-x 42`,
confirmando que a reprodutibilidade por seed continua intacta quando o
comando é o mesmo — a comparação estrutural do `chopin_full` acima já basta
para descartar qualquer mudança de comportamento no caminho `dotlottie`
puro (nenhum `"sid"` aparece nele em nenhum dos dois lados, confirmado por
`grep -c '"sid"'` = 0 nos dois).
- Frame 0 de `dotlottie-highlight` sem slot: `compare diff` entre o PNG do
  pacote gerado antes de C03 (`compare/out/c02/scarlatti_p1.lottie`) e depois
  (`compare/out/c03/scarlatti_p1.lottie`) deu **0 pixels diferentes em
  6 237 000 comparados, tolerância 0** — confirma que adicionar `sid` não
  muda a renderização quando nenhum slot está setado.
- `s/sm_highlight.json`: único diff estrutural em ambos os pacotes de teste
  é a transição `{"type":"Transition","toState":"idle","guards":[{"type":"Event","inputName":"idle"}]}`
  a mais no `GlobalState` e `"idle"` a mais em `inputs` (contagem de estados
  inalterada) — exatamente o previsto.

**Validação de `sid` em propriedade estática (`"a":0`)** — achado
importante: com a árvore de decisão atual (`interactiveIds` sempre derivado
como a união dos `memberIds` de `groups`, e `groups` cobre **toda** nota que
aparece em algum instante do timemap), **toda nota que ganha um slot também
está sempre num grupo M2** — ou seja, `dotlottie-highlight` nunca exercita a
combinação `"a":0`+`sid` na prática hoje (só `"a":1`+`sid`). Como o writer
precisa suportar as duas combinações (é o contrato documentado em
`WriteAnimation`), testado à parte com um harness sintético
(`test_sid_static.cpp`, mesmo padrão `g++` direto de C01's `test_risk1.cpp`,
linkando só `lottiewriter.cpp`+`filereader.cpp` com stubs de
`LogWarning`/`LogError`): uma página de um shape só, com `id="note1"`,
`WriteAnimation(pages, "score", /*highlightGroups=*/{}, 0xE53935,
{"note1"})` — confirma no JSON emitido `{"a":0,"sid":"note1","k":[0,0,0,1]}`
e, empiricamente via `compare lottie-to-png --slot note1:0,1,0`, que o
`dotlottie-rs`/ThorVG realmente respeita `sid` numa propriedade não-animada
(preto sem slot, verde `rgba(0,255,0,255)` com slot) — fecha a lacuna que o
spike B01/E6 tinha deixado (só testou `sid` em propriedades `"a":1`).

**Validação do handoff completo** (`compare sm-render` com o roteiro
estendido, pacote `compare/out/c03/scarlatti_p1.lottie`, sample points
`(1386,212)`/`(1385,485)` = as duas notas do acorde `hl24`,
`(209,389)` = a nota isolada `d1e134`/`hl0`):

```
--script "0:fire d1e510;250:fire idle;300:slot d1e134:0,0,1;350:clearslots;400:fire d1e134"
t=0ms   state=hl24  (1386,212)=vermelho (1385,485)=vermelho (209,389)=preto   # M2: acorde acende junto
t=250ms state=idle  (1386,212)=preto    (1385,485)=preto    (209,389)=preto   # handoff M2->M3: fire("idle") apaga o destaque em andamento na hora (não esperou o fade natural)
t=300ms state=idle  (1386,212)=preto    (1385,485)=preto    (209,389)=azul    # M3: slot acende a nota isolada, independente do playhead (SM parada em idle) e sem afetar as outras
t=350ms state=idle  (1386,212)=preto    (1385,485)=preto    (209,389)=preto   # handoff M3->M2: clear_slots() devolve o controle ao valor nativo (preto, já que a SM está em idle)
t=400ms state=hl0   (1386,212)=preto    (1385,485)=preto    (209,389)=vermelho # M2 volta a funcionar normalmente na mesma nota depois do handoff
```

Confirma o protocolo de handoff nas duas direções, numa única execução, na
mesma nota que acabou de passar pelo modo interativo. Achado colateral (erro
do próprio teste, não do código): tentar `fire hl24`/`fire hl0` (nome do
grupo, não de um `xml:id` membro) falha com `FireEventError` — confirma que
o nome do grupo é de fato opaco (só endereçável pelos `xml:id` membros),
como o comentário de `LottieHighlightGroup::name` já documentava.

Arquivos de teste (`compare/out/c03/*`, `/tmp/.../scratchpad/test_sid_static.*`)
ficaram fora do controle de versão (mesmo padrão de C01/C02).
