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
| D-TEXTO | Como renderizar texto comum (títulos, andamento, dedilhados, letra) | D01 | Decidir após o memorando B03 |
| D-DESTAQUE | Mecanismo para destacar notas simultâneas (acordes, duas mãos) com fade controlado pelo Lottie | Fase C | Decidir após o memorando B02 |
| D-LAYOUT-PAGINAS | Disposição das páginas na composição e animação de virada | C (virada de página) | Decidir junto com D-DESTAQUE |

Decisões **já tomadas** (não reabrir): ver "Decisões arquiteturais já tomadas" no
`CLAUDE.md`.

## Riscos conhecidos

1. **Concorrência de destaques** — uma animação Lottie tem um único playhead.
   Acordes e as duas mãos do piano exigem várias notas destacadas ao mesmo tempo,
   com fades que se sobrepõem. Isso pode conflitar com "um segmento por nota".
   Os passos B01/B02 existem para resolver isso antes de qualquer código de
   animação.
2. **Texto sem contornos** — ver D-TEXTO.
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
| [B01](B01-spike-state-machine.md) | Spike: state machine com eventos por `xml:id` | A05 | — | pendente |
| [B02](B02-memorando-mecanismo-de-destaque.md) | Memorando: mecanismo de destaque e virada | B01 | produz D-DESTAQUE | pendente |
| [B03](B03-memorando-texto.md) | Memorando: texto comum | A13 | produz D-TEXTO | pendente |
| [C00](C00-fase-c-esboco.md) | Fase C (animações) — esboço a detalhar | B02, A13 | D-DESTAQUE, D-LAYOUT-PAGINAS | bloqueado |
| [D00](D00-fase-d-esboco.md) | Fase D (paridade completa) — esboço a detalhar | A13, B03 | D-TEXTO | bloqueado |
| [E00](E00-bindings-opcional.md) | Bindings JS/Python (opcional) | A12 | — | opcional |

Ordem sugerida: A01 → A02 → A03 → A04 → A05 → A06 → A07 → A08 → A09 → A10 →
A11 → A12 → A13. B01 pode começar em paralelo assim que A05 terminar (não
depende do exportador).
