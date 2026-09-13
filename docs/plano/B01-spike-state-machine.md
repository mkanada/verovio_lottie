# B01 — Spike: state machine com eventos por `xml:id`

**Depende de:** A05 (só do `compare` compilado) · **Decisão:** nenhuma ·
Não altera o Verovio.

## Objetivo

Descobrir, com um exemplo mínimo feito à mão, o que a State Machine v2 do
`dotlottie-rs` consegue fazer diante dos requisitos de destaque, **antes** de
desenhar a fase C. Produz evidência, não código de produção.

Requisitos a testar (ver `CLAUDE.md`):

- (a) disparo pelo host por nome = `xml:id`;
- (b) ir para qualquer nota diretamente (topologia em estrela);
- (c) duração do fade controlada pelo Lottie;
- (d) várias notas simultâneas (acordes, duas mãos) com fades que se sobrepõem;
- (e) virada de página disparada pelo host convivendo com os destaques.

## Ler antes (só isto)

No checkout local do `dotlottie-rs` (localize com
`find ~/.cargo/git/checkouts -maxdepth 3 -type d -name dotlottie-rs`):

- `src/player.rs` L1834-L1855 — `state_machine_load`, `state_machine_load_data`.
- `src/state_machine/mod.rs` L334-L349 (`fire`) e ~L1415-L1430 (GlobalState).
- `src/state_machine/states.rs` L14-L36; `src/state_machine/inputs.rs` L7-L12;
  `src/state_machine/transitions/guard.rs` L52-L71;
  `src/state_machine/transitions/mod.rs` L30-L40 (`"Transition"` é um
  `"Tweened"` de duração zero).
- `src/state_machine/actions/mod.rs` L68-L130 — ações.
- `src/dotlottie/manifest.rs` L20-L40 — `themes`.
- `examples/state_machine.rs` — uso da API.
- `assets/animations/dotlottie/v2/sm-tween.lottie` — exemplo real (`unzip -p`).
- `compare/README.md` — pegadinha de `set_frame`/`render`.

## Arquivos

- Modificar: `compare/src/main.rs`.
- Criar: `compare/fixtures/b01/gen.py`, `compare/fixtures/b01/notes.lottie`
  (gerado), `docs/plano/spikes/B01-resultado.md`.

## O que fazer

1. Subcomando `compare sm-render` (host simulado, reaproveitado na fase C):
   carrega um `.lottie`, carrega uma state machine por id, recebe um roteiro
   `--script "0:fire n1;250:fire n2;260:fire n3"` (milissegundos:ação), avança o
   player em passos fixos (`tick`) e salva PNGs nos instantes pedidos
   (`--snap 0,100,300`).
2. Fixture gerada por `gen.py`:
   - animação 300×100, `fr` 30, três círculos em grupos `nm` = `n1`, `n2`, `n3`;
   - markers `n1` [0,15), `n2` [15,30), `n3` [30,45), `idle` [45,46);
   - no trecho de cada marker, só o círculo correspondente tem cor keyframed
     vermelho → preto; os demais ficam pretos;
   - state machine: um `PlaybackState` por nota (`segment` = marker, `loop` false),
     um para `idle`, e um `GlobalState` com transições guardadas pelos eventos
     `n1`, `n2`, `n3`.
3. Experimentos (guardar PNGs de evidência em `compare/out/b01/`):
   - **E1**: `fire n3` a partir de `idle` vai direto para n3?
   - **E2**: `fire n2` no meio do fade de n1 — o fade de n1 é interrompido?
   - **E3**: `fire n1` e `fire n2` no mesmo instante (acorde) — quantas notas ficam vermelhas?
   - **E4**: `Transition` vs `Tweened` na troca entre notas.
   - **E5**: escala — gerar state machine sintética com 3000 notas; medir tamanho
     de `s/*.json` e tempo de `state_machine_load`.
   - **E6**: slots/temas — dá para mudar a cor de várias notas independentes ao
     mesmo tempo com `SetTheme` ou slots de cor animados
     (`set_color_slot`, `src/renderer/slots/color.rs`) sem depender do playhead?
     Documentar o que foi possível testar.
4. `docs/plano/spikes/B01-resultado.md`: respostas objetivas (sim/não/parcial +
   evidência) para E1–E6 e para os requisitos (a)–(e).

## Fora de escopo

Mudanças no Verovio; escolher o mecanismo (isso é B02, com o usuário).

## Critérios de aceite

- `compare sm-render` funciona com a fixture.
- `docs/plano/spikes/B01-resultado.md` responde E1–E6 e (a)–(e).

## Notas de execução

- `compare sm-render` implementado em `compare/src/main.rs` (host simulado:
  roteiro `ms:fire nome` avançando o player em passos de 1ms via
  `engine.tick`, disparando via `engine.fire`, salvando PNG + amostra de
  pixels em cada `--snap`). `compare lottie-to-png` ganhou `--slot
  id:r,g,b` (`Player::set_color_slot`) e `--sample x,y` pro E6.
- Fixture (`compare/fixtures/b01/gen.py`, stdlib só): pacote `notes.lottie`
  com 3 notas + um marker mudo `page2` (virada de página) + duas state
  machines (`sm_instant`/`sm_tweened`, ver E4) + `sm_scale.json` (3000
  notas sintéticas, E5).
- **Achado que mudou a fixture**: markers de nota não podem ser contíguos
  (`end_frame()` do marker ativo é inclusive — ver
  `docs/plano/spikes/B01-resultado.md` seção "Layout da fixture"); corrigido
  reservando 1 frame de folga entre notas. Vale pra qualquer geração real
  de markers na fase C.
- Todos os critérios de aceite batem: `compare sm-render` funciona com a
  fixture (E1-E4 e o extra de virada de página, todos reproduzidos e
  com números conferidos no resultado); `docs/plano/spikes/B01-resultado.md`
  responde E1-E6 e (a)-(e).
- Achados principais (detalhes e evidência em `B01-resultado.md`): (a) e
  (b) confirmados diretamente; (c) confirmado pra uma nota por vez; (d)
  **não** resolvido pelo mecanismo `PlaybackState`+`segment` (só uma nota
  fica destacada por vez — um único playhead compartilhado, E3), com uma
  rota parcial via slots de cor (E6, acende N notas ao mesmo tempo mas sem
  fade automático comprovado); (e) **não** resolvido se notas e página
  dividirem a mesma state machine/engine (colidem, mesmo mecanismo do E2
  entre domínios diferentes). `Tweened` (E4), ao contrário do que a leitura
  do código sugeria, não faz scrub pelo timeline — é um blend de pose
  ThorVG entre 2 frames, sem vazamento visual de notas intermediárias.
- Essas lacunas ((d) e (e)) são exatamente o que B02 precisa decidir com o
  usuário — não foram resolvidas aqui de propósito (fora de escopo do B01).
