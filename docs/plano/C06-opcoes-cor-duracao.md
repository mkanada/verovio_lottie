# C06 — Opções de cor/duração do destaque e da virada de página

**Depende de:** C02 (M2, cor/duração de destaque hoje hardcoded),
C04 (páginas/virada, duração de "espreitar"/"cobrir" hoje hardcoded) ·
**Decisão necessária:** nenhuma nova — este passo só existe "se o usuário
pedir" (C00, C02, C03, C04 todos adiaram isso explicitamente para cá com essa
condição); o usuário pediu explicitamente a execução deste passo nesta
sessão, então a condição já está satisfeita.

## Objetivo

Desde C02, a cor de destaque (`0xE53935`) e a duração do fade
(`kHighlightDurationFrames = 20`) são constantes hardcoded dentro de
`Toolkit::RenderToDotLottieFile`/`RenderToDotLottieHighlightFile`. Desde C04,
o mesmo vale para a duração do "espreitar"/"cobrir" da virada de página
(`kPeekDurationFrames = 15`, `kCoverDurationFrames = 20`) e a fração de
"espreitar" (`peekFraction`, default `0.08`). Este passo expõe essas cinco
constantes como opções de linha de comando, no mesmo padrão de registro já
usado pelas opções `svg*` (`verovio/src/options.cpp` L1177-1189) — `SetInfo`
+ `Init` + `Register`, sem nenhuma outra mudança de arquitetura. Passar a
usar essas opções sem o usuário informar nenhuma delas precisa continuar
produzindo o **mesmo** pacote de hoje (mesmos defaults).

## Decisões de escopo tomadas aqui

- **Cor e as três durações/fração viram opções; o "gap" entre segmentos
  (`kHighlightGapFrames`/`kPageGapFrames = 1`) não.** O esboço C00 fala
  especificamente em "cor/duração", e C04 (o único passo que menciona C06 de
  novo depois de C02/C03) generaliza para "cor/duração/fração de espreitar"
  — nunca menciona o `gap`. O `gap` não é um parâmetro de produto (não muda
  a aparência do destaque para o usuário final), é a margem de segurança
  descoberta empiricamente em B01/E2 para o marker de um segmento não
  colidir com o início do próximo; deixar configurável abriria a
  possibilidade de alguém zerar isso e reintroduzir silenciosamente o bug
  já corrigido, sem nenhum ganho de produto em troca. Fica fixo.
- **As opções entram no grupo `m_general` (`OptionsCategory::General`), não
  num grupo novo dedicado.** `OptionsCategory` é um `enum class` fechado
  (`verovio/include/vrv/options.h` L98) compartilhado por outros
  consumidores das categorias de opções (`Toolkit::GetAvailableOptions`/
  filtros de binding, `verovio/src/toolkit.cpp` L1407/L1427); criar uma
  categoria nova só para 4 opções deste exportador aumentaria o raio de
  mudança sem necessidade. Como o próprio C00 aponta a linha das opções
  `svg*` — que também vivem em `m_general`/`General` — como o padrão a
  seguir, reaproveitar o mesmo grupo é tanto a leitura mais literal da
  instrução quanto a de menor risco. (O grupo dedicado `m_midi` existe para
  um número bem maior de opções específicas de MIDI; não é o padrão que C00
  pediu para seguir aqui.)
- **Cor como string hex de 6 dígitos (`"E53935"`, sem `#`), não como inteiro
  decimal nem três componentes RGB separados.** Combina com o literal já
  hardcoded hoje (`0xE53935`) e com o formato mental de cor mais comum
  (hex); `parse_color_slot` do `compare` já usa um formato diferente
  (`id:r,g,b` 0-1 por causa da API do `dotlottie-rs`), mas aquele é um
  parâmetro de teste de baixo nível, não a interface deste exportador.
  Entrada inválida (não exatamente 6 dígitos hex) cai para o default via
  `LogWarning`, não aborta a exportação — mesmo padrão de tolerância a
  entrada malformada já usado por `LottieHighlightBuilder::BuildGroups`
  (nomes reservados/ids duplicados, C03).
- **As opções afetam `RenderToDotLottieFile` e `RenderToDotLottieHighlightFile`
  igualmente** (cor/duração de destaque, já que os dois formatos geram M2);
  **as três opções de página só têm efeito em `RenderToDotLottieFile`**
  (`RenderToDotLottieHighlightFile` é sempre uma página isolada, sem câmera —
  decisão de C02 nunca revista). `RenderToLottieAnimation`/formato `lottie`
  continuam sem nenhum destaque (nenhuma dessas opções se aplica).

## Ler antes (só isto)

- `verovio/src/options.cpp` L1051-1058 (bloco `m_landscape`/
  `m_minLastJustification` — ponto exato de inserção alfabética dentro de
  `m_general`) e L1177-1189 (`svg*`, o padrão de registro a copiar:
  `SetInfo(rótulo, descrição)` + `Init(default, ...)` + `Register(&campo,
  "chaveCamelCase", &m_general)`).
- `verovio/include/vrv/options.h` L637-691 (bloco de campos `OptionBool`/
  `OptionInt`/`OptionDbl`/`OptionString` de `m_general`, em ordem
  alfabética — os cinco campos novos entram entre `m_landscape` e
  `m_minLastJustification`) e L221-260/269-308/317+ (`OptionDbl`/`OptionInt`/
  `OptionString`: assinaturas de `Init`, sem surpresa nenhuma).
- `verovio/src/toolkit.cpp` L1848-1965 (`RenderToDotLottieFile`,
  `RenderToDotLottieHighlightFile`) — as cinco constantes hoje hardcoded
  (`kFirstHighlightFrame`, `kHighlightDurationFrames`, `kHighlightGapFrames`,
  cor `0xE53935` passada direto pra `WriteAnimation`; e, só na primeira,
  `kPeekDurationFrames`, `kCoverDurationFrames`, `kPageGapFrames`,
  `peekFraction` implícito no default de `WriteAnimation`).
- `verovio/include/vrv/lottiewriter.h` (assinatura de `WriteAnimation`,
  já aceita `highlightColor`/`peekFraction` como parâmetros — este passo só
  passa valores vindos de `Options` em vez dos literais hardcoded, não muda
  a assinatura).
- `docs/plano/C02-notas-animadas.md`/`C04-paginas-virada.md`, seções "Fora de
  escopo" — cada uma cita C06 como o lugar reservado para isso.

## Arquivos

- Modificar: `verovio/include/vrv/options.h` (5 novos campos `Option*` em
  `m_general`), `verovio/src/options.cpp` (registro dos 5 campos),
  `verovio/src/toolkit.cpp` (lê as opções em vez das constantes locais; novo
  helper `ParseLottieHighlightColor`).
- Nenhum arquivo novo.

## O que fazer

1. **`verovio/include/vrv/options.h`**: entre `OptionBool m_landscape;` e
   `OptionDbl m_minLastJustification;`, cinco campos novos (ordem
   alfabética das chaves, mesmo critério do resto do arquivo):
   ```cpp
   OptionString m_lottieHighlightColor;
   OptionInt m_lottieHighlightDuration;
   OptionInt m_lottiePageCoverDuration;
   OptionInt m_lottiePagePeekDuration;
   OptionDbl m_lottiePagePeekFraction;
   ```

2. **`verovio/src/options.cpp`**: entre o bloco de registro de `m_landscape`
   (L1051-1053) e o de `m_minLastJustification` (L1055-1058), mesmo estilo
   (`SetInfo(rótulo, descrição)` citando o default atual; `Init` com os
   mesmos valores hoje hardcoded em `toolkit.cpp` como default/min/max;
   `Register(&campo, "chave", &m_general)`):
   ```cpp
   m_lottieHighlightColor.SetInfo("Lottie highlight color",
       "Color used to flash a note (or M2 group) on destaque, as a 6-digit hex string "
       "without '#' (e.g. \"E53935\"); fades back to the note's own resolved color");
   m_lottieHighlightColor.Init("E53935");
   this->Register(&m_lottieHighlightColor, "lottieHighlightColor", &m_general);

   m_lottieHighlightDuration.SetInfo("Lottie highlight duration",
       "Duration of the note highlight fade, in frames at the animation's fixed 30fps");
   m_lottieHighlightDuration.Init(20, 1, 300);
   this->Register(&m_lottieHighlightDuration, "lottieHighlightDuration", &m_general);

   m_lottiePageCoverDuration.SetInfo("Lottie page-turn cover duration",
       "Duration of the page-turn \"cover\" phase (camera completes the move to the next "
       "page), in frames at the animation's fixed 30fps");
   m_lottiePageCoverDuration.Init(20, 1, 300);
   this->Register(&m_lottiePageCoverDuration, "lottiePageCoverDuration", &m_general);

   m_lottiePagePeekDuration.SetInfo("Lottie page-turn peek duration",
       "Duration of the page-turn \"peek\" phase (camera hints at the next page), in "
       "frames at the animation's fixed 30fps");
   m_lottiePagePeekDuration.Init(15, 1, 300);
   this->Register(&m_lottiePagePeekDuration, "lottiePagePeekDuration", &m_general);

   m_lottiePagePeekFraction.SetInfo("Lottie page-turn peek fraction",
       "Fraction (0-1) of the distance to the next page the camera moves during the "
       "\"peek\" phase before pausing and \"covering\" the rest");
   m_lottiePagePeekFraction.Init(0.08, 0.0, 1.0);
   this->Register(&m_lottiePagePeekFraction, "lottiePagePeekFraction", &m_general);
   ```
   (Confirmar depois de registrado, com `verovio -h` ou lendo o parser de
   `Toolkit::ParseCommandLine`, que a chave camelCase vira mesmo a flag de
   CLI — mesmo mecanismo dinâmico que já expõe `svgViewBox` etc. sem
   nenhuma linha em `tools/main.cpp`; não é escopo deste passo mudar esse
   mecanismo, só confirmar que as opções novas passam por ele sem
   necessidade de wiring adicional.)

3. **`verovio/src/toolkit.cpp`**: novo helper de arquivo (mesmo padrão
   `static` de função livre dentro de `namespace vrv` já usado em
   `lottiehighlight.cpp`), antes de `Toolkit::RenderToDotLottieFile`:
   ```cpp
   // Parses a 6-digit hex string (no leading '#', matching the exporter's own
   // 0xE53935-style literal default) into a 24-bit RGB int; falls back to
   // defaultColor with a warning on anything else instead of aborting the
   // export (same tolerance-to-malformed-input pattern as
   // LottieHighlightBuilder's reserved-name/duplicate-id checks, C03).
   static int ParseLottieHighlightColor(const std::string &hex, int defaultColor)
   {
       const bool valid = (hex.size() == 6)
           && std::all_of(hex.begin(), hex.end(), [](unsigned char c) { return std::isxdigit(c); });
       if (!valid) {
           LogWarning("Invalid lottieHighlightColor '%s' (expected 6 hex digits, e.g. \"E53935\"); "
                      "using default.",
               hex.c_str());
           return defaultColor;
       }
       return static_cast<int>(std::stoul(hex, nullptr, 16));
   }
   ```
   Adicionar `#include <algorithm>` e `#include <cctype>` ao topo do arquivo
   se ainda não estiverem incluídos.

4. **`Toolkit::RenderToDotLottieHighlightFile`**: trocar
   ```cpp
   const int kFirstHighlightFrame = 1;
   const int kHighlightDurationFrames = 20;
   const int kHighlightGapFrames = 1;
   ```
   por
   ```cpp
   const int kFirstHighlightFrame = 1;
   const int kHighlightDurationFrames = m_options->m_lottieHighlightDuration.GetValue();
   const int kHighlightGapFrames = 1;
   const int highlightColor = ParseLottieHighlightColor(m_options->m_lottieHighlightColor.GetValue(), 0xE53935);
   ```
   e passar `highlightColor` em vez do literal `0xE53935` na chamada de
   `LottieWriter::WriteAnimation` mais abaixo. `kFirstHighlightFrame`/
   `kHighlightGapFrames` continuam fixos (ver "Decisões de escopo").

5. **`Toolkit::RenderToDotLottieFile`**: mesma troca de
   `kHighlightDurationFrames`/`highlightColor` (passo 4), mais:
   ```cpp
   const int kPeekDurationFrames = m_options->m_lottiePagePeekDuration.GetValue();
   const int kCoverDurationFrames = m_options->m_lottiePageCoverDuration.GetValue();
   const int kPageGapFrames = 1;
   ```
   e passar `m_options->m_lottiePagePeekFraction.GetValue()` como último
   argumento (`peekFraction`) na chamada de `LottieWriter::WriteAnimation`
   (hoje omitido, usando o default `0.08` do parâmetro — trocar para
   explícito não muda o valor quando a opção não é passada na CLI, já que o
   `Init` do passo 2 usa o mesmo `0.08`).

## Fora de escopo

- Opção pra `kHighlightGapFrames`/`kPageGapFrames` (ver "Decisões de
  escopo").
- Opção pra frame rate (`fr:30`, `lottiewriter.cpp` L637) — nunca foi citada
  como pendente em nenhum passo anterior; mudar isso teria efeito colateral
  em toda conta de frame já feita em C02-C04 (duração em frames, não em
  ms), risco desproporcional a um pedido que ninguém fez.
- Opções de cor/duração independentes para M3 (slot de cor interativo) — M3
  não tem fade autorado (decisão de B02: "o host liga/desliga a cor
  diretamente"), não há "duração" pra configurar; a cor de M3 nem é
  escolhida pelo exportador (o host escolhe via `set_color_slot` na hora).
- Validação de opções fora do que já é feito pelo próprio `OptionInt`/
  `OptionDbl` (`min`/`max` já clampam/rejeitam na leitura da CLI, mecanismo
  existente do framework de `Options`, não deste passo).
- Mudar `tools/main.cpp` — nenhuma linha nova de CLI wiring é necessária
  (mecanismo dinâmico existente, ver passo 2).
- Atualizar `compare/scripts/sm-playback.sh` (C05) para ler essas opções
  dinamicamente — já registrado como dívida aceitável na seção "Fora de
  escopo" de C05.

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`).
- **Não-regressão**: `dotlottie`/`dotlottie-highlight` gerados sem nenhuma
  das cinco opções novas na CLI produzem saída **byte-idêntica** à de antes
  deste passo, para 2-3 peças do corpus (`-x 42`, mesmo padrão de
  regressão de C01-C04) — confirma que os `Init(...)` escolhidos batem
  exatamente com as constantes hardcoded que substituem.
- `verovio -t dotlottie --lottie-highlight-color 00FF00 --lottie-highlight-duration 10
  -x 42 -o <out> corpus/mei/Scarlatti_Sonata_in_C-major.mei`: `a/score.json`
  do pacote gerado tem a cor de destaque `00ff00` (não `e53935`) e a duração
  do marker `hl0` é `10` frames (não `20`) — confirma que as duas opções de
  destaque realmente chegam ao `LottieWriter`. (Nota: a chave de registro é
  camelCase — `lottieHighlightColor` — mas o parser de CLI dinâmico expõe
  isso como flag **kebab-case**, `--lottie-highlight-color`; confirmar com
  `verovio -h general` antes de assumir a grafia.)
- `verovio -t dotlottie --lottie-page-peek-duration 5 --lottie-page-cover-duration 8
  --lottie-page-peek-fraction 0.2 -x 42 -o <out>
  corpus/mei/Scarlatti_Sonata_in_C-major.mei` (peça com 3 páginas): os
  markers `peek1`/`cover1` em `a/score.json` têm `dr` 5/8 (não 15/20) —
  confirma que as três opções de página chegam a
  `LottiePageTurnBuilder::BuildLayout`/`LottieWriter::WriteAnimation`.
- `verovio -t dotlottie --lottie-highlight-color xyz -x 42 -o <out>
  corpus/mei/Scarlatti_Sonata_in_C-major.mei` não aborta, emite um
  `LogWarning` mencionando `lottieHighlightColor`/`xyz`, e o pacote gerado
  usa a cor default `e53935` — confirma o fallback tolerante.
- `python3 -m json.tool` valida `a/score.json`/`s/sm_highlight.json`/
  `s/sm_page.json` de um pacote gerado com as cinco opções não-default ao
  mesmo tempo.
- `unzip -t` no pacote gerado passa.
- `compare/scripts/compare-page.sh`/`compare-corpus.sh` continuam passando
  sem erro sobre o corpus (mesma varredura de A13/C04) usando os defaults
  (nenhuma opção nova passada) — confirma que nada no caminho comum
  regrediu.

## Notas de execução

Implementado exatamente como planejado nos passos 1-5, com um achado sobre a
grafia da flag de CLI (não muda nenhum código, só a documentação) e uma
correção de um bug introduzido durante a própria escrita deste passo
(pego pelo compilador, não por teste manual).

- **Achado sobre a grafia da flag**: `verovio -h general` mostra as cinco
  opções expostas como **kebab-case** (`--lottie-highlight-color`,
  `--lottie-highlight-duration`, `--lottie-page-cover-duration`,
  `--lottie-page-peek-duration`, `--lottie-page-peek-fraction`), não
  camelCase — o parser de CLI dinâmico (`Toolkit::ParseCommandLine`) converte
  a chave de registro (`Register(&campo, "lottieHighlightColor", ...)`) para
  kebab-case ao gerar a flag, mesmo mecanismo que já expõe `--xml-id-seed`
  a partir da chave interna `"xmlIdSeed"`. As primeiras tentativas de teste
  deste passo usaram `--lottieHighlightColor` (camelCase, como a redação
  original dos "Critérios de aceite" e do passo 2 sugeriam) e falharam com
  `unrecognized option`; corrigido nos dois lugares deste arquivo depois de
  confirmar a grafia real com `verovio -h general`. Nenhuma mudança de
  código — só documentação; o registro em `options.cpp` continua camelCase
  (`"lottieHighlightColor"`), como todo o resto do arquivo já faz.
- **Bug pego pelo compilador, não pelo plano**: a primeira versão do passo 4
  calculava `highlightColor` em `RenderToDotLottieHighlightFile` mas
  esqueceu de passá-lo pra `LottieWriter::WriteAnimation` (a chamada
  continuou com o literal `0xE53935` hardcoded) — `make` acusou
  `warning: unused variable 'highlightColor'` no primeiro build. Corrigido
  antes de qualquer teste manual; sem esse warning, o teste de
  `--lottie-highlight-color` teria passado silenciosamente só para
  `RenderToDotLottieFile` (que estava correto) e falhado silenciosamente só
  no formato `dotlottie-highlight` — reforça o valor de prestar atenção em
  warnings novos do build, não só erros.
- **Não-regressão confirmada byte-a-byte** (não só por leitura de código):
  a primeira tentativa de comparação usou um artefato "antes" gerado sem
  `-x 42` (smoke test de build, feito antes deste passo começar) contra um
  "depois" com `-x 42` — deu um diff enorme, mas inspecionando o byte
  divergente confirmou que era só `xml:id` aleatório trocando (`u1owbd09`
  vs `jrtr619`, etc.), não conteúdo de destaque — falso positivo de
  metodologia, não uma regressão real (ver `docs/plano/C05-host-simulado-timemap.md`
  sobre `-x`/`xml:id` não ser determinístico sem seed). Refeito
  corretamente: `git stash push -- verovio/include/vrv/options.h
  verovio/src/options.cpp` (100% conteúdo novo de C06, seguro stashear
  inteiro) + reversão manual das 5 edições em `toolkit.cpp` (usando os
  mesmos `old_string`/`new_string` das edições originais, de trás pra
  frente) reproduziu fielmente o estado "antes de C06, depois de C04" sem
  precisar descartar nenhum trabalho de sessões anteriores. Rebuild (nota:
  como `options.h` é incluído por ~60 outros arquivos — `doc.h`,
  `svgdevicecontext.h`, etc. — mudar esse header força recompilação quase
  completa do projeto, não só de `toolkit.cpp`; o build "antes" e o build
  "depois" levaram vários minutos cada por causa disso, não é um sinal de
  problema). Gerados `dotlottie`/`dotlottie-highlight` (`-x 42`, sem
  nenhuma das cinco opções novas) para 3 peças do corpus
  (`Scarlatti_Sonata_in_C-major`, `Chopin_Mazurka_Op6_No1`,
  `Chopin_Etude_Op10_No9`) antes e depois de reaplicar as edições de C06:
  **`a/score.json`, `manifest.json`, `s/sm_highlight.json` e
  `s/sm_page.json` byte-idênticos nos dois formatos, nas três peças** —
  confirma que os `Init(...)` do passo 2 batem exatamente com as constantes
  hardcoded que substituíram.
- **As três opções de destaque/cor validadas com override real** (peça
  `Scarlatti_Sonata_in_C-major.mei`, `-x 42`):
  `--lottie-highlight-color 00FF00 --lottie-highlight-duration 10` produz
  um pacote com o marker `hl0` em `dr:10` (era `20`) e keyframes de cor
  `[0, 1, 0, 1]` (verde) espalhados pelo `a/score.json` onde antes havia
  `[0.898, 0.224, 0.208, 1]` (vermelho, `0xE53935` normalizado) — confirma
  que as duas opções chegam ao `LottieWriter` de ponta a ponta, não só que
  compilam.
- **As três opções de página validadas com override real** (mesma peça, 3
  páginas): `--lottie-page-peek-duration 5 --lottie-page-cover-duration 8
  --lottie-page-peek-fraction 0.2` produz markers `peek1`/`cover1` com
  `dr:5`/`dr:8` (eram `15`/`20`) — confirma que as três opções chegam a
  `LottiePageTurnBuilder::BuildLayout`/`WriteAnimation`. (Não verificado
  numericamente o efeito da fração em si — `peekFraction` só desloca *onde*
  a câmera para durante o "espreitar", não um campo visível diretamente no
  JSON de forma tão direta quanto `dr`; o teste de C04 já cobre a mecânica
  de recorte da câmera, este passo só confirma que o valor passado pela CLI
  chega ao parâmetro certo.)
- **Fallback de cor inválida validado** (`--lottie-highlight-color xyz`):
  emite `[Warning] Invalid lottieHighlightColor 'xyz' (expected 6 hex
  digits, e.g. "E53935"); using default.` no stderr, não aborta, e o
  pacote gerado usa `[0.898, 0.224, 0.208, 1]` (o default `0xE53935`) —
  confirma o fallback tolerante do passo 3.
- `python3 -m json.tool` e `unzip -t` passaram em todos os pacotes gerados
  com opções não-default (`color-test.lottie`, `page-test.lottie`,
  `badcolor-test.lottie`).
- **Varredura do corpus inteiro com os defaults** (`compare-page.sh` numa
  peça + `compare-corpus.sh` no corpus completo, 10 peças/34 páginas,
  tolerância 32, mesmo padrão de A13/C04): rodou sem erro, e o CSV
  resultante bate **exatamente** com os números já publicados em
  `docs/plano/relatorio-paridade.md`/registrados nas notas de C04 (ex.:
  `Chopin_Etude_Op10_No9` p.1 `0,4842%`, `Clair_de_Lune__Debussy` p.1
  `0,7658%`) — confirma que nenhum caminho comum (sem opções novas na CLI)
  regrediu visualmente em nenhuma peça do corpus.
- Build limpo (`cmake ../cmake && make -j4`, `cargo build --release` do
  `compare`) depois da correção do warning; nenhum warning novo
  remanescente.
- Arquivos de teste (`compare/out/c06/*` — na verdade gerados no
  scratchpad da sessão, não em `compare/out/`, já que eram só para validar
  este passo — e `compare/out/corpus/*`) não foram versionados.
- Nenhum desvio de escopo: as cinco opções ficaram exatamente como
  desenhado (cor + duração de destaque, três de página), `gap` continua
  fixo, nenhuma mudança em `tools/main.cpp`.
