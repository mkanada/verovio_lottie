# Plano de implementação — exportador dotLottie no Verovio

Este diretório divide o trabalho em passos pequenos, cada um executável por um
modelo com contexto limitado. Leia **só** este README, o `CLAUDE.md` da raiz e o
arquivo do passo que for executar — cada passo lista exatamente quais trechos de
código abrir.

Contexto do projeto: [`docs/descricao-do-projeto.md`](../descricao-do-projeto.md).
Todas as referências de linha foram verificadas no Verovio **6.3.0** vendorizado
em `verovio/`. Se uma linha "andou" por causa de edições anteriores, localize o
símbolo com `graft grep "<símbolo>"`.

## Como executar um passo

1. Leia `CLAUDE.md`, as seções **Convenções** e **Mapa do código** abaixo, e o
   arquivo do passo.
2. Confira na tabela de passos se as dependências estão concluídas.
3. Se o passo tiver um bloco **Decisão necessária**, pare e pergunte ao usuário
   antes de escrever código. Não decida sozinho.
4. Implemente só o escopo do passo (a seção "Fora de escopo" existe para isso).
5. Rode os critérios de aceite.
6. Marque o passo como `concluído` na tabela abaixo e registre em "Notas de
   execução" (no final do arquivo do passo) qualquer desvio ou descoberta que
   afete passos seguintes.
7. Não faça commit nem push sem o usuário pedir.

## Convenções

- **Build do Verovio** (a partir da raiz do repositório):
  `cd verovio/tools && cmake ../cmake && make -j4`. Sempre que criar um `.cpp`
  novo, rode o `cmake ../cmake` de novo: o CMake coleta `../src/*.cpp` por glob
  no momento da configuração (`verovio/cmake/CMakeLists.txt` L185).
- **Binário**: `verovio/tools/verovio`, sempre com
  `--resource-path verovio/data`.
- **Ferramenta de comparação**: `cd compare && cargo build --release` →
  `compare/target/release/compare` (ver `compare/README.md`).
- **Saídas temporárias** de testes vão em `compare/out/` (ignorado pelo git a
  partir do passo A05). Nunca em `verovio/`.
- **Estilo**: imite `include/vrv/svgdevicecontext.h` e
  `src/svgdevicecontext.cpp` (namespace `vrv`, cabeçalho de arquivo, separadores
  `//----`). Existe `verovio/.clang-format`; use `clang-format -i` nos arquivos
  novos se estiver disponível.
- **Não altere** `SvgDeviceContext`, `BBoxDeviceContext` nem as classes `View`.
  O exportador é um novo `DeviceContext` alimentado pelo mesmo `View` que já
  desenha o SVG — é isso que garante paridade visual.
- **Unidades**: tudo que chega ao `DeviceContext` está em "unidades de
  definição" (`DEFINITION_FACTOR = 10`), com eixo y já apontando para baixo (o
  `View` inverte: `src/view.cpp` L85-L92).
- **Corpus de teste**: `corpus/mei/*.mei` e `corpus/musicxml/*.mxl`.

## Arquitetura alvo

```
Toolkit::RenderToDotLottieFile()
  └─ para cada página p: Toolkit::RenderToDeviceContext(p, &lottieDc)   (já existe, genérico)
       └─ View::DrawCurrentPage(dc)                                     (já existe)
            └─ StartPage / StartGraphic / Draw* / EndGraphic / EndPage
                 └─ LottieDeviceContext monta uma árvore interna (IR)
  └─ LottieWriter serializa a IR → JSON Lottie (uma camada por página)
  └─ ZipFileWriter empacota → .lottie (manifest.json, a/score.json, depois s/*.json)
```

Arquivos novos previstos (todos dentro de `verovio/`):

| Arquivo | Criado em | Papel |
| --- | --- | --- |
| `include/vrv/lottiedevicecontext.h`, `src/lottiedevicecontext.cpp` | A01 | Subclasse de `DeviceContext` |
| `include/vrv/lottiegeometry.h` | A02 | Structs da IR (vetores, béziers, formas, nós, páginas) |
| `include/vrv/lottiewriter.h`, `src/lottiewriter.cpp` | A03 | IR → JSON Lottie |
| `include/vrv/svgpathparser.h`, `src/svgpathparser.cpp` | A08 | Path SVG dos glifos → béziers |
| `ZipFileWriter` em `include/vrv/filereader.h` / `src/filereader.cpp` | A11 | Escrita do zip |
| `include/vrv/lottiestatemachine.h` | C01 | IR da state machine dotLottie v2 |
| `include/vrv/lottiehighlight.h`, `src/lottiehighlight.cpp` | C02 | Agrupamento M2 por instante do timemap + state machine de destaque |

## Mapa do código (verificado)

| Conceito | Onde |
| --- | --- |
| API abstrata de desenho | `include/vrv/devicecontext.h` L99-L337; membros protegidos `m_penStack`, `m_brushStack`, `m_fontStack`, `m_pushBack` em L352-L364 |
| Construtor com `ClassId` | `include/vrv/devicecontext.h` L81-L97 |
| `ClassId` dos device contexts | `include/vrv/vrvdef.h` L287-L289 |
| `DEFINITION_FACTOR` | `include/vrv/vrvdef.h` L457 |
| `Pen` / `Brush` / `FontInfo`, `COLOR_NONE` | `include/vrv/devicecontextbase.h` L57 / L112 / L137, L23 |
| `SetPen`, `SetBrush`, `SetFont`, `SetViewBoxFactor` | `src/devicecontext.cpp` L139-L201 |
| Implementação de referência (SVG) | `src/svgdevicecontext.cpp`: `Commit` L150, `StartGraphic` L251, `ResumeGraphic` L427, `StartPage` L484, `AddChild` L607, primitivas L686-L1017, texto L1019-L1167, `DrawMusicText` L1174, `AppendIdAndClass` L1251, `GetColor` L1295 |
| Implementação enxuta (exemplo de subclasse) | `include/vrv/bboxdevicecontext.h` |
| Desenho de uma página | `src/view_page.cpp` L66-L117 (`View::DrawCurrentPage`) |
| Offsets só aplicados se `ApplyOffset()` | `src/view.cpp` L188-L196 |
| Desenho de nota (grupo com `xml:id`) | `src/view_element.cpp` L1515-L1627 |
| Glifos SMuFL | `src/view_graph.cpp` L279-L357; `Resources::GetGlyph` em `include/vrv/resources.h` L94; `Glyph::GetXML` em `src/glyph.cpp` L150-L161; dados em `data/Leipzig/*.xml` (fonte padrão: `src/options.cpp` L1306-L1308) |
| Toolkit | `include/vrv/toolkit.h` (`GetPageCount` L196, `RenderToSVG` L365, `RenderToSVGFile` L376, `RenderToTimemap` L422, `RenderToDeviceContext` L669); `src/toolkit.cpp` (`RenderToDeviceContext` L1674-L1730, `RenderToSVG` L1740-L1797, `RenderToSVGFile` L1799-L1816, `RenderToTimemap` L1892-L1920) |
| Timemap (ids das notas por instante) | `include/vrv/timemap.h` L35-L43; `src/timemap.cpp` L40-L100 |
| Formatos de saída | `include/vrv/toolkitdef.h` L13-L35 (`FileFormat`); `src/options.cpp` L1987-L2027 (`SetOutputTo`); `tools/main.cpp` L284-L292 (validação), L312-L315 (força `breaks: none` — **não** incluir os formatos novos), L350-L371 (laço de páginas do SVG) |
| Registro de opções (padrão) | `include/vrv/options.h` L678-L685; `src/options.cpp` L1177-L1189 |
| ZIP embutido | `include/zip/zip_file.hpp` (`zip_file` L9486, `save` L9541/L9569, `writestr` L9849); leitor existente em `src/filereader.cpp` L20-L125 |
| Bindings | `tools/c_wrapper.cpp` L335-L359; `emscripten/exports.txt` L37-L38 |

Fatos sobre os dados que orientam o plano:

- Os glifos das fontes musicais são `<g id="E0A4"><path transform="scale(1,-1)" d="..."/></g>`.
  Em Leipzig e Bravura só aparecem os comandos `M c h l s v z`, um `<path>` por
  glifo e sempre `transform="scale(1,-1)"`.
- `data/text/Times*.xml` só tem métricas (zero `<path>`): **não há contornos de
  texto comum** no Verovio. Texto com fonte SMuFL (dinâmicas etc.) usa glifos
  que têm contorno.
- Contagem de elementos no SVG do corpus inteiro: `path` 8025, `use` (glifos)
  5444, `polygon` 1526, `tspan` 398, `ellipse` 339, `rect` 250, `text` 160,
  `polyline` 52, `image` 0. Primitivas praticamente sem chamadas no `View`:
  `DrawEllipticArc` e `DrawSpline` (0), `DrawRotatedText` (0), `DrawSvgShape`
  (1), `DrawGraphicUri` (1), `RotateGraphic` (2).

## Referência rápida: pacote dotLottie v2 (verificado)

Verificado em fixtures do `dotlottie-rs` (checkout local em
`~/.cargo/git/checkouts/dotlottie-rs-*/<rev>/dotlottie-rs/assets/animations/dotlottie/v2/`,
arquivos `sm-tween.lottie` e `elapsed_time.lottie`):

```
manifest.json   {"version":"2","generator":"...","animations":[{"id":"score"}],
                 "stateMachines":[{"id":"..."}],"initial":{"animation":"score"}}
a/<id>.json     animação Lottie
s/<id>.json     state machine
f/<arquivo>.ttf fontes, referenciadas pela animação em fonts.list[].fPath (com "origin": 3)
```

State machine (formato aceito pelo `dotlottie-rs`):

```json
{
  "initial": "GLOBAL",
  "states": [
    {"name": "n1", "type": "PlaybackState", "animation": "score",
     "segment": "<nome do marker>", "autoplay": true, "loop": false, "transitions": []},
    {"name": "GLOBAL", "type": "GlobalState", "animation": "",
     "transitions": [{"type": "Tweened", "toState": "n1", "duration": 5,
                      "easing": [0,0,0.58,1],
                      "guards": [{"type": "Event", "inputName": "n1"}]}]}
  ],
  "inputs": [{"type": "Event", "name": "n1"}],
  "interactions": []
}
```

- Tipos de input: `Numeric`, `String`, `Boolean`, `Event`
  (`src/state_machine/inputs.rs` L7-L12).
- Guards: `Numeric`, `String`, `Boolean`, `Event`
  (`src/state_machine/transitions/guard.rs` L52-L71).
- Transições do `GlobalState` são avaliadas qualquer que seja o estado atual
  (`src/state_machine/mod.rs` ~L1415-L1430) — é o que viabiliza a "topologia em
  estrela" exigida.
- Ações disponíveis: `OpenUrl`, `Increment`, `Decrement`, `Toggle`, `SetBoolean`,
  `SetString`, `SetNumeric`, `SetRandom`, `Multiply`, `Floor`, `Clamp`, `Fire`,
  `Reset`, `SetTheme`, `SetFrame`, `SetProgress`, `FireCustomEvent`
  (`src/state_machine/actions/mod.rs` L68+).
- O host dispara eventos com `StateMachineEngine::fire(nome, run_pipeline)`
  (`src/state_machine/mod.rs` L334).
- `PlaybackState.segment` referencia um marker da animação.

## Decisões pendentes

Estas decisões **bloqueiam** os passos indicados. Quem executar deve perguntar ao
usuário ao chegar neles.

| Id | Pergunta | Bloqueia | Recomendação inicial |
| --- | --- | --- | --- |
| D-CLI | ~~Nomes dos formatos de saída na CLI~~ — **decidido em A04**: `lottie` (JSON cru de uma página, para depuração) e `dotlottie` (pacote final da música) | ~~A04~~, A12 | `lottie` (JSON cru de uma página, para depuração) e `dotlottie` (pacote final da música) |
| D-TEXTO | ~~Como renderizar texto comum (títulos, andamento, dedilhados, letra)~~ — **decidido em B03**: T1 — fonte TTF (Liberation Serif, Regular+Italic+Bold) embutida no pacote + camada de texto nativa do dotLottie (`ty:5`+`fonts.list`), em vez de converter em contornos. Ver `docs/plano/decisoes/B03-texto.md`. | D01 | T1 (fonte embutida) |
| D-DESTAQUE | ~~Mecanismo para destacar notas simultâneas (acordes, duas mãos) com fade controlado pelo Lottie~~ — **decidido em B02**: dois mecanismos por modo de uso, mutuamente exclusivos — M2 (agrupamento por instante do timemap, fade autorado) no modo automático; M3 (slot de cor por nota, sem limite, fade não autorado) no modo interativo. Ver `docs/plano/decisoes/B02-mecanismo-destaque.md`. | ~~Fase C~~ ~~C01~~ ~~C02~~ ~~C03~~ ~~C04~~ (C01 entrega o writer genérico + valida o Risco 1; C02 entrega M2 funcional — cores keyframadas + agrupamento por instante do timemap — para uma página por vez; C03 entrega M3 — slot de cor por nota — e o protocolo de handoff M2↔M3, ver `docs/plano/C03-slots-interativos.md`; C04 reintegra M2+M3 na partitura inteira dentro do pacote `dotlottie` final, ver `docs/plano/C04-paginas-virada.md`) | M2 (auto) + M3 (interativo) |
| D-LAYOUT-PAGINAS | ~~Disposição das páginas na composição e animação de virada~~ — **decidido em B02**: trilha horizontal + engine separado do destaque, virada em dois eventos discretos (entrar no último compasso da página atual → "espreitar"; entrar no primeiro compasso da próxima → "cobrir"). Ver `docs/plano/decisoes/B02-mecanismo-destaque.md`. **Implementado em C04** (`docs/plano/C04-paginas-virada.md`): camada-câmera nula + `sm_page`, `Toolkit::RenderToDotLottieFile`/CLI `dotlottie` agora produz o pacote completo (M2+M3+página) da partitura inteira. | ~~C (virada de página)~~ ~~C04~~ | trilha horizontal, dois eventos por fronteira |

Decisões **já tomadas** (não reabrir): ver "Decisões arquiteturais já tomadas" no
`CLAUDE.md`.

## Riscos conhecidos

1. **Concorrência de destaques** — uma animação Lottie tem um único playhead.
   Acordes e as duas mãos do piano exigem várias notas destacadas ao mesmo tempo,
   com fades que se sobrepõem. Isso pode conflitar com "um segmento por nota".
   **Confirmado empiricamente pelo B01** (E3/E6 e o teste extra de virada de
   página em `docs/plano/spikes/B01-resultado.md`): `PlaybackState`+`segment`
   só permite uma nota destacada por vez (um único `current_state`), e uma
   virada de página no mesmo engine cancela um destaque em andamento; slots
   de cor (E6) acendem várias notas ao mesmo tempo mas sem fade automático
   comprovado. **Decidido em B02** (`docs/plano/decisoes/B02-mecanismo-destaque.md`):
   dois mecanismos por modo de uso, mutuamente exclusivos — agrupamento por
   instante do timemap (M2, fade autorado) no modo automático; slot de cor
   por nota sem limite (M3, fade não autorado) no modo interativo; engine
   totalmente separado (dois eventos por fronteira de página) pra virada.
   Risco residual aceito sem spike dedicado: não está confirmado se os
   runtimes de player do zywny suportam rodar as 2 instâncias simultâneas
   (engine principal + engine de página) e compositar entre elas — atenção
   no início de C00/C01. **Achado concreto de C04** (leitura de código, não
   suposição): o `dotlottie-rs` de hoje não expõe, na API pública de
   `Player`, nenhum jeito de ler a posição/transform de uma camada por fora
   (o único mecanismo relacionado, `get_layer_obb`/`hit_test`, vive atrás de
   `renderer: pub(crate)` e só é usado internamente pelas guardas de
   interação da state machine) — o risco continua aberto e não resolvido,
   só mais concreto; ver `docs/plano/C04-paginas-virada.md`, seção "Ler
   antes".
2. **Texto sem contornos** — **decidido em B03** (ver D-TEXTO acima): fonte
   TTF (Liberation Serif) embutida no pacote, não contornos assados. Custo
   de tamanho medido no spike: ~208-220 KB comprimidos por estilo de fonte;
   com Regular+Italic+Bold decidido, ~600-650 KB fixos por peça, acima da
   maioria dos pacotes do corpus hoje (77-430 KB, A13) — atenção especial em
   D01 se isso virar problema real.
3. **Tamanho do arquivo** — a fase A "assa" as coordenadas de cada glifo em cada
   uso (não há `<use>` em Lottie). Otimização fica para a fase D, se o relatório
   de paridade mostrar arquivos grandes demais.
4. **`zip_file.hpp` é header-only com a implementação do miniz** — incluí-lo em
   mais de uma unidade de tradução pode gerar símbolos duplicados no link. Por
   isso o escritor de zip fica em `src/filereader.cpp`, que já o inclui (A11).
5. **Comparação justa** — o `resvg` usa fontes do sistema para `<text>` e não
   carrega o `@font-face` woff2 embutido no SVG. A05 faz o `compare` carregar as
   fontes do Verovio (`verovio/fonts/Leipzig/Leipzig.ttf` etc.).

## Passos

| Passo | Título | Depende de | Decisão | Status |
| --- | --- | --- | --- | --- |
| [A01](A01-esqueleto-lottiedevicecontext.md) | Esqueleto do `LottieDeviceContext` | — | — | concluído |
| [A02](A02-arvore-de-graficos.md) | Árvore interna (IR) de grupos e páginas | A01 | — | concluído |
| [A03](A03-serializador-lottie-json.md) | Serializador `LottieWriter` | A02 | — | concluído |
| [A04](A04-toolkit-e-cli-lottie-json.md) | `Toolkit::RenderToLottie` + CLI `lottie` | A03 | D-CLI | concluído |
| [A05](A05-script-de-comparacao.md) | Script de comparação por página + fontes no `compare` | A04 | — | concluído |
| [A06](A06-primitivas-retas-e-cores.md) | Linhas, polígonos, retângulos, elipses e cores | A05 | — | concluído |
| [A07](A07-curvas-bezier.md) | Curvas Bézier (ligaduras, beams curvos) | A06 | — | concluído |
| [A08](A08-parser-de-path-dos-glifos.md) | Parser de path SVG dos glifos | A02 | — | concluído |
| [A09](A09-draw-music-text.md) | `DrawMusicText` (glifos SMuFL) | A06, A08 | — | concluído |
| [A10](A10-texto-smufl.md) | Texto: fontes SMuFL via glifos; texto comum contabilizado | A09 | — | concluído |
| [A11](A11-zip-writer.md) | `ZipFileWriter` | A01 | — | concluído |
| [A12](A12-pacote-dotlottie-multipagina.md) | Pacote `.lottie` com todas as páginas + CLI `dotlottie` | A10, A11 | D-CLI | concluído |
| [A13](A13-varredura-do-corpus.md) | Varredura do corpus e relatório de paridade | A12 | — | concluído |
| [B01](B01-spike-state-machine.md) | Spike: state machine com eventos por `xml:id` | A05 | — | concluído |
| [B02](B02-memorando-mecanismo-de-destaque.md) | Memorando: mecanismo de destaque e virada | B01 | produziu D-DESTAQUE e D-LAYOUT-PAGINAS | concluído |
| [B03](B03-memorando-texto.md) | Memorando: texto comum | A13 | produziu D-TEXTO | concluído |
| [C00](C00-fase-c-esboco.md) | Fase C (animações) — esboço a detalhar, reescrever em C01…Cn | B02, A13 | — (D-DESTAQUE e D-LAYOUT-PAGINAS já decididos) | concluído (C01-C06 todos extraídos e concluídos) |
| [C01](C01-writer-state-machine.md) | Writer de state machine (`s/<id>.json`, `stateMachines` no `manifest.json`) | A12, B02 | — | concluído |
| [C02](C02-notas-animadas.md) | Propriedades animadas das notas (M2: cores keyframadas + agrupamento por instante do timemap), uma página por vez | A12, C01 | — | concluído |
| [C03](C03-slots-interativos.md) | Slots interativos (M3: `sid` por nota), protocolo de handoff M2↔M3 (`fire idle`/`clear_slots`) e validação de nomes/unicidade | A12, C02 | — | concluído |
| [C04](C04-paginas-virada.md) | Páginas e virada estilo Synthesia (trilha horizontal + câmera + `sm_page`); reintegra M2/M3 na partitura inteira no pacote `dotlottie` final | A12, A13, C01, C02, C03 | — | concluído |
| [C05](C05-host-simulado-timemap.md) | Host simulado com timemap real (`verovio -t timemap` → roteiro de `compare sm-render`), `compare/scripts/sm-playback.sh` | A12, A13, B01, C01-C04 | — | concluído |
| [C06](C06-opcoes-cor-duracao.md) | Opções de CLI para cor/duração do destaque e da virada de página | C02, C04 | — | concluído |
| [D00](D00-fase-d-esboco.md) | Fase D (paridade completa) — esboço a detalhar | A13, B03 | — (D-TEXTO já decidido) | concluído (reescrito em D01-D06) |
| [D01](D01-texto-comum.md) | Texto comum: Liberation Serif embutida (T1), camada de texto nativa `ty:5` | A10, B03 | — (T1 já decidido) | concluído (ver "Notas de execução" — achado importante: % de divergência do corpus não caiu como esperado, causa é a ferramenta de comparação, não o exportador) |
| [D01-2](D01-2-controle-de-fonte-na-comparacao.md) | Tarefa paralela: `compare svg-to-png` fixar a fonte (Liberation Serif) via `fontdb::set_serif_family`, independente do sistema — hoje `--font` sozinho não muda nada | D01, A05 | — (fonte já decidida em B03; só a ferramenta de comparação) | concluído (ver "Notas de execução" — corpus caiu de 0,1115%–0,8206%/média 0,3601% para 0,1002%–0,6646%/média 0,3154%; bug do `<title>` aninhado ainda em aberto) |
| [D01-3](D01-3-titulo-aninhado-resvg.md) | Tarefa paralela: `compare svg-to-png` remover `<title>` aninhado antes do `usvg` processar — o `resvg` mede erroneamente esse texto não-renderizável ao resolver `text-anchor` | D01, D01-2 | — (só a ferramenta de comparação) | concluído (ver "Notas de execução" — corpus caiu de 0,1002%–0,6646%/média 0,3154% para 0,1002%–0,5997%/média 0,2977%; melhoria concentrada na p.1 das peças de `corpus/mei`, que são as únicas com `@label` no cabeçalho) |
| [D01-4](D01-4-italico-sintetico-thorvg.md) | Tarefa paralela: ThorVG local (`thorvg/` + `dotlottie-rs/` vendorizados) sem itálico sintético por cima de fonte já itálica — o loader de Lottie inclinava duas vezes todo texto comum em itálico | D01, D01-3 | manter cópia local do ThorVG com a correção (decidido pelo usuário) | concluído (corpus: 0,0196%–0,5219%/média 0,2293% → 0,0135%–0,4153%/média 0,1318%; os players oficiais continuam com o bug — ver "Pendências" no doc) |
| [D02](D02-drawsvgshape.md) | `DrawSvgShape` (SVG embutido no MEI) — achado via Mesa de Prova: cobre também o rodapé "MEI engraved with Verovio" que `Doc::GenerateFooter()` insere em toda página, hoje ausente em 100% do corpus (stub vazio) | A08 | — | concluído (corpus: 0,1002%–0,5997%/média 0,2977% → 0,0196%–0,5212%/média 0,2290%; ver "Notas de execução" no doc) |
| [D03](D03-drawgraphicuri.md) | `DrawGraphicUri` (imagens raster) | — | vale a pena implementar? qual resolução de `target`? | decidido: não implementar agora (stub permanece), sem caso de uso real no corpus — revisitar quando o zywny precisar |
| [D04](D04-rotategraphic.md) | `RotateGraphic` — confirmar sinal e pivô da rotação | — | — | concluído (hipótese confirmada — nenhuma mudança de código; ver "Notas de execução" no doc) |
| [D05](D05-casos-de-borda-estilo.md) | Casos de borda de estilo (opacidade, tracejado, visibilidade, cue) | A06, A09, A10 | — | concluído (3/4 bateram sem mudança; opacidade achou um bug real, mas na ferramenta `compare` — ver "Notas de execução" no doc) |
| [D06](D06-tamanho-do-arquivo.md) | Tamanho do arquivo — reuso de glifos repetidos | D01, A13 | remedir e perguntar antes de decidir a técnica | bloqueado (remedir após D01) |
| [E00](E00-bindings-opcional.md) | Bindings JS/Python (opcional) | A12 | — | opcional |
| [E01](E01-bindings-dart-ffi.md) | Bindings Dart FFI (versão "library" — wrapper C + `libverovio.so` + pacote Dart), padrão `verovio_flutter` | A12, A04 | — | concluído |

Ordem sugerida: A01 → A02 → A03 → A04 → A05 → A06 → A07 → A08 → A09 → A10 →
A11 → A12 → A13. B01 pode começar em paralelo assim que A05 terminar (não
depende do exportador).
