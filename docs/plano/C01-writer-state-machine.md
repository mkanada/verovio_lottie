# C01 — Writer de state machine

**Depende de:** A12 (pacote `.lottie`), B01 (formato validado por spike), B02
(mecanismo decidido) · **Decisão:** nenhuma nova — D-DESTAQUE e
D-LAYOUT-PAGINAS já foram decididos em B02. Este passo só constrói a
infraestrutura genérica de serialização (o "writer"), sem decidir
agrupamento de notas por instante nem nomes de evento — isso é C02/C03.

## Objetivo

Dar ao Verovio a capacidade de serializar uma state machine no formato
dotLottie v2 validado empiricamente pelo spike B01 (`s/<id>.json`) e de
registrá-la no `manifest.json` (`stateMachines`), como infraestrutura
genérica reutilizável tanto pelo modo automático (M2, C02/C03) quanto pelo
engine separado de virada de página (C04). Sem gerar ainda os markers/cores
de nota reais nem validar nomes de evento — isso é C02 e C03.

## Ler antes (só isto)

- `docs/plano/decisoes/B02-mecanismo-destaque.md` — decisão final (M2 no
  modo automático + M3 no modo interativo, mutuamente exclusivos; engine
  separado para página).
- `docs/plano/spikes/B01-resultado.md` — E1, E4 (topologia em estrela via
  `GlobalState`, `Transition` é o mecanismo adequado, não `Tweened`) e o
  "Risco 1" da decisão B02 (transições múltiplas guardadas por eventos
  diferentes, mesmo `toState` — base do agrupamento M2, não testada
  isoladamente).
- `compare/fixtures/b01/gen.py` — **formato JSON real**, o mesmo que o
  `dotlottie-rs` aceitou empiricamente em todos os testes de B01 (ground
  truth para este writer; mais confiável que o exemplo ilustrativo do
  README).
- `docs/plano/README.md` seção "Referência rápida: pacote dotLottie v2".
- `verovio/include/vrv/lottiegeometry.h`, `verovio/include/vrv/lottiewriter.h`,
  `verovio/src/lottiewriter.cpp` — padrão a seguir (`FormatNumber`,
  `EscapeJsonString`, `JoinItems`, sem `jsonxx`).
- `verovio/include/vrv/filereader.h` (`ZipFileWriter`).
- `verovio/src/toolkit.cpp` L1845-L1859 (`Toolkit::RenderToDotLottieFile`,
  manifest hoje hardcoded).

## Arquivos

- Criar: `verovio/include/vrv/lottiestatemachine.h`.
- Modificar: `verovio/include/vrv/lottiewriter.h`, `verovio/src/lottiewriter.cpp`
  (novos métodos), `verovio/src/toolkit.cpp` (só o refactor do manifest
  para usar o novo método — **sem** mudar nenhuma assinatura pública).

## O que fazer

1. **IR** (`lottiestatemachine.h`), mínima o suficiente para a topologia em
   estrela já validada (star topology = todas as transições reais vivem no
   `GlobalState`; `PlaybackState`s não têm transições próprias nesta fase):

   ```cpp
   struct LottieSMTransition {
       std::string toState;
       std::string eventInput; // nome do input Event que dispara esta transição
   };

   struct LottieSMState {
       std::string name;
       bool isGlobal = false; // false = PlaybackState, true = GlobalState
       std::string animation; // só PlaybackState
       std::string segment; // só PlaybackState (nome do marker)
       bool autoplay = true; // só PlaybackState
       bool loop = false; // só PlaybackState
       std::vector<LottieSMTransition> transitions; // só GlobalState usa isto na prática
   };

   struct LottieStateMachine {
       std::string id;
       std::string initial;
       std::vector<LottieSMState> states;
   };
   ```

   Note que o agrupamento M2 ("qualquer `xml:id` do grupo dispara o mesmo
   estado") **não precisa de um campo novo**: são só várias
   `LottieSMTransition` diferentes com `eventInput` diferente e o mesmo
   `toState`, dentro do `transitions` do `GlobalState` — confirmar isso
   empiricamente é justamente o critério de aceite do Risco 1 abaixo.
   Guard/input types além de `Event` e transições `Tweened` **não entram**
   (ver "Fora de escopo").

2. **`LottieWriter::WriteStateMachine`** (novo método estático, mesmo
   padrão de `WriteAnimation`: `std::ostringstream` com
   `imbue(std::locale::classic())`, reaproveitar `FormatNumber`/
   `EscapeJsonString`/`JoinItems` já existentes):
   - Top-level: `{"initial":<sm.initial>,"states":[...],"inputs":[...],"interactions":[]}`.
   - `PlaybackState`: `{"name":..,"type":"PlaybackState","animation":..,"segment":..,"autoplay":bool,"loop":bool,"transitions":[]}`.
   - `GlobalState`: `{"name":..,"type":"GlobalState","transitions":[...]}`
     — **sem** `animation`/`segment`/`autoplay`/`loop` (confirmado em
     `gen.py`; o exemplo do README inclui `"animation":""` mas isso não é
     o formato realmente exercitado pelo spike).
   - Transição: `{"type":"Transition","toState":<t.toState>,"guards":[{"type":"Event","inputName":<t.eventInput>}]}`.
   - `inputs`: lista deduplicada (preservando a primeira ordem de
     aparição) de todos os `eventInput` usados em qualquer `transitions` de
     qualquer estado — `{"type":"Event","name":<nome>}` cada. Deriva
     automaticamente em vez de pedir uma lista redundante ao chamador (evita
     o writer produzir um pacote inválido por esquecimento de um input).
3. **`LottieWriter::WriteManifest`** (novo método estático, generaliza o que
   hoje está hardcoded em `Toolkit::RenderToDotLottieFile`):

   ```cpp
   static std::string WriteManifest(const std::string &generator,
       const std::vector<std::string> &animationIds, const std::string &initialAnimation,
       const std::vector<std::string> &stateMachineIds = {}, const std::string &initialStateMachine = "");
   ```

   - `"version":"2"`, `"generator":<generator>`, `"animations":[{"id":..} para cada animationIds]`.
   - Se `stateMachineIds` não vazio: acrescenta `"stateMachines":[{"id":..} ...]`.
   - `"initial":{"animation":<initialAnimation>}`, acrescentando
     `"stateMachine":<initialStateMachine>` só se não vazio. Decidir *se*
     uma state machine deve autoativar (`initial.stateMachine`) é decisão
     de produto/host, não deste passo — o método só oferece a opção.
4. **Refactor sem mudar comportamento**: trocar a string hardcoded em
   `Toolkit::RenderToDotLottieFile` (`src/toolkit.cpp` L1853-L1855) por
   `LottieWriter::WriteManifest(this->GetVersion() + " (verovio_lottie)"... , {"score"}, "score")`
   (mantendo o texto exatamente igual ao gerado hoje — comparar
   `unzip -p` antes/depois). **Não** adicionar parâmetro de state machines
   à assinatura pública de `RenderToDotLottieFile` neste passo — não há
   ainda conteúdo real de state machine pra passar (isso é C02/C03); mudar
   a API pública agora seria especulativo.
5. **Validação do Risco 1 de B02** (transições múltiplas, mesmo alvo):
   escrever um teste standalone fora do CMake (mesmo padrão do `main()`
   sintético de A03): monta uma `LottieStateMachine` com um `GlobalState`
   contendo duas transições diferentes (`eventInput` `"n2"` e `"n2b"`)
   apontando para o **mesmo** `toState` (`"n2"`), serializa com
   `WriteStateMachine`, empacota um `.lottie` de teste com `ZipFileWriter`
   reaproveitando a animação `a/score.json` de
   `compare/fixtures/b01/notes.lottie` (mesmos círculos/markers), e valida
   com `compare sm-render --script "0:fire n2b"` que o estado `n2` é
   alcançado (mesmo pixel vermelho que `fire n2` produziria) — fecha o
   Risco 1 registrado em B02.

## Fora de escopo

- Tipos de guard/input além de `Event` (`Numeric`/`Boolean`/`String`) — não
  usados por nenhum mecanismo decidido em B02.
- Transições `Tweened` — descartadas na decisão final de B02 (nem destaque
  automático nem virada de página precisam: "ambas são segmentos comuns...
  sem precisar da semântica de `Tweened`").
- Agrupamento real de notas por instante do timemap e cores keyframed do
  fade (C02).
- Validação de unicidade/caracteres aceitos em nomes de evento derivados de
  `xml:id` (C03).
- Protocolo de handoff entre modo automático (M2) e modo interativo (M3)
  (mencionado em B02 como escopo C01/C03) — adiado para C03: não há ainda
  nem uma state machine real nem slots de M3 pra coordenar.
- Qualquer wiring de `Toolkit`/CLI que produza automaticamente uma state
  machine a partir da partitura — só faz sentido quando C02/C03 tiverem
  conteúdo real pra oferecer.
- Modo interativo (M3, slots de cor) — não usa state machine, irrelevante
  para este passo.
- Páginas/virada (C04).

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`).
- JSON de `LottieWriter::WriteStateMachine` validado com
  `python3 -m json.tool`.
- `Toolkit::RenderToDotLottieFile` continua produzindo o **mesmo**
  `manifest.json` de antes do refactor (comparação byte-a-byte de
  `unzip -p <pacote> manifest.json`) — o refactor para `WriteManifest` não
  muda comportamento observável.
- Teste standalone do Risco 1: `compare sm-render` no pacote de teste,
  `--script "0:fire n2b"`, mostra a nota `n2` vermelha (mesma amostra de
  pixel que `fire n2` produz em B01/E1) — confirma que múltiplas
  transições com `eventInput` diferente e `toState` igual funcionam no
  `dotlottie-rs`, fechando o Risco 1 de B02.
- `unzip -t` no pacote de teste passa.

## Notas de execução

- IR criada em `verovio/include/vrv/lottiestatemachine.h`
  (`LottieSMTransition`/`LottieSMState`/`LottieStateMachine`), exatamente
  como planejado — sem campo dedicado pro agrupamento M2 (é só reaproveitar
  várias `LottieSMTransition` com o mesmo `toState`).
- `LottieWriter::WriteStateMachine`/`WriteManifest` implementados em
  `lottiewriter.cpp` reaproveitando `EscapeJsonString`/`JoinItems` já
  existentes (nenhum helper novo de formatação precisou ser criado — não há
  números de ponto flutuante no formato da state machine). `inputs` é
  derivado dos `eventInput` das transições (`CollectEventInputs`, dedup
  preservando ordem via `unordered_set` + vetor), como planejado.
- `Toolkit::RenderToDotLottieFile` (`src/toolkit.cpp`) trocado para chamar
  `LottieWriter::WriteManifest(...)`; confirmado que o `manifest.json`
  gerado é **byte-idêntico** ao formato hardcoded anterior (testado
  regenerando `compare/out/c01/chopin.lottie` a partir de
  `corpus/mei/Chopin_Etude_Op10_No9.mei` e inspecionando com `unzip -p`).
- Build limpo: `cmake ../cmake && make -j4` sem warnings novos.
- **Validação do Risco 1 de B02** (fechado): teste standalone em
  `test_risk1.cpp` (fora do CMake, mesmo padrão do `main()` sintético de
  A03 — compilado direto com `g++` linkando `lottiewriter.cpp` +
  `filereader.cpp` e stubs de `LogError`/`LogWarning`/etc. pra evitar
  puxar `vrv.cpp`/o grafo de `Object`), reaproveitando a animação
  `a/score.json` de `compare/fixtures/b01/notes.lottie`. Uma
  `LottieStateMachine` com `GlobalState.transitions` contendo duas
  transições independentes — `{toState:"n2", eventInput:"n2"}` e
  `{toState:"n2", eventInput:"n2b"}` — foi serializada, empacotada com
  `ZipFileWriter` e testada com `compare sm-render --script "0:fire n2b"`:
  resultado **idêntico** a `fire n2` direto (`state=n2`,
  `pixel(150,50)=rgba(255,0,0,255)` nos dois casos). Confirma
  empiricamente que múltiplas transições com `eventInput` diferente e
  `toState` igual funcionam no `dotlottie-rs` exatamente como o mecanismo
  M2 (B02) precisa — não é mais uma extrapolação não testada.
  `unzip -t risk1.lottie` e `python3 -m json.tool` no `s/sm_risk1.json`
  passaram. Arquivos de teste ficaram no scratchpad da sessão, não
  versionados (não fazem parte do código do exportador).
- **Decisão de escopo tomada durante a execução**: `Tweened` foi
  descartado inteiramente do formato (nem o campo `type` da transição é
  variável — sempre `"Transition"`), e o `GlobalState` não emite
  `animation`/`segment`/`autoplay`/`loop`, seguindo o formato real do
  `gen.py` do B01 em vez do exemplo ilustrativo do README (que tinha
  `"animation":""` e usava `Tweened` no `GLOBAL`→`n1`). Nenhuma mudança
  na assinatura pública de `Toolkit::RenderToDotLottieFile` — fica pra
  quando C02/C03 tiverem conteúdo real de nota/evento pra oferecer.
