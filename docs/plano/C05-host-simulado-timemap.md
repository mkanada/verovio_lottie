# C05 — Host simulado com timemap

**Depende de:** A12 (pacote `.lottie`), A13 (varredura do corpus), B01
(`compare sm-render`), C01-C04 (writer de state machine, M2, M3, páginas) ·
**Decisão necessária:** nenhuma. Este passo não muda nenhum comportamento do
Verovio nem do `dotlottie-rs` — é ferramental de validação (mesmo espírito de
B01: "produz evidência, não código de produção"), fechando a fase C com uma
prova de ponta a ponta usando **tempo real** de tocada em vez de roteiros
escritos à mão como B01-C04 fizeram.

## Objetivo

Toda validação de `sm_highlight` até aqui (B01, C02, C03, C04) usou roteiros
`--script "ms:fire id"` com instantes **escolhidos à mão** pelo implementador.
Isso nunca exercitou o timing real de uma peça — não prova que, tocando a
partitura de verdade, os eventos disparados pelo host na hora certa produzem
o resultado esperado.

Este passo fecha essa lacuna: gera o roteiro de `compare sm-render`
**automaticamente** a partir da saída real de `verovio -t timemap` (que já dá,
por nota, o instante exato de onset em milissegundos — a mesma fonte de dados
que o host real do zywny vai usar para tocar a partitura), simulando um host
de playback de verdade sobre um pacote `dotlottie` gerado por
`Toolkit::RenderToDotLottieFile` (C04). Salva PNGs de evidência nos instantes-
chave (cada onset disparado, mais o instante em que o fade da última nota
termina) para inspeção visual.

## Achado que molda este passo (leitura de código + teste direto, não suposição)

`verovio/src/options.cpp` L980 (`m_xmlIdSeed.SetInfo("XML IDs seed", "Seed the
random number generator for XML IDs (**default is random**)")`): os `xml:id`
que o Verovio atribui a notas sem id explícito no MEI de origem (todo o corpus
atual) **não são determinísticos entre execuções** a menos que `-x <seed>`
seja passado. Como este passo precisa rodar `verovio -t timemap` e
`verovio -t dotlottie` em **duas invocações separadas** do binário (formatos
de saída diferentes, CLI só aceita um `-t` por vez) e casar os `xml:id` de uma
com os eventos da state machine da outra, isso só funciona de forma confiável
com a mesma seed fixa nas duas chamadas — sem isso, todo `fire` falharia
silenciosamente (`compare sm-render` só avisa no stderr, não aborta, então o
sintoma seria "nenhuma nota acende" sem erro óbvio). Confirmado empiricamente
antes de escrever o script (`corpus/mei/Scarlatti_Sonata_in_C-major.mei`,
`-x 42` nas duas chamadas): a primeira nota (`d1e134`, tstamp 0) e a ordem dos
`xml:id` batem exatamente entre a saída de `-t timemap` e os eventos
declarados em `s/sm_highlight.json` do pacote `-t dotlottie`. O script usa
`-x 42` como default (mesmo valor já usado nos testes manuais de C02-C04, por
consistência), configurável.

## Ler antes (só isto)

- `docs/plano/B01-spike-state-machine.md` e `compare/README.md` — `compare
  sm-render` (roteiro `ms:ação`, `--snap`, a pegadinha de `set_frame`/`render`
  — não relevante para escrever este script, mas para entender por que o
  binário já lida com isso sozinho).
- `docs/plano/C02-notas-animadas.md`, seção "Fora de escopo", último item:
  `LottieHighlightBuilder::BuildGroups` roda o timemap sobre `m_doc`
  diretamente (não `m_midiDoc`); `Toolkit::RenderToTimemap`
  (`verovio/src/toolkit.cpp` L2073-2101, usado por `-t timemap`) roda sobre
  `m_midiDoc` (via `SetMidiDoc()`, `L280`). Nas peças do corpus atual (sem
  expansão de repetição — confirmado por C02), as duas fontes produzem o
  mesmo timemap; isso deixa de valer para peças com expansão (limitação já
  conhecida, não deste passo).
- `verovio/include/vrv/timemap.h`, `verovio/src/timemap.cpp::ToJson` — formato
  exato do JSON de `-t timemap` (`on`/`off`/`tstamp` em ms, já arredondado
  — `midifunctor.cpp::AddTimemapEntry` usa `round(...)`).
  `verovio/src/toolkit.cpp` L2073-2101 (`Toolkit::RenderToTimemap`).
- `verovio/src/toolkit.cpp` L1848-1919 (`Toolkit::RenderToDotLottieFile`,
  C04) — confirma que o pacote `dotlottie` cobre a partitura inteira com uma
  única `sm_highlight`, e que a câmera de página (`sm_page`, se houver mais de
  uma página) começa parada na página 1 (não é acionada por este passo —
  só `sm_highlight` é testado, então só notas da primeira página aparecem no
  PNG por padrão, ver "Decisões de escopo").
- `verovio/src/lottiewriter.cpp` L637 — `"fr":30` fixo (frame rate da
  composição); a duração do fade (`kHighlightDurationFrames = 20`, hoje
  hardcoded em `RenderToDotLottieFile`) dá ~667ms — usado para calcular o
  instante do snap final deste script.
- `compare/src/main.rs` (`sm_render`, `parse_script`, `parse_snap`) — formato
  exato aceito por `--script`/`--snap` (já lido integralmente para B01-C04,
  sem mudança neste passo).
- `compare/scripts/compare-page.sh` — convenções de script já estabelecidas
  neste repositório (resolução de `SCRIPT_DIR`/`REPO_ROOT`, checagem dos
  binários, prefixo sem pontos pro `-o` do Verovio, leitura de metadados via
  `python3 - <<'PYEOF' ... PYEOF` dentro de `$(...)`) — este passo segue o
  mesmo estilo, não inventa um novo.
- `verovio/src/options.cpp` L980-984 (`m_xmlIdSeed`) — ver "Achado" acima.

## Decisões de escopo tomadas aqui

- **Só testa `sm_highlight`, não `sm_page`.** O objetivo é validar o timing
  real de destaque de nota; a câmera de página já foi validada
  separadamente em C04 com seu próprio roteiro dedicado. Rodar as duas state
  machines juntas exigiria duas instâncias de `Player` compositando (o risco
  aberto de B02/C04, explicitamente responsabilidade do host, não deste
  exportador) — fora de escopo aqui.
- **Roteiro real, mas truncado a um prefixo configurável da peça
  (`máx-eventos`, default 12).** Simular a peça inteira (algumas têm centenas
  de onsets) produziria um roteiro gigante e, mais importante, uma explosão
  de PNGs se cada onset virasse um snapshot. Como o objetivo é provar que o
  **timing real** (não mais instantes escolhidos à mão) dirige a state
  machine corretamente, um prefixo real já basta — não há nada
  qualitativamente diferente no meio/fim de uma peça que um prefixo maior
  provaria e um menor não. `máx-eventos` é um parâmetro do script exatamente
  para permitir estender a amostra quando fizer sentido (ex.: alcançar um
  acorde específico mais adiante — ver "Critérios de aceite").
- **Cada instante do timemap com `on` dispara só o primeiro id da lista.**
  M2 (C02) já agrupa todas as notas do mesmo instante no mesmo estado/marker
  — disparar mais de um `xml:id` do mesmo grupo no mesmo instante seria
  redundante (mesmo `toState`), não errado, só desperdiçado. Isso também
  significa que um acorde real do timemap já testa M2 através de um único
  evento de host, exatamente como o mecanismo foi desenhado.
- **Instantes-chave (`--snap`) = todo instante de onset incluído, mais um
  snap final em `último-onset + duração-do-fade`.** Cada onset já é por
  definição um instante-chave (é quando o estado visível muda). O snap final
  isolado (sem nenhum evento agendado depois dele no roteiro truncado) é o
  único ponto em que dá pra observar um fade **completo e não interrompido**
  até preto — qualquer snap "onset + duração" no meio do roteiro seria
  ambíguo, porque muito provavelmente uma nota seguinte já disparou antes
  (ver "Achado" abaixo sobre re-trigger).
- **Não usa `--sample`.** Amostrar pixels exigiria coordenadas de nota
  conhecidas de antemão; como o roteiro é derivado dinamicamente de qualquer
  arquivo de entrada, não há como calcular isso sem já ter renderizado. A
  validação aqui é visual (inspecionar os PNGs salvos, como o esboço C00
  pede) e pelo log de `state=` que `sm_render` já imprime a cada snap
  (confirma qual grupo está ativo em cada instante, sem precisar de pixel).
- **Não gera nenhum roteiro pra `sm-render --measure-load`/E5 de novo** — já
  coberto por B01; irrelevante aqui.

## Arquivos

- Criar: `compare/scripts/sm-playback.sh`.
- Nenhuma mudança em `verovio/` nem em `compare/src/main.rs` — toda a
  infraestrutura necessária (`-t timemap`, `-t dotlottie`, `compare
  sm-render`) já existe.

## O que fazer

1. **`compare/scripts/sm-playback.sh <arquivo> [máx-eventos] [seed]`**
   (`máx-eventos` default `12`, `seed` default `42`), mesmo esqueleto de
   `compare-page.sh` (resolução de caminhos, checagem dos binários,
   `compare/out/c05/<nome>/` como diretório de saída, prefixo temporário sem
   pontos pro `-o` do Verovio):
   1. `verovio -t dotlottie -x <seed> -o <tmp> --resource-path
      verovio/data <arquivo>` → move para `<out>/<nome>.lottie`.
   2. `verovio -t timemap -x <seed> -o - --resource-path verovio/data
      <arquivo>` → salva em `<out>/timemap.json` (stdout, sem `-p` — timemap
      já cobre a partitura inteira).
   3. Um bloco `python3 - <<'PYEOF' ... PYEOF` dentro de `$(...)` (mesmo
      padrão de `compare-page.sh`) que:
      - Lê `timemap.json`; filtra entradas com chave `"on"`; ordena por
        `tstamp` (já vem ordenado, mas reordena defensivamente); trunca aos
        primeiros `máx-eventos`.
      - Para cada entrada incluída, usa `entry["on"][0]` (ver "Decisões de
        escopo") e `round(entry["tstamp"])` como o par `ms:fire xml_id`.
      - `FADE_MS = round(20 / 30 * 1000)` (constantes hoje hardcoded em
        `Toolkit::RenderToDotLottieFile` — 20 frames a 30fps; ver
        `docs/plano/C06-opcoes-cor-duracao.md` se isso um dia deixar de ser
        fixo: este script **não** lê a opção de CLI, é um valor replicado à
        mão, documentado aqui como uma dívida pequena e aceitável).
      - Monta `script.txt` (`;`-separado), `snap.txt` (`,`-separado: `{0} ∪
        {tstamp de cada evento incluído} ∪ {tstamp do último evento +
        FADE_MS}`), e `roteiro.txt` (uma linha legível por evento, para
        inspeção humana sem precisar reabrir o JSON).
      - Lê `w`/`h` de `a/score.json` dentro do pacote gerado (`unzip -p ...
        | json.load`, mesmo padrão que C04 já usou em `compare-page.sh` para
        ler o marker de página) — não precisa renderizar SVG nenhum, o
        `w`/`h` declarado no Lottie já é a única fonte de verdade do
        viewport (C04).
      - Imprime `W H TOTAL INCLUDED` numa linha (capturado pelo bash com
        `read`).
   4. `compare sm-render <out>/<nome>.lottie <out> --sm sm_highlight --width
      $W --height $H --script "$(cat script.txt)" --snap "$(cat snap.txt)"
      --prefix playback`.
   5. Imprime um resumo (`$INCLUDED de $TOTAL instantes de onset reais
      incluídos`).

## Fora de escopo

- Qualquer mudança em `verovio/src`/`verovio/include` — puro ferramental de
  validação, nenhum comportamento de exportação muda.
- Simular `sm_page` junto (ver "Decisões de escopo").
- Amostragem de pixel automática / limiar de diff automático — validação
  aqui é visual (PNGs + log de `state=`), não um critério numérico de
  passou/falhou (mesmo espírito do `compare/README.md`, seção "Limitações
  atuais").
- Rodar sobre o corpus inteiro tipo A13 — este passo valida o **mecanismo**
  (timing real → state machine), não faz uma varredura de paridade; A13 já
  cobre a superfície do corpus para o critério visual/pixel.
- Consumir a opção de duração de C06 dinamicamente (ver nota no passo 3) —
  o valor fica replicado à mão neste script; não vale a pena parametrizar
  isso antes de C06 decidir se a opção existe de verdade.
- Ler o número de páginas / mover a câmera do pacote para notas fora da
  primeira página — fora do roteiro deste script (ver "Decisões de escopo").

## Critérios de aceite

- `compare/scripts/sm-playback.sh corpus/mei/Scarlatti_Sonata_in_C-major.mei`
  roda sem nenhum `aviso: fire(...) falhou` no stderr do `compare
  sm-render` — confirma que todo `xml:id` derivado do timemap real casa
  exatamente com um evento declarado em `sm_highlight` do pacote gerado na
  mesma execução (fecha o "Achado" da seed acima, de ponta a ponta, não só
  no teste manual isolado feito durante a pesquisa deste passo).
- O log de `state=` impresso a cada `--snap` muda de estado a cada onset
  disparado (confirma que o host simulado realmente dirige a state machine
  no tempo certo, não só que os `fire` não erraram).
- Inspeção visual de pelo menos 3 PNGs salvos (`playback-t0.png` = a
  **primeira** nota já destacada — `sm_render` roda as ações marcadas para o
  instante `t` **antes** do snapshot desse instante, mesmo comportamento já
  usado em B01/E1, então `t=0` nunca é "repouso preto" quando há um `fire`
  agendado exatamente em `t=0`, que é sempre o caso aqui; um PNG no meio do
  roteiro = nota daquele instante destacada, a anterior já apagada; o último
  PNG, `t = último-onset + FADE_MS` = de volta a preto, sem interrupção)
  confirma visualmente o comportamento esperado.
- Rodando com `--` — ou melhor, com `máx-eventos=25` sobre o mesmo arquivo
  (`sm-playback.sh corpus/mei/Scarlatti_Sonata_in_C-major.mei 25`): o
  instante `tstamp=6000` (24º onset) dispara `d1e510` (ou `d1e539`, o
  primeiro da lista), e o PNG desse snap mostra **as duas** notas do acorde
  (`d1e510`+`d1e539`, o mesmo grupo `hl24` já validado visualmente em C02)
  destacadas juntas a partir de um único evento — confirma M2 funcionando
  através do agrupamento real do timemap, não de um par de ids escolhido à
  mão como em C02/C03.
- Rodar o mesmo script numa segunda peça do corpus com estrutura diferente
  (ex.: `Chopin_Mazurka_Op6_No1.mei`) sem erro — confirma que o script não é
  específico do Scarlatti.
- `unzip -t` no `.lottie` gerado passa.

## Notas de execução

Implementado exatamente como planejado: `compare/scripts/sm-playback.sh
<arquivo> [máx-eventos=12] [seed=42]`, mesmo esqueleto de `compare-page.sh`
(sem nenhuma mudança em `verovio/` nem em `compare/src/main.rs` — puro
ferramental de validação, como previsto). Nenhum desvio do plano além de uma
correção de redação nos "Critérios de aceite" descoberta ao rodar pela
primeira vez (documentada abaixo).

- **Achado da seed confirmado na prática, não só no teste manual da
  pesquisa**: rodando `sm-playback.sh corpus/mei/Scarlatti_Sonata_in_C-major.mei`
  (defaults), **zero** `aviso: fire(...) falhou` no stderr — todo `xml:id`
  derivado do timemap real (`-t timemap -x 42`) casou exatamente com um
  evento declarado em `s/sm_highlight.json` do pacote gerado na mesma
  execução (`-t dotlottie -x 42`). Mesmo resultado numa segunda peça de
  estrutura bem diferente, `Chopin_Mazurka_Op6_No1.mei` (ritmo irregular,
  onsets a 500/42/41/42/250/125/125/375/500/375/125/667ms de distância uma
  da outra, nada parecido com o espaçamento quase-uniforme do Scarlatti) —
  confirma que a seed fixa resolve o problema de forma geral, não só para a
  peça usada na pesquisa deste passo.
- **Correção de redação nos "Critérios de aceite"** (comportamento correto,
  só a descrição do doc original estava errada): o primeiro PNG
  (`playback-t0.png`) mostra a primeira nota **já destacada**, não "repouso
  preto" como o texto original deste arquivo dizia — `compare sm-render` já
  roda as ações agendadas para o instante `t` **antes** de tirar o
  snapshot desse mesmo instante (mesmo comportamento de B01/E1), e como o
  roteiro sempre tem um `fire` em `t=0` (primeiro onset da peça), o snap de
  `t=0` sempre mostra esse `fire` já aplicado. Corrigido no corpo deste
  arquivo (seção "Critérios de aceite") depois de ver o primeiro PNG gerado.
- **Validação visual** (`corpus/mei/Scarlatti_Sonata_in_C-major.mei`,
  defaults, seed 42): `playback-t0.png` mostra só `d1e134` (a primeira nota
  do baixo) em vermelho; `playback-t2750.png` mostra só `d1e252` (11º
  onset, uma nota do meio da melodia) em vermelho, as anteriores já pretas
  — confirma a state machine trocando de nota em nota, no tempo real de
  cada onset; `playback-t3417.png` (`= 2750 + 667ms`, o snap final, sem
  nenhum evento agendado depois) volta inteiramente a preto — confirma um
  fade completo e não interrompido quando não há re-trigger.
- **Validação do acorde real** (`sm-playback.sh
  corpus/mei/Scarlatti_Sonata_in_C-major.mei 25`, o critério de aceite que
  pede alcançar o primeiro acorde real da peça): o 25º onset do timemap
  (`tstamp=6000`, entrada `{"off":["d1e423","d1e492"],"on":["d1e510","d1e539"]}`)
  é exatamente o grupo `hl24` (`d1e510`+`d1e539`) já conhecido de C02.
  `playback-t6000.png` mostra **as duas notas simultaneamente** em vermelho
  (uma no sistema de agudos, outra no de graves) a partir de um único
  `fire d1e510` — confirma M2 funcionando através do agrupamento real do
  timemap (não de um par de ids escolhido à mão), a lacuna que este passo
  existia para fechar.
- **`unzip -t`** nos dois pacotes gerados (`Scarlatti_Sonata_in_C-major.lottie`,
  `Chopin_Mazurka_Op6_No1.lottie`) passou.
- Achado colateral, não relacionado ao mecanismo testado: `verovio -t
  timemap` (que usa `m_midiDoc`/`SetMidiDoc()`, `Toolkit::RenderToTimemap`)
  emitiu `[Warning] An expansion cannot be generated with more than one
  section` no Scarlatti, aviso que **não** aparece na chamada `-t dotlottie`
  (que roda o timemap sobre `m_doc` diretamente, `LottieHighlightBuilder::
  BuildGroups`). Não impediu a correspondência de ids (confirmado acima) —
  é só evidência concreta, e não mais hipotética, de que os dois caminhos
  de timemap *podem* divergir em algum processamento interno (aqui,
  tentativa de expansão que falha e não é aplicada) mesmo sem mudar o
  resultado final nas peças do corpus atual, reforçando por que C02 já
  registrou a divergência `m_doc`/`m_midiDoc` como limitação conhecida (não
  escopo deste passo resolver).
- Arquivos de teste (`compare/out/c05/*`) ficaram em diretório já ignorado
  pelo git, não foram versionados (mesmo padrão de B01-C04).
- **Limitação notada, mas não é regressão nem bug**: `sm-playback.sh`
  cria seu pacote/timemap temporários num nome fixo
  (`compare/out/c05/_sm-playback-tmp.*`) — duas execuções **concorrentes**
  do script (peças diferentes ao mesmo tempo) colidiriam nesse arquivo
  temporário compartilhado. Não é um problema para o uso previsto (rodar
  uma peça de cada vez, mesmo padrão de uso de `compare-page.sh`), só
  registrado aqui para quem for automatizar isso num laço tipo
  `compare-corpus.sh` no futuro (fora de escopo deste passo, que
  explicitamente não pede uma varredura de corpus).
