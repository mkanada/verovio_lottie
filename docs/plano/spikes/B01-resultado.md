# B01 — Resultado do spike: state machine com eventos por `xml:id`

**Data:** 2026-09-13 · dotlottie-rs `0.1.58` (checkout local
`~/.cargo/git/checkouts/dotlottie-rs-880a588a6ecaf2f6/eb44c99/dotlottie-rs`,
revisão do `Cargo.lock` do `compare`).

Evidência bruta (PNGs de cada snapshot) em `compare/out/b01/` — não
versionado (`compare/.gitignore`); os números abaixo (amostra de pixel
RGBA no centro de cada círculo, impressos por `compare sm-render`) são a
evidência citável. Para reproduzir, ver "Comandos" no fim de cada seção.

## Ferramental construído

- **`compare sm-render <pacote.lottie> <out_dir> --sm <id>|--sm-file
  <json> --width --height [--script "ms:fire nome;..."] [--snap
  "ms,ms,..."] [--sample x,y]... [--measure-load]`**
  (`compare/src/main.rs`): simula um host — carrega a animação e a state
  machine, dispara eventos num roteiro de tempo (`engine.fire`), avança o
  player em passos de 1ms (`engine.tick`) e salva um PNG + amostra pixels
  em cada instante pedido. Reaproveitável na fase C sem mudanças.
- **`compare lottie-to-png ... --slot id:r,g,b ... --sample x,y`**:
  estendido para aplicar slots de cor (`Player::set_color_slot`) antes de
  renderizar, e amostrar pixels — usado no E6.
- **`compare/fixtures/b01/gen.py`** (stdlib só, sem dependências): gera
  `notes.lottie` (animação `score` 300×100 com 3 círculos `n1`/`n2`/`n3` +
  um marker mudo `page2`, mais as state machines `sm_instant` e
  `sm_tweened`) e `sm_scale.json` (state machine sintética com 3000
  notas, para E5).

### Layout da fixture (`notes.lottie`)

`fr=30`. Markers (`tm`/`dr` em frames): `n1 [0,15]`, `n2 [16,31]`, `n3
[32,47]`, `idle [48,48]`, `page2 [64,64]`. Cada nota tem uma propriedade
de cor (`fl.c`, com `"sid"` `n1Color`/`n2Color`/`n3Color` para o E6)
keyframada: preta, um keyframe de **hold** (`h:1`) até o início do seu
marker, salto pra vermelho exatamente no início, e fade linear de volta a
preto até o fim do marker (500 ms, 15 frames a 30fps).

**Achado de fixture, não de arquitetura:** os markers de nota **não podem
ser contíguos** (`n1 [0,15)`, `n2 [15,30)` como o plano original descrevia).
`Player::end_frame()` é o fim do segmento do marker *ativo*, e é
**inclusive** — o playhead pode ficar parado exatamente no frame
`tm+dr` (`handle_forward_mode` em `player.rs`). Como todas as notas
compartilham **um único playhead** (é a mesma composição `score` pra
todas as camadas), se o frame final de `n2` (`tm+dr=30`, no layout
original sem gap) coincide com o `tm` de `n3` (também 30), a nota `n2`
"presa" nesse frame de fronteira acaba mostrando a cor keyframada de
`n3` (vermelha), mesmo que ninguém tenha disparado `n3`. Corrigido
reservando 1 frame de folga entre notas (`NOTE_SLOT = NOTE_LEN + 1`);
qualquer geração real de markers pela fase C precisa do mesmo cuidado.

## E1 — `fire n3` a partir de `idle` vai direto pra n3?

**Sim.** `compare sm-render ... --sm sm_instant --script "0:fire n3" --snap "0,500"`:

| t (ms) | estado | n1 (50,50) | n2 (150,50) | n3 (250,50) |
| --- | --- | --- | --- | --- |
| 0 | `n3` | 0,0,0 | 0,0,0 | **255,0,0** |
| 500 | `n3` | 0,0,0 | 0,0,0 | 0,0,0 (preto, fade completo) |

Transição direta `idle → n3` num único pipeline run, sem passar por `n1`/`n2`
— confirma a topologia em estrela via `GlobalState` (evaluada independente
do `current_state`, `state_machine/mod.rs` ~L1415-L1430).

## E2 — `fire n2` no meio do fade de `n1`: o fade de `n1` é interrompido?

**Sim, instantaneamente.** `--script "0:fire n1;250:fire n2"`:

| t (ms) | estado | n1 | n2 |
| --- | --- | --- | --- |
| 0 | `n1` | 255,0,0 | 0,0,0 |
| 240 | `n1` | 132,0,0 (~52% do fade) | 0,0,0 |
| 250 | `n2` | **0,0,0** (corta na hora) | **255,0,0** |
| 560 | `n2` | 0,0,0 | 96,0,0 |

`n1` não termina de esmaecer visualmente — ao entrar em `n2`,
`set_marker` move o playhead pro início do marker de `n2` (frame 16), e a
própria curva de cor de `n1` (que já tinha terminado seu fade em frame 15)
avalia como preto nesse novo frame. Não houve "vazamento" da cor de `n1`
pro frame de `n2`.

## E3 — `fire n1` e `fire n2` no mesmo instante (acorde): quantas notas ficam vermelhas?

**Só uma — a última do roteiro, e a outra nunca chega a ser renderizada.**
`--script "0:fire n1;0:fire n2"` → só `n2` fica vermelho (`state=n2`, n1 em
preto) em t=0 e t=50. Com a ordem invertida (`"0:fire n2;0:fire n1"`) → só
`n1` fica vermelho. `fire()` com `run_pipeline=true` roda o pipeline
imediatamente e sincronamente (`state_machine/mod.rs` L334-L349): o segundo
`fire` já encontra `current_state` mudado pelo primeiro e transiciona de
novo antes de qualquer render acontecer — o estado intermediário nunca é
visível.

**Isto é uma limitação de arquitetura, não um bug de timing do roteiro**:
o engine tem exatamente **um** `current_state`/playhead por composição.
Duas notas simultâneas (acorde) não podem ficar vermelhas ao mesmo tempo
via `PlaybackState`+`segment`, não importa a ordem ou o espaçamento dos
`fire`. Ver E6 para uma alternativa parcial.

## E4 — `Transition` vs `Tweened` na troca entre notas

Testado com um salto "longo" (`n1 → n3`, pulando o marker de `n2`), pra
expor diferença de comportamento. `--script "0:fire n1;100:fire n3"`:

**`sm_instant` (`Transition` = `Tweened` de duração zero):** corte seco.
`n1` some e `n3` aparece vermelho no mesmo frame em que o evento é
processado (t=100ms: `n1`=0,0,0, `n3`=255,0,0, estado já é `n3`). `n2`
nunca aparece (0,0,0 o tempo todo) — **nenhum vazamento visual** do
marker que fica "no meio do caminho" no timeline.

**`sm_tweened` (duração 150ms, easing `[0,0,0.58,1]`):** resultado
**não** é um scrub pelo timeline (o que faria `n2`, cujo marker fica
espacialmente entre `n1` e `n3`, piscar vermelho de passagem). É uma
**interpolação direta de pose entre os dois frames-alvo** — `Player::tween`
usa `renderer.tween_to(to)`/`tween_go(progress)` (blend nativo do ThorVG
entre exatamente 2 frames, não um `set_frame` contínuo por todos os frames
intermediários):

| t (ms) | n1 | n2 | n3 | `get_current_state_name()` |
| --- | --- | --- | --- | --- |
| 0 | 0,0,0 (ainda não iniciou o tween) | 0,0,0 | 0,0,0 | `idle` |
| 50 | 125,0,0 | 0,0,0 | 0,0,0 | `idle` |
| 100 | 215,0,0 (tween ainda em curso; evento `n3` interrompe aqui) | 0,0,0 | 0,0,0 | `idle` |
| 140 | 128,0,0 (decrescendo) | 0,0,0 | 102,0,0 (crescendo) | `idle` |
| 200 | 33,0,0 | 0,0,0 | 215,0,0 | `idle` |
| 250 | 0,0,0 | 0,0,0 | 255,0,0 | `n3` |

`n2` fica em preto o tempo todo — **nenhuma nota "de passagem" pisca**,
mesmo com o segundo tween redirecionando o primeiro antes de terminar.
Achado colateral importante: **`get_current_state_name()` só reflete o
estado novo quando o tween *termina*** (`resume_from_tweening`, chamado
de dentro de `engine.tick`) — um tween interrompido por outro nunca
"chega" a definir `current_state` como seu alvo, então uma lógica de host
que consulte o nome do estado atual durante um `Tweened` em andamento vê
um valor desatualizado (aqui, ficou em `idle` de t=0 a t=200, mesmo já
tendo trocado de alvo às pressas em t=100).

Conclusão prática: `Transition` (instantâneo) é o mecanismo adequado pro
destaque de nota (troca limpa, sem side-effect); `Tweened` é utilizável
sem risco de vazamento entre notas não relacionadas, mas muda a semântica
de leitura de estado durante a transição — relevante se a fase C usar
`Tweened` pra virada de página (câmera) ou crossfade de nota.

## E5 — Escala: 3000 notas

`compare/fixtures/b01/gen.py` gera `sm_scale.json` com 3000
`PlaybackState`s + 3000 transições guardadas por evento no `GlobalState` +
3000 `Event` inputs.

- **Tamanho:** 849 260 bytes (~830 KB) de JSON cru; **34 784 bytes (~34
  KB, 4,1%)** comprimido em `deflate` nível 9 — o formato repetitivo
  comprime muito bem, e é assim que entraria no `.lottie` (`s/<id>.json`
  dentro do zip).
- **Tempo de `state_machine_load` (3 execuções, `--measure-load`):**
  12,52ms / 11,24ms / 12,29ms. Pra comparação, a state machine pequena (4
  notas) carrega em ~0,05ms. Ambos os números são desprezíveis frente ao
  tempo de abrir uma partitura inteira — mesmo numa peça de milhares de
  notas, carregar a state machine custaria ~1-2% de um segundo.

Conclusão: escala não é um problema prático nessa faixa (uma música
inteira, milhares de notas) nem pro tamanho do pacote nem pro tempo de
carregamento.

## E6 — Slots/temas: dá pra mudar a cor de várias notas independentemente do playhead?

**Sim, para uma sobreposição estática — e isso resolve o problema do E3**
(mais de uma nota vermelha ao mesmo tempo), mas só parcialmente: não achei
como fazer o *fade automático de volta a preto* funcionar sem depender do
mesmo playhead compartilhado. Testado com `compare lottie-to-png --slot`
(chamando `Player::set_color_slot` diretamente, sem passar pela state
machine), no mesmo frame 0:

| Cenário | n1Color | n2Color | n3Color | resultado |
| --- | --- | --- | --- | --- |
| baseline (sem slot, frame 0) | — | — | — | n1=255,0,0 (keyframe nativo), n2=0,0,0, n3=0,0,0 |
| `--slot n1Color:0,0,0 --slot n2Color:1,0,0` | 0,0,0 | 1,0,0 | — | n1=**0,0,0**, n2=**255,0,0** — inverteu o que o keyframe nativo diria pro frame 0 |
| `--slot n1Color:1,0,0 --slot n3Color:1,0,0` (frame 0) | 1,0,0 | — | 1,0,0 | n1=**255,0,0** *e* n3=**255,0,0** ao mesmo tempo |
| mesmos slots, frame 44 (dentro da janela nativa de n3, que já estaria esmaecendo) | 1,0,0 | — | 1,0,0 | idêntico ao frame 0: n1=255,0,0, n3=255,0,0 — **o slot ignora completamente o frame/keyframe nativo** |

Confirma (`src/renderer/mod.rs` `set_color_slot`/`flush_slots`,
`slots_dirty`): um slot de cor **estático** substitui o valor keyframado
por completo, não interpola com ele, e é aplicado no próximo `render()`
**independente de qual frame está corrente**. Isso permite acender N notas
simultaneamente sem tocar no `current_state`/playhead da state machine —
uma rota real para o requisito (d) que o mecanismo de `segment` (E3) não
cobre.

**O que não ficou resolvido, e não dava pra testar neste spike sem
inventar arquitetura (fora de escopo aqui, é decisão de B02):** um slot
também aceita keyframes (`ColorSlot::with_keyframes`/`LottieProperty`),
mas cada `LottieKeyframe` tem um campo `frame: f32` — pela leitura do
código (`src/renderer/slots/mod.rs` L44-50), esse "frame" é o mesmo
espaço de tempo da composição, ou seja, um slot *animado* provavelmente
reintroduz a dependência do playhead único (não testado empiricamente).
Ou seja: dá pra **acender** várias notas ao mesmo tempo via slot estático,
mas o **fade automático controlado pelo Lottie** (requisito CLAUDE.md)
de cada uma, se usado overlapping, ainda não tem um mecanismo comprovado
nesse spike — precisaria ou (a) o host repetir `set_color_slot` a cada
tick com a cor calculada (o que fere "duração controlada pelo Lottie, não
pelo host"), ou (b) investigar `ColorSlot::with_keyframes` combinado com
alguma forma de dar a cada nota seu próprio "tempo zero" — isso é
trabalho de design pra B02, não deste spike.

## Extra: virada de página convive com destaques? (requisito e)

Não implementável de verdade nesta fase (sem exportador de virada de
página ainda), mas dava pra testar a pergunta estrutural: **um evento de
virada de página, numa state machine que compartilha o mesmo
`current_state` das notas, cancela o destaque em andamento?**

Adicionei um estado mudo `page2` (marker de 1 frame, sem cor associada) à
mesma `sm_instant`/`sm_tweened`, disparável como qualquer nota
(`GlobalState` → `Transition` guardado por `Event page2`).
`--script "0:fire n1;100:fire page2"`:

| t (ms) | estado | n1 |
| --- | --- | --- |
| 50 | `n1` | 229,0,0 (em fade) |
| 100 | `page2` | **0,0,0** — o destaque morre na hora |

**Sim, colidem.** Enquanto notas e página dividirem o mesmo
`current_state`/playhead de uma única state machine/engine, disparar a
virada de página cancela qualquer destaque em andamento (mesmo
mecanismo do E2, entre domínios diferentes). Se a fase C precisar dos
dois rodando de forma independente (nota continuando a esmaecer durante
uma virada de página), a topologia atual (uma state machine, um
`current_state`) não serve como está — precisa ou de dois engines/players
compostos, ou side-channel via slots (E6) pro destaque, deixando a state
machine "de verdade" só para a posição de página.

## Respostas aos requisitos (CLAUDE.md)

| Requisito | Resposta |
| --- | --- |
| (a) disparo pelo host por nome = `xml:id` | **Sim** — `engine.fire(xml_id, true)`, casando com um `Event` input de mesmo nome (E1-E4). |
| (b) ir direto pra qualquer nota (estrela) | **Sim** — via `GlobalState`, comprovado em E1 (idle→n3 direto) e E4 (n1→n3 sem passar visualmente por n2). |
| (c) duração do fade controlada pelo Lottie | **Sim**, para uma nota de cada vez — a duração é o comprimento do marker (frames/`fr`), authored no `.lottie`, não enviada pelo host (E1, E2). |
| (d) várias notas simultâneas com fades sobrepostos | **Não, via `segment`/`PlaybackState`** (E3: só a última nota do roteiro fica vermelha — um único playhead compartilhado). **Parcialmente via slots de cor** (E6: acender N notas ao mesmo tempo funciona; o fade automático *sobreposto* de cada uma não tem mecanismo comprovado neste spike). Fica pra B02 decidir a rota. |
| (e) virada de página convivendo com destaques | **Não, se dividirem a mesma state machine/engine** (achado extra acima) — colidem porque só existe um `current_state`. Precisa de desenho explícito em B02 (motores separados ou side-channel). |

## Comandos pra reproduzir

```sh
cd compare/fixtures/b01 && python3 gen.py   # gera notes.lottie e sm_scale.json
cd ../../..
cargo build --release --manifest-path compare/Cargo.toml
B=./compare/target/release/compare
L=compare/fixtures/b01/notes.lottie
O=compare/out/b01
mkdir -p "$O"

# E1
$B sm-render $L $O --sm sm_instant --width 300 --height 100 \
  --script "0:fire n3" --snap "0,500" --prefix e1 \
  --sample 50,50 --sample 150,50 --sample 250,50

# E2
$B sm-render $L $O --sm sm_instant --width 300 --height 100 \
  --script "0:fire n1;250:fire n2" --snap "0,240,250,560" --prefix e2 \
  --sample 50,50 --sample 150,50 --sample 250,50

# E3
$B sm-render $L $O --sm sm_instant --width 300 --height 100 \
  --script "0:fire n1;0:fire n2" --snap 0 --prefix e3 \
  --sample 50,50 --sample 150,50 --sample 250,50

# E4 (instant vs tweened)
$B sm-render $L $O --sm sm_instant --width 300 --height 100 \
  --script "0:fire n1;100:fire n3" --snap "0,100,200" --prefix e4-instant \
  --sample 50,50 --sample 150,50 --sample 250,50
$B sm-render $L $O --sm sm_tweened --width 300 --height 100 \
  --script "0:fire n1;100:fire n3" --snap "0,50,100,140,200,250" --prefix e4-tweened \
  --sample 50,50 --sample 150,50 --sample 250,50

# E5
$B sm-render $L $O --sm-file compare/fixtures/b01/sm_scale.json \
  --width 300 --height 100 --measure-load --prefix scale

# E6
$B lottie-to-png $L $O/e6.png --width 300 --height 100 --frame 0 \
  --slot n1Color:1,0,0 --slot n3Color:1,0,0 \
  --sample 50,50 --sample 150,50 --sample 250,50

# extra (virada de página)
$B sm-render $L $O --sm sm_instant --width 300 --height 100 \
  --script "0:fire n1;100:fire page2" --snap "50,100" --prefix epage \
  --sample 50,50
```
