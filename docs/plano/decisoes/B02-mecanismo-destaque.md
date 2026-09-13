# B02 — Memorando: mecanismo de destaque e virada de página

**Insumos:** `CLAUDE.md`, `docs/descricao-do-projeto.md` (seções "Destaque de
notas" e "Virada de página"), `docs/plano/spikes/B01-resultado.md`.

**Status:** aguardando decisão do usuário (D-DESTAQUE e D-LAYOUT-PAGINAS).

## Recapitulando o que o B01 provou (não reabrir)

Toda a análise abaixo parte destes fatos, já confirmados empiricamente:

1. `PlaybackState` + `segment`, endereçado por evento nomeado = `xml:id`, dá
   **topologia em estrela real** (E1, E4) e **fade com duração/curva
   autorada no Lottie** (E1, E2) — mas só **uma nota por vez**: o engine tem
   um único `current_state`/playhead, e disparar uma segunda nota corta a
   primeira instantaneamente, mesmo em acorde (E3).
2. Slots de cor (`set_color_slot`) acendem N notas ao mesmo tempo
   independente do playhead (E6), mas não há fade automático comprovado —
   só substituição estática de cor.
3. Virada de página e destaque de nota, **se dividirem a mesma state
   machine/engine**, colidem: disparar a virada cancela um fade em
   andamento (achado "Extra" do B01), pelo mesmo mecanismo do E2.
4. Escala não é um problema: 3000 estados comprimem para ~34 KB e carregam
   em ~12 ms (E5). Custo de ter *mais* estados/animações no pacote é
   desprezível na faixa de uma música inteira.

O requisito que gera o conflito central: **CLAUDE.md exige, ao mesmo tempo,
notas simultâneas com fades sobrepostos, fade controlado pelo Lottie (não
pelo host) e endereçamento direto por `xml:id`**. B01 mostrou que nenhum
mecanismo de *engine único* entrega os três ao mesmo tempo — só há como
sobrepor fades de verdade dando a cada fonte de fade seu próprio playhead
(ou seja, sua própria instância de engine).

## Candidatos

| Id | Descrição | Base |
| --- | --- | --- |
| **M1** | Um `PlaybackState`/segmento por nota, um único engine/state machine para todo o destaque. | Validado em E1-E4 |
| **M2** | Como M1, mas notas que começam no **mesmo instante do timemap** compartilham um único estado/segmento (evento pode ser disparado por **qualquer** `xml:id` do grupo — múltiplas transições guardadas, cada uma por um `Event` diferente, todas apontando pro mesmo estado-alvo; não precisa de tabela de tradução de IDs nova). | Extrapolação estrutural de E1/E4 (não testada isoladamente, ver "Riscos") |
| **M3** | Cores por slot/tema (`SetTheme`, `set_color_slot`), sem depender do playhead. | Validado (parcialmente) em E6 |
| **M4** | Várias animações no mesmo pacote — uma minúscula por nota (ou por grupo de instante), **nomeada pelo próprio `xml:id`**, endereçada diretamente (sem state machine: o nome da animação já é o endereço) — tocadas por um **pool de K instâncias de player** que o host mantém e composita por cima do render principal. | Composição de E1-E4 (o padrão de segmento/fade autorado) + E5 (custo de mais entradas no pacote é baixo) |
| **M5** | Timeline única com o host dirigindo o playhead via `SetFrame`/`SetProgress`; Lottie só fornece a curva local de cada trecho. | Descartado — ver abaixo |
| **M6** | **Recomendado.** Híbrido: M4 (com o refinamento de agrupar instantes simultâneos, M2, como otimização de nº de slots) para destaque de nota + engine **totalmente separado** (state machine própria, padrão M1, aplicada à posição da câmera) para virada de página. | Resolve (d) e (e) juntos |

### Por que M5 foi descartado sem tabela

M5 pede ao host que empurre `SetFrame`/`SetProgress` continuamente (scrubbing
ativo), o que contraria "o disparo vem do host, a duração/curva fica no
Lottie" — na prática o host passaria a ser responsável por calcular o
progresso do fade a cada tick, exatamente o que CLAUDE.md diz para evitar.
Além disso continua preso a um único playhead, então herda a limitação de
M1 em (d) sem ganhar nada em troca. Não comparado na tabela por não atender
nenhum critério que M1 já não atenda melhor.

## Tabela candidato × critério

Legenda: ✅ atende · ⚠️ atende parcialmente / com ressalva · ❌ não atende ·
🧪 extrapolação plausível, não testada diretamente em B01.

| Critério | M1 | M2 | M3 | M4 | M6 (recomendado) |
| --- | --- | --- | --- | --- | --- |
| Endereçamento por `xml:id` | ✅ (E1) | ✅ 🧪 (grupo disparável por qualquer `xml:id` membro) | ✅ (slot nomeado por nota) | ✅ (nome da animação = `xml:id`) | ✅ |
| Acesso direto a qualquer nota (estrela) | ✅ (E1, E4) | ✅ 🧪 | ✅ (sem playhead, direto) | ✅ (nenhuma indireção — nem precisa de state machine) | ✅ |
| Notas simultâneas | ❌ (E3: só a última do roteiro) | ⚠️ (só as do **mesmo instante exato**; vozes com onsets diferentes ainda colidem) | ✅ (E6, estático) | ✅ (até K instâncias concorrentes) | ✅ (até K, com M2 economizando slots em acordes) |
| Fades sobrepostos | ❌ | ⚠️ (dentro do grupo, o fade é um só, compartilhado — não há "dois fades independentes" dentro do mesmo instante, mas isso é aceitável pois começam juntos) | ❌ (sem fade automático comprovado, E6) | ✅ (cada instância tem seu próprio playhead) | ✅ |
| Fade controlado pelo Lottie (não pelo host) | ✅ (E1, E2) | ✅ 🧪 | ❌ (precisaria o host recalcular cor a cada tick — E6) | ✅ (curva autorada por nota/grupo, autoplay) | ✅ |
| Convive com virada de página | ❌ se dividir engine (achado "Extra") | ❌ idem | ⚠️ (slots não usam playhead de state machine, mas ainda não testado com página real) | ✅ **se** a página usar outra(s) instância(s) | ✅ (por desenho: página é outro engine) |
| Tamanho / tempo de carga | ✅ ótimo (E5) | ✅ ótimo (menos estados que M1) | não medido em B01 | ✅ (N animações pequenas; custo por estado é baixo, E5) — geometria por nota não medida isoladamente | mesmo custo de M4 |
| Compatível com players comuns (web/mobile) | ✅ (uso padrão de state machine) | ✅ (idem) | ⚠️ (`set_color_slot`/temas é API menos universalmente exposta nos players de UI pronta) | ⚠️ **não confirmado**: exige o runtime permitir múltiplas instâncias de player compositadas — não testado no B01 (`sm-render` só exercitou 1 engine) | ⚠️ mesma ressalva de M4 |
| Complexidade no exportador | baixa | baixa | baixa/média (temas) | baixa (animações pequenas e repetitivas, mesmo padrão N vezes) | baixa/média |
| Complexidade no host (zywny) | baixa | baixa | média (recalcular cor se quiser fade) | **alta**: pool de K instâncias, política de "voice stealing" quando >K notas ativas, compositing e sincronismo de viewport/câmera com a instância de página | alta (mesma de M4) |

## Recomendação

**M6**: destaque de nota via **M4 refinado por M2** (animações minúsculas
por nota — ou por grupo de instante simultâneo, para economizar slots em
acordes densos —, nomeadas pelo próprio `xml:id`, tocadas por um pool de K
instâncias de player mantido pelo host) **+** virada de página como um
**engine textualmente separado** dentro do mesmo pacote `.lottie`,
reaproveitando o padrão já validado de state machine em estrela (E1, E4)
aplicado à posição da câmera em vez da cor da nota.

Por quê:

- É o único candidato que não exige abrir mão de nenhum dos três requisitos
  em conflito (simultaneidade, fade autorado, endereço por `xml:id`) —
  todos os outros (M1, M2 puro, M3) exigem relaxar pelo menos um.
- Reaproveita 100% do que já foi validado empiricamente em B01 (o padrão
  segmento+evento+estrela) só que instanciado K+1 vezes em vez de uma —
  não é uma tecnologia nova, é o mesmo primitivo composto de forma diferente.
- Resolve o achado "Extra" (colisão página×destaque) **por construção**,
  não por um mecanismo novo: bastam engines diferentes.
- Continua sendo **um único arquivo `.lottie`** (várias animações e
  state machines dentro do mesmo pacote) — não fere a decisão já tomada de
  "um `.lottie` por música inteira".

O preço é complexidade no **host** (zywny), não no exportador: pool de
instâncias, política de "voice stealing" (o que fazer quando mais de K
notas estão destacadas ao mesmo tempo — sugestão inicial: roubar o slot com
o fade mais avançado, i.e. mais perto do preto), e sincronizar a
transformação (posição de câmera/página) do overlay de destaque com a
instância que desenha a partitura.

## Riscos e itens não testados que a recomendação carrega

1. **🧪 Transições guardadas por múltiplos `Event`s apontando pro mesmo
   estado-alvo (base do M2)** não foi testado isoladamente em B01 — é uma
   extrapolação estrutural de baixo risco (mesmo formato de transição já
   usado nota-a-nota, só que N transições em vez de 1 mirando o mesmo
   estado), mas deveria ser confirmado com um teste pequeno no início da
   Fase C antes de depender dela.
2. **Não sabemos se o(s) runtime(s) de player que o zywny vai usar
   (web/mobile) suportam de forma barata K+2 instâncias simultâneas
   compositadas.** B01 só exercitou um engine por vez (`compare sm-render`).
   Isso é a maior incerteza da recomendação — vale um spike dedicado no
   início da Fase C (C00) usando o player real do zywny (ou o mais próximo
   disponível), antes de comprometer o desenho do exportador a essa rota.
3. **Tamanho de arquivo com N animações extras** (uma por nota, ou por
   grupo de instante) não foi medido — só a escala de *state machines*
   (E5). Como cada glifo já é "assado" por uso (sem `<use>` em Lottie, ver
   README do plano), a animação de destaque de cada nota duplica outline +
   keyframes de cor daquela nota; a ordem de grandeza deve ser comparável
   ao custo que a nota já tem na composição principal, mas isso é uma
   suposição, não um número medido.
4. **Escolha de K** (quantas instâncias de overlay manter) é um trade-off
   memória/compositing × cobertura de polifonia — proposta inicial abaixo,
   mas é decisão do usuário.

## Perguntas explícitas ao usuário

1. **Nível de fidelidade de simultaneidade que o MVP de destaque precisa
   ter, dado que nenhum mecanismo de engine único entrega os três
   requisitos ao mesmo tempo.** M6 (recomendado) entrega os três, mas move
   complexidade real para o host (zywny) — pool de instâncias, "voice
   stealing", compositing. Alternativas mais simples exigem **relaxar
   explicitamente** um requisito do CLAUDE.md:
   - M1: só uma nota destacada por vez (relaxa "notas simultâneas").
   - M2 puro: acordes com onset exatamente igual funcionam, mas vozes com
     onsets diferentes que se sobrepõem no tempo ainda se cancelam (relaxa
     "fades sobrepostos" parcialmente).
   - M3: várias notas acesas ao mesmo tempo, mas sem fade automático — o
     host teria que apagar as notas manualmente (relaxa "fade controlado
     pelo Lottie").
2. **Se M6/M4 for aceito, qual o valor inicial de K** (nº de instâncias de
   overlay mantidas pelo host)? Proposta: **K = 8** (cobre confortavelmente
   destaque de acorde a duas mãos em piano; acima disso, aplica-se a
   política de "voice stealing"). Alternativas: 4 (mais enxuto, cobre
   acordes simples de uma mão), 16 (texturas densas/redução orquestral), ou
   decidir isso só na Fase C com um spike de dimensionamento real.
3. **Confirmar que virada de página deve usar um engine totalmente
   separado do destaque de nota** (em vez de dividir a mesma state machine,
   que o B01 provou colidir). Isso é pré-requisito pra M6 funcionar e para
   a proposta de layout de páginas abaixo.
4. **Vale a pena investir num spike de compositing multi-instância no
   início da Fase C** (item de risco 2 acima) antes de comprometer o
   exportador ao desenho M6, dado que é a maior incerteza não coberta pelo
   B01?

## Proposta para D-LAYOUT-PAGINAS (compatível com M6)

Como o destaque de nota deixa de compartilhar engine com a partitura/página
(M6), o desenho da virada de página fica livre para reaproveitar, sozinho,
o mesmo padrão de estrela já validado (E1, E4), agora aplicado à posição de
uma "câmera" em vez da cor de uma nota:

- **Disposição:** trilha horizontal — cada página é um layer/grupo
  posicionado em `x = pagina_n * largura_da_pagina` dentro da composição
  `score` (todas as páginas na mesma composição, layers lado a lado, como
  já decidido em CLAUDE.md).
- **Câmera:** um grupo-câmera de nível superior cuja translação em X tem um
  keyframe "parado" por página (`PageState_N`, análogo a `PlaybackState`,
  com um marker de 1 frame = câmera parada exatamente na posição da página
  N — mesmo truque do estado mudo `page2` já testado no "Extra" do B01).
- **Transição:** `GlobalState` com uma transição `Tweened` (não
  `Transition` instantânea) de `PageState_N` para `PageState_N+1`, guardada
  por um evento nomeado (ex.: `"page:N+1"`, disparado pelo host). A duração
  e a curva de easing do `Tweened` ficam autoradas no Lottie (ver E4: um
  `Tweened` faz um blend de pose direto entre os dois frames-alvo, **sem
  revelar visualmente páginas intermediárias** — adequado, já que a trilha
  horizontal tem páginas fisicamente entre a atual e a próxima só se o
  salto for maior que 1, o que não deveria acontecer em virada de página
  normal).
- **Efeito "peek" do Synthesia:** a curva de easing do `Tweened` (não o
  host) é responsável por autorar a sensação de "espreitar antes de
  cobrir" — por exemplo, um easing com uma "pré-acumulação" que já desloca
  a câmera parcialmente antes de acelerar. Isso mantém "o host só dispara
  o evento, a curva mora no Lottie" (CLAUDE.md), ao custo de a curva exata
  do "peek" ser fixa por música (não reagir dinamicamente a quão perto o
  estudante está do fim do compasso) — se isso for um problema, é um novo
  ponto a discutir, não coberto por este memorando.
- **Compositing com destaque de nota:** como M6 usa engines separados, o
  host precisa desenhar, a cada frame: (1) a instância `score` (câmera na
  posição corrente); (2) por cima, as até K instâncias de overlay ativas,
  **transladadas pela mesma posição de câmera corrente** — isso é a peça
  de sincronismo mencionada no risco 2 acima, e é responsabilidade do host,
  não do exportador (o exportador só precisa garantir que as coordenadas
  absolutas das animações de destaque batem com as coordenadas absolutas da
  nota dentro da página, no mesmo sistema de coordenadas da composição
  `score`).

Esta proposta não decide o algoritmo exato de easing do "peek" (isso é
trabalho de authoring/design da Fase C, não uma decisão arquitetural), só a
topologia (trilha horizontal + câmera com state machine em estrela +
engine separado do destaque).

## Decisão do usuário

Registrado em 2026-09-13. **Nota sobre o histórico:** a pergunta 1 foi
respondida duas vezes — a primeira resposta ("M6 com M2 embutido") foi
corrigida pelo próprio usuário logo em seguida ("acho que respondi
errado"), e a resposta final (abaixo) **substitui** a primeira por
completo. A tabela/recomendação acima (que aponta M6) permanece como
registro da análise, mas **a decisão final diverge dela** — ver
justificativa no item 1.

1. **Mecanismo de destaque: não é M6 — é M2 + M3 combinados por *modo de
   uso*, mutuamente exclusivos.** O zywny tem dois modos de operação
   distintos, e cada um usa um mecanismo diferente:
   - **Modo automático (playback/demonstração):** **M2** — state machine em
     estrela (padrão E1/E4 já validado), com notas do mesmo instante do
     timemap agrupadas num único estado/segmento (evento disparável por
     qualquer `xml:id` do grupo). Fade com curva autorada no Lottie, como
     já validado. Mantém a limitação já conhecida de M2 puro: vozes com
     onsets *diferentes* que se sobrepõem no tempo ainda se cancelam (só
     notas do *mesmo* instante exato compartilham destaque) — aceito
     conscientemente para este modo.
   - **Modo interativo (aluno tocando ao vivo):** **M3** — um slot de cor
     por nota (`set_color_slot`, nomeado pelo `xml:id` da nota), sem pool
     nem limite de quantas notas podem estar acesas ao mesmo tempo (E6
     confirmou que múltiplos slots estáticos convivem sem depender do
     playhead). Cobre exatamente o caso em que a simultaneidade real
     importa mais — o aluno pode tocar qualquer combinação de notas a
     qualquer momento, sem seguir um roteiro.
   - **Os dois mecanismos são mutuamente exclusivos**, nunca controlam a
     mesma nota ao mesmo tempo — decisão explícita do usuário. Trocar de
     modo exige um protocolo de handoff (levar a state machine do modo
     automático a um estado base/idle antes de ativar slots, e limpar os
     slots que estavam setados antes de voltar ao modo automático) —
     **não desenhado neste memorando**, fica como escopo de C01/C03.
   - Isso muda a conclusão da tabela acima: nenhum candidato único
     satisfazia os três requisitos em conflito simultaneamente (daí M6), mas
     **não é preciso satisfazê-los simultaneamente** — os dois modos de uso
     nunca coexistem, então cada modo pode relaxar um requisito diferente
     (ver item 2) sem que isso seja visível ao mesmo tempo.
   - **M4 e o pool de K instâncias de player ficam descartados** — não são
     mais necessários. M3 não precisa de múltiplas instâncias de player:
     os slots são nomeados dentro da própria animação principal (`score`),
     manipulados diretamente pelo host via `set_color_slot`, sem overlay
     nem compositing extra. Isso simplifica bastante a complexidade no host
     em relação à recomendação original.
2. **Fade autorado pelo Lottie: continua exigido no modo automático (M2),
   relaxado no modo interativo (M3).** No modo interativo o host liga/
   desliga a cor diretamente (imediato, ou calculando ele mesmo uma
   transição), em vez de depender de uma curva autorada. Motivo: slots
   *animados* (`ColorSlot::with_keyframes`) não foram testados em B01 e,
   por leitura de código, provavelmente dependem do mesmo espaço de tempo
   da composição (mesmo problema de playhead único que motivou trocar de
   M1/M2 para M3 no modo interativo) — depender deles arriscaria reintroduzir
   exatamente a limitação que M3 foi escolhido para evitar. Fica como item
   em aberto para uma eventual Fase C+ (não bloqueia o MVP): testar
   `with_keyframes` se um fade autorado no modo interativo vier a ser
   desejado depois.
3. **Sem limite de notas simultâneas no modo interativo.** Um slot de cor
   por nota da partitura inteira; qualquer subconjunto pode estar aceso ao
   mesmo tempo, sem pool nem "voice stealing" (isso só fazia sentido para
   M4/M6, que foram descartados).
4. **Virada de página: engine separado, confirmado; mecanismo refinado
   pelo usuário para dois eventos discretos por fronteira de página** (esta
   parte da decisão **não muda** com a correção da pergunta 1 — é
   independente do mecanismo de destaque escolhido):
   - Evento A — o playhead de reprodução **entra no último compasso da
     página atual**: dispara uma animação pré-autorada de "espreitar"
     (peek), que revela parcialmente a próxima página. Fica "parada" no
     frame final dessa revelação parcial.
   - Evento B — o playhead **entra no primeiro compasso da próxima
     página**: dispara uma segunda animação pré-autorada que remove/cobre
     o que restava da página anterior (o compasso final que ainda estava
     visível), completando a transição.
   - Ambos os eventos são disparáveis diretamente (topologia em estrela,
     mesmo padrão de `PlaybackState`+`segment`+`GlobalState` já validado em
     E1/E4 para notas) — **não precisa da semântica especial de
     `Tweened`** cogitada na proposta original: cada uma das duas
     animações (peek e cover) é um segmento comum, com a curva de
     movimento/opacidade inteiramente autorada como keyframes dentro do
     próprio segmento, do mesmo jeito que o fade de cor de uma nota é
     autorado dentro do marker dela.
   - Este mecanismo de página continua **num engine dedicado**, separado
     do engine da state machine de destaque automático (M2) — os dois
     eventos de página são endereçados por seus próprios nomes (ligados
     aos `xml:id` dos compassos de fronteira, não aos das notas). Isso
     mantém válido o motivo original de separar os engines (evitar a
     colisão do achado "Extra" do B01). Como M3 (modo interativo) não usa
     state machine nem playhead — os slots são aplicados diretamente sobre
     a instância que já está renderizando o `score` —, ele não conflita com
     o engine de página de forma nenhuma; só o engine de M2 (automático)
     precisava dessa separação.
5. **Sem spike dedicado de compositing multi-instância antes da Fase C —
   decisão explícita do usuário: "Não, seguir direto para C00".** O risco
   de compositing **encolheu** com a correção da pergunta 1: antes era
   "K+2 instâncias" (pool de overlays de M6 + página), agora é só **2
   instâncias** (o engine principal `score`, que já basta pro modo
   automático e pro modo interativo via slots, + o engine separado de
   página). Ainda **permanece aberto e não confirmado** se o(s) runtime(s)
   de player do zywny suportam rodar 2 instâncias simultâneas e ler/
   compositar a posição de câmera de uma na renderização visível da outra;
   deve ser o primeiro ponto de atenção prático ao implementar C00, em vez
   de investigado isoladamente antes.

## Notas de execução

- Memorando produzido; primeira resposta à pergunta 1 foi corrigida pelo
  usuário na sessão seguinte, com 3 perguntas de acompanhamento pra fechar
  os detalhes que a correção deixava em aberto (concorrência entre modos,
  fade no modo interativo, limite de notas simultâneas) — todas
  respondidas e incorporadas na seção "Decisão do usuário" acima.
- D-DESTAQUE e D-LAYOUT-PAGINAS **decididos** (versão final: M2+M3 por
  modo, engine de página separado) — replicar em `CLAUDE.md` (seção
  "Decisões arquiteturais já tomadas") e na tabela de decisões/passos de
  `docs/plano/README.md`, corrigindo as referências a M6/K=8 registradas
  anteriormente lá, depois seguir para C00 (que precisa ser reescrito de
  esboço para passos `C01…Cn` executáveis — ainda não feito nesta sessão).
- Risco 1 (transições com múltiplos `Event`s pro mesmo estado-alvo, base de
  M2) segue válido como ponto de atenção da Fase C. Risco 2 (compositing
  multi-instância) segue válido mas com escopo reduzido (2 instâncias, não
  K+2) — ver item 5 da decisão.
- **Protocolo de handoff entre modo automático e modo interativo** (como
  resetar a state machine de M2 e os slots de M3 ao trocar de modo) ficou
  explicitamente não desenhado — é escopo novo para C01/C03, não coberto
  por este memorando.
