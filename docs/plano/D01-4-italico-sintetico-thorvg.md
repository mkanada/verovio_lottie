# D01-4 — ThorVG local: itálico sintético aplicado por cima de fonte já itálica

**Depende de:** D01 (texto comum embutido), D01-2 e D01-3 (removeram as duas
causas de divergência do lado do `resvg`) · **Decisão necessária:** tomada
pelo usuário — manter uma **cópia local do ThorVG com a correção
definitiva**, em vez de contornar o problema no exportador.

## Problema

Achado a olho nu em `docs/matriz-layout/` (investigação registrada em
`thorvg-cli/README.md`): texto comum em itálico ("cresc.", "con forza",
"sempre legatissimo") saía no `.lottie` renderizado com as **letras
grudadas**, mais inclinadas e deslocadas que no SVG de referência. Texto reto
e negrito saíam pixel-idênticos.

Não era o exportador. O `LottieWriter` escreve a fonte certa
(`"f":"LiberationSerif-Italic"`, com o `.ttf` itálico em `f/`) na mesma
posição que o SVG declara. A causa estava no loader de Lottie do ThorVG
(`src/loaders/lottie/tvgLottieBuilder.cpp`, igual no fork usado pelo
dotlottie-rs e no `main` de `thorvg/thorvg` em 2026-09-15, `b07c83fe`):

```cpp
if (text->font && text->font->style && strstr(text->font->style, "Italic")) paint->italic();
```

Sempre que `fonts.list[].fStyle` contém `"Italic"`, o ThorVG pede um **itálico
sintético** (`Text::italic()`, cisalhamento padrão 0,18 ≈ 10°), sem verificar
se a face carregada já é itálica. Serve para quando só existe a face regular
da família; com a `LiberationSerif-Italic.ttf` embutida (T1, B03), a
inclinação era aplicada **duas vezes** (16,3° da própria fonte + ~10°). Além
disso, `SfntLoader::transform` soma um deslocamento horizontal de
`cisalhamento × largura do primeiro glifo`. O topo de cada letra invadia a
seguinte.

A própria fonte declara que é itálica, nas três tabelas usuais:

| Face | `post.italicAngle` | `OS/2.fsSelection` bit 0 (ITALIC) | `head.macStyle` bit 1 |
| --- | --- | --- | --- |
| LiberationSerif-Regular | 0 | 0 | 0 |
| LiberationSerif-Italic | −16,33 | 1 | 1 |
| LiberationSerif-Bold | 0 | 0 | 0 |

**Prova antes da correção** (dotlottie-rs original; mesmo pacote, só o
`fStyle` da entrada itálica editado no `a/score.json`; tolerância 32):

| Página | `"Italic"` (como o exportador escreve) | `"italic"` ou `"Regular"` (sem o gatilho do `strstr`) |
| --- | --- | --- |
| Chopin Étude Op.10 No.9, p.1 | 0,4520% | 0,0465% |
| Clair de Lune, p.1 | 0,5219% | 0,3963% |

Por que não foi pego antes: a inspeção de recortes de D01 (item 4 das
"Notas de execução") confirmou a face certa, mas não comparou a inclinação
lado a lado. Já `docs/matriz-layout/README.md` atribuiu a divergência a ruído
de antialiasing "em bordas finas de glifos de texto em itálico". As duas
leituras foram corrigidas neste passo.

## Opções consideradas

1. **Contorno no exportador** — escrever o `fStyle` itálico sem a substring
   `"Italic"` (`"italic"` minúsculo ou `"Regular"`). Corrigiria inclusive os
   players oficiais, mas `"italic"` depende de o `strstr` do ThorVG
   diferenciar maiúsculas, e `"Regular"` grava metadado falso. **Não
   adotado.**
2. **Correção no ThorVG, mantida numa cópia local** — **adotado** (decisão do
   usuário).
3. Compensar com skew inverso na transformação da camada — descartado: só
   funcionaria no ThorVG, e o deslocamento depende da largura do primeiro
   glifo de cada texto.

## Decisões de escopo tomadas aqui

- **Vendorizar o ThorVG em `thorvg/`** (raiz), no mesmo padrão do `verovio/`:
  cópia solta, sem submódulo. Origem: commit `019d18c7` do fork
  `theashraf/thorvg`, exatamente o submódulo do dotlottie-rs `eb44c991` que o
  `compare/` já usava. Copiados só `src/`, `inc/`, `tools/` e arquivos de
  build/licença (~11 MB); `test/` etc. omitidos.
- **Vendorizar também o dotlottie-rs em `dotlottie-rs/`**: o `build.rs` dele
  compila o ThorVG de um caminho fixo (`deps/thorvg`), sem variável de
  ambiente, e o Cargo não permite trocar o submódulo de uma dependência git.
  Nenhuma mudança no código Rust; `deps/thorvg` é um symlink para
  `../../thorvg`. `compare/Cargo.toml` passou a usar
  `path = "../dotlottie-rs"`.
- **Correção restrita ao loader de Lottie**; API pública (`inc/thorvg.h`)
  intacta — `Text::italic()` chamado diretamente continua cisalhando.
  Fonte **não** itálica com `fStyle` "Italic" continua recebendo o itálico
  sintético (o caso para o qual a heurística existe).
- **Detecção por `head.macStyle` e `OS/2.fsSelection`** (bits de
  estilo padrão do formato; bit 9 OBLIQUE incluído), não por
  `post.italicAngle`.
- Toda linha alterada no ThorVG leva o marcador `verovio_lottie`; a lista
  de modificações e o procedimento de atualização ficam em
  `thorvg/VEROVIO_LOTTIE.md`.
- `thorvg-cli/` passou a compilar o mesmo `thorvg/` (build meson fora da
  árvore em `thorvg-cli/build/`); `fetch-thorvg.sh` removido.
- **Nada muda no exportador nem nos `.lottie`** — `corpus/lottie/` não
  precisa ser regenerado.

## Arquivos

- Novo: `thorvg/` (vendorizado) e `thorvg/VEROVIO_LOTTIE.md`.
- Novo: `dotlottie-rs/` (vendorizado), `dotlottie-rs/deps/thorvg` (symlink) e
  `dotlottie-rs/VEROVIO_LOTTIE.md`.
- Modificados no ThorVG local:
  - `src/loaders/sfnt/tvgSfntReader.h` — `metrics.italic`.
  - `src/loaders/sfnt/tvgSfntReader.cpp` — `SfntReader::header()` lê
    `head.macStyle`/`OS/2.fsSelection`.
  - `src/renderer/tvgLoader.h` — `FontLoader::italic()` (padrão `false`).
  - `src/loaders/sfnt/tvgSfntLoader.h` — `SfntLoader::italic()`.
  - `src/loaders/lottie/tvgLottieBuilder.cpp` — itálico sintético só se
    `!loader->italic()`.
- Modificados: `compare/Cargo.toml` (+ `Cargo.lock`), `thorvg-cli/build.sh`,
  `thorvg-cli/.gitignore`.
- Documentação corrigida: `thorvg-cli/README.md`, `docs/matriz-layout/`
  (reexecutada), `docs/plano/D01-texto-comum.md`, `compare/README.md`,
  `README.md`, `CLAUDE.md`, `docs/descricao-do-projeto.md`.
- Regenerados: `docs/mesa-de-prova/` (102 PNGs), `docs/exemplos/` (9 PNGs) e
  o artefato "Mesa de Prova" na conta claude.ai. Apagado: `thorvg-cli/thorvg/`
  (clone antigo).

## Fora de escopo

- Enviar a correção upstream (ThorVG ou dotlottie-rs) — é uma ação externa,
  fica para decisão do usuário.
- O loader de **SVG** do ThorVG não aplicar `font-style: italic` (achado de
  `thorvg-cli/README.md`) — não afeta o `.lottie` nem o `compare`.
- Investigar a divergência residual de Nocturne e Clair de Lune (ver
  abaixo).

## Critérios de aceite

- Cópia fiel antes do patch (`diff -r` contra o checkout do cargo).
- `compare` e `thorvg-cli` compilam contra o ThorVG local.
- Chopin p.1 com o pacote **sem nenhuma edição** (`"fStyle":"Italic"`) cai
  para o mesmo valor do experimento sem o gatilho (0,0465%).
- Regular/Bold e o itálico sintético para fontes regulares sem regressão.
- `compare-corpus.sh 32` e `compare-layout-matrix.sh` reexecutados e
  documentados.

## Notas de execução

Implementado como descrito acima; nenhum ajuste de desenho foi necessário.

- **Cópia fiel:** `diff -r` de `src/`, `inc/`, `tools/` (ThorVG) e de `src/`,
  `cpp/`, `examples/`, `benches/` (dotlottie-rs) contra o checkout do cargo:
  sem diferenças, antes de aplicar a correção.
- **Builds:** `cargo build --release` do `compare` (51 s, recompilando o
  ThorVG local) e `thorvg-cli/build.sh` terminam sem erro.
- **Correção, comparando PNGs do binário novo com os do antigo, tolerância 0:**
  - Chopin p.1, pacote original (`"Italic"`): SVG vs. Lottie
    **0,4520% → 0,0465%**, e o PNG novo é **idêntico pixel a pixel** ao PNG
    antigo do pacote com `"italic"` minúsculo.
  - Clair de Lune p.1: **0,5219% → 0,3963%**, também idêntico ao
    experimento sem o gatilho.
- **Sem regressão:**
  - Pacote com `"fStyle":"Regular"` na entrada itálica: PNG novo = PNG
    antigo (0 pixels). Nada fora do itálico sintético mudou.
  - Pacote com `"fStyle":"Italic"` mas com os bytes da
    `LiberationSerif-Regular.ttf` no lugar da itálica: PNG novo = PNG antigo
    (0 pixels), ou seja, o itálico sintético **continua** sendo aplicado a
    uma face regular.
  - `thorvg-render` (meson) e `compare` (dotlottie-rs) no mesmo pacote: 0
    pixels diferentes a tolerância 32.
- **Corpus** (`compare-corpus.sh 32`, 34 páginas). O "antes" foi medido de
  novo nesta data com o binário ainda sem a correção; a média difere
  ligeiramente dos 0,2290% de D02 por causa de mudanças posteriores:

  | | mín | máx | média |
  | --- | --- | --- | --- |
  | Antes (dotlottie-rs original) | 0,0196% | 0,5219% | 0,2293% |
  | Depois (ThorVG local) | **0,0135%** | **0,4153%** | **0,1318%** |

  As 34 páginas melhoraram, nenhuma piorou. Por peça (média das páginas):

  | Peça | Páginas | Antes | Depois |
  | --- | --- | --- | --- |
  | Chopin Étude Op.10 No.9 | 4 | 0,3509% | 0,0480% |
  | Chopin Nocturne Op.9 No.1 | 7 | 0,4091% | 0,2726% |
  | Chopin Mazurka Op.6 No.1 | 3 | 0,1561% | 0,0498% |
  | Clair de Lune | 5 | 0,3952% | 0,3178% |
  | Satie Gymnopédie No.1 | 2 | 0,0677% | 0,0322% |
  | Scarlatti Sonata em Dó | 3 | 0,0707% | 0,0368% |
  | Prelúdio BWV 846 | 2 | 0,0711% | 0,0426% |
  | Grieg Butterfly | 3 | 0,0672% | 0,0395% |
  | Maple Leaf Rag | 3 | 0,0824% | 0,0551% |
  | Grieg Little bird | 2 | 0,0736% | 0,0490% |

- **Matriz de layout** (`compare-layout-matrix.sh`, Chopin p.1, 16
  combinações): mín 0,2692% / máx 0,4550% / média 0,3836% → **mín 0,0318% /
  máx 0,0751% / média 0,0480%**. No git, só os `*-lottie.png`, `*-diff.png`
  e o `resultado.csv` mudaram; os `*-svg.png` ficaram idênticos.
- **O que sobra:** no Chopin p.1 (0,0465%), os clusters restantes são ruído
  de antialiasing em traços finos (rodapé SVG "MEI engraved with Verovio",
  chave, linhas de pauta), sem nada de itálico. **Nocturne (0,27%) e Clair de
  Lune (0,32%, máximo do corpus na p.4 com 0,4153%)** continuam bem acima das
  outras peças; causa não investigada — investigação nova, se importar.

### Imagens regeneradas e limpeza

- **`docs/mesa-de-prova/`** (102 PNGs): saídas da rodada pós-correção do
  `compare-corpus.sh 32`, regravadas sem perda no mesmo formato da versão
  anterior (RGB para `svg`/`lottie`, RGBA para `diff`; pixels conferidos
  contra a saída do `compare`). O diretório foi de 33,7 para 40,9 MB. O
  artefato "Mesa de Prova" (claude.ai) foi republicado com essas imagens e
  as porcentagens novas por página.
- **`docs/exemplos/destaque-notas/`**: `compare/scripts/sm-playback.sh
  corpus/mei/Scarlatti_Sonata_in_C-major.mei 25` (seed 42), snapshots
  `playback-t{0,2750,6000,6667}.png`.
- **`docs/exemplos/virada-pagina/`**: o roteiro original não estava
  registrado. Foi reconstruído comparando as imagens antigas com os frames
  dos segmentos `peek1`/`cover1` (o antigo t150 bate com 1,5 frame de
  `peek1`, o t650 com 1,5 frame de `cover1`):

  ```sh
  compare sm-render compare/out/c05/Scarlatti_Sonata_in_C-major/Scarlatti_Sonata_in_C-major.lottie <saída> \
    --sm sm_page --width 2100 --height 2970 \
    --script "100:fire d1e3859;600:fire d1e4029" --snap "0,150,590,650,1300" --prefix virada
  ```

  t0 e t1300 batem 0 px com os frames de repouso das páginas 1 e 2.
- Contra as imagens antigas, cada nova difere em 2.500–3.600 px (tolerância
  32), só nos textos em itálico (números de compasso). Exceção: o
  `04-fade-completo-t6667ms.png` antigo também não tinha o título "Suite I"
  e diferia na faixa central até o rodapé (9.202 px no total). O novo é
  idêntico, pixel a pixel, ao frame de repouso da página 1, como um fade
  completo deve ser.
- `thorvg-cli/thorvg/` (clone antigo do ThorVG, ~407 MB) apagado.

### Pendências (não decididas)

- **Players oficiais continuam com o bug.** O zywny só vê o itálico certo se
  usar um player compilado contra `thorvg/` (ou se a correção for aceita
  upstream e chegar ao dotlottie-rs). A opção 1 (contorno no exportador)
  continua disponível como complemento, se isso virar um problema.
