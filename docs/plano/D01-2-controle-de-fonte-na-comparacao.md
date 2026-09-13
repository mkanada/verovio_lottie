# D01-2 — Controle total da fonte usada por `compare` (SVG e dotLottie)

**Depende de:** D01 (embutiu Liberation Serif de verdade no `.lottie`; vendorizou
`verovio/data/text/LiberationSerif-{Regular,Italic,Bold}.ttf`), A05 (script de
comparação) · **Decisão necessária:** nenhuma nova — a fonte já foi decidida em
`docs/plano/decisoes/B03-texto.md` (Liberation Serif). Este passo só faz a
*ferramenta de comparação* (`compare`) respeitar essa escolha nos dois lados
(SVG e dotLottie) em vez de depender de fontes instaladas no sistema.

Aberto em paralelo à Fase D principal (D02...) a pedido do usuário, depois de
D01 revelar que, sem isso, a % de divergência do `compare-corpus.sh` não é um
proxy confiável pra "o exportador está certo" quando envolve texto comum (ver
`docs/plano/D01-texto-comum.md`, seção "Notas de execução" → "Achado
importante").

## Problema (medido, não hipotético)

Hoje `compare svg-to-png` (`resvg`/`usvg`) e `compare lottie-to-png`
(`dotlottie-rs`/ThorVG) **não usam a mesma fonte física** pro texto comum
("Times, serif" no SVG do Verovio), e não há como forçar as duas a
concordarem:

- **Lado dotLottie: já tem controle total** (desde D01/T1) — o `.lottie`
  embute o `.ttf` de verdade (`f/LiberationSerif-*.ttf`) e o `dotlottie-rs`
  sempre usa exatamente esse arquivo (`origin:3`, sem substituição). Nada a
  fazer aqui.
- **Lado SVG: zero controle hoje.** `svg_to_png` (`compare/src/main.rs:154-176`)
  chama `opt.fontdb_mut().load_system_fonts()` (linha 167) e resolve
  "Times, serif"/"Liberation, serif" via **fontconfig do sistema operacional**
  — neste ambiente, `fc-match "Times, serif"` e `fc-match "serif"` resolvem
  pra **Nimbus Roman**, não Liberation Serif. `--font <arquivo>` (linhas
  172-176) só *adiciona* fontes ao `fontdb`; não muda qual família o genérico
  `serif` resolve, então carregar a Liberation Serif por `--font` **não teve
  nenhum efeito medido** (PNG byte-idêntico com e sem a flag — confirmado
  nesta sessão). Também tentei ativar `verovio --font-text-liberation` (troca
  `font-family` pra `"Liberation, serif"` e embute a fonte via `@font-face`
  base64 no `<style>` do SVG): **também sem efeito**, por dois motivos
  confirmados — (a) o `usvg` 0.48.1 não implementa `@font-face`/at-rules
  nenhuma (usa `simplecss`, um matcher só de seletores); (b) o Verovio escreve
  o nome curto `"Liberation"`, que não bate com o nome de família registrado
  no sistema (`"Liberation Serif"`, com espaço) — cai no genérico `serif` de
  novo, mesmo resultado de sempre. Só editando o SVG à mão pra
  `font-family="Liberation Serif, serif"` (nome completo, batendo o que o
  `fc-match` reconhece) o `resvg` trocou de fonte de verdade (confirmado:
  PNG mudou 0,4977% dos pixels a tolerância 0 contra o original) — mas isso
  não é uma correção viável (não dá pra depender de o Verovio escrever o
  nome completo, nem de editar SVGs à mão em todo teste).
- Sem fixar isso, qualquer `compare diff`/`compare-page.sh`/
  `compare-corpus.sh` envolvendo texto comum está comparando **contornos de
  fontes fisicamente diferentes** por causa de uma substituição de fonte que
  varia por máquina/ambiente (depende do que está instalado e de como o
  fontconfig local resolve genéricos) — não é reproduzível nem confiável
  como critério de aceite.

## Objetivo

Fazer `compare svg-to-png` usar, de forma determinística e independente do
que está instalado no sistema, exatamente os mesmos arquivos `.ttf` que o
exportador dotLottie embute (`verovio/data/text/LiberationSerif-{Regular,
Italic,Bold}.ttf`) — pra que uma comparação SVG vs. dotLottie de texto comum
deixe de depender de qual fonte o `fc-match` do ambiente escolhe.

## Ler antes (só isto)

- `compare/src/main.rs:154-197` (`svg_to_png`) — `opt.fontdb_mut()
  .load_system_fonts()` (L167) e o laço `--font`/`load_font_file` (L172-176)
  que este passo estende.
- `fontdb` 0.24.0 (`~/.cargo/registry/src/index.crates.io-*/fontdb-0.24.0/
  src/lib.rs:606-660`): `Database::set_serif_family(name)` (e as variantes
  `set_sans_serif_family`/`set_monospace_family`/etc., não usadas aqui) —
  troca pra qual família o genérico CSS `Family::Serif` resolve,
  independente de quais fontes foram carregadas ou do que o SO tem
  instalado. É um campo simples (`self.family_serif = name`), pode ser
  chamado a qualquer momento antes de `usvg::Tree::from_data`.
- `compare/scripts/compare-page.sh:110-130` e
  `compare/scripts/compare-corpus.sh:37-95` — os dois únicos lugares que
  chamam `svg-to-png` de verdade (o ramo de `compare-page.sh` que recebe um
  `.lottie` pronto, L47-101, reaproveita um PNG de SVG já gerado antes e
  **não** chama `svg-to-png` de novo — não precisa mudar).
- `verovio/data/text/LiberationSerif-{Regular,Italic,Bold}.ttf` (D01) — os
  arquivos-fonte que também vão pro `f/*.ttf` do pacote `.lottie`
  (`verovio/src/toolkit.cpp`, `EmbedCommonTextFonts`); usar exatamente estes
  mesmos arquivos aqui, não uma cópia do sistema, pra não reabrir o mesmo
  problema por outra porta.
- `compare/README.md`, as duas seções "Pegadinha" adicionadas em D01
  ("texto comum centralizado/à direita com `<title>` aninhado" e "`--font`
  não força o `resvg` a trocar a fonte") — a segunda é exatamente o problema
  que este passo resolve; a primeira é um bug de medição de largura
  **independente da fonte** (não é resolvido por este passo, ver "Fora de
  escopo").
- `docs/descricao-do-projeto.md`/`CLAUDE.md` — nada de arquitetura do
  exportador muda aqui; é só a ferramenta de teste (`compare/`).

## Decisões de escopo tomadas aqui

- **Não mexer no Verovio nem no exportador dotLottie.** O problema é
  inteiramente do lado do `compare`/`resvg`; `--font-text-liberation` do
  Verovio não é necessário (o SVG já termina toda lista `font-family` no
  genérico `serif` — `"Times, serif"` — então fixar pra qual família esse
  `serif` resolve já cobre o texto comum inteiro, sem precisar trocar a CLI
  do Verovio nem o nome escrito no SVG).
- **Fixar só `serif`** (`set_serif_family`), não os outros genéricos
  (`sans-serif`/`monospace`/etc.) — o SVG do Verovio nunca usa outro
  genérico pra texto comum (confirmado em `compare/README.md`: "o único
  `font-family` que aparece é `Times, serif`"). Se isso mudar no futuro,
  revisitar.
- **Flag nova, genérica, não hardcoded**: `--pin-serif-family <NOME>` em
  `svg-to-png` (nome de família livre, não fixo em "Liberation Serif" no
  binário) — mantém `compare` reutilizável pra outras fontes/testes; quem
  fixa "Liberation Serif" são os *scripts* (`compare-page.sh`/
  `compare-corpus.sh`), carregando os `.ttf` de D01 via `--font` (comportamento
  já existente) e passando `--pin-serif-family "Liberation Serif"` (nome
  exato registrado pelas próprias fontes, confirmado via `fc-match`).
- **`load_system_fonts()` continua** (não remover) — continua sendo o
  fallback pra qualquer outra fonte/família que apareça num SVG fora do
  padrão "Times, serif" do Verovio (ex.: um MEI com uma peça exótica); só o
  genérico `serif` fica fixo, o resto do comportamento de hoje não muda.
- **Bug do `<title>` aninhado com `text-anchor` (segunda "Pegadinha" de
  D01) fica fora daqui** — é um problema de medição de largura do `resvg`,
  ortogonal a qual fonte é usada (reproduzido com a fonte do sistema, não
  testado se a troca de fonte muda o comportamento, mas não é o objetivo
  deste passo). Ver "Fora de escopo".

## Arquivos

- Modificar: `compare/src/main.rs` (`svg_to_png`: novo parâmetro/flag
  `--pin-serif-family`).
- Modificar: `compare/scripts/compare-page.sh`,
  `compare/scripts/compare-corpus.sh` (adicionar os 3 `.ttf` de
  `verovio/data/text/` ao array de `--font` já existente, e passar
  `--pin-serif-family "Liberation Serif"` na chamada de `svg-to-png`).
- Não modificar: `verovio/` (nenhum arquivo do exportador).

## O que fazer

1. Em `compare/src/main.rs`, adicionar ao `Command::SvgToPng`:
   ```rust
   /// Nome de família a que o genérico CSS "serif" deve resolver,
   /// independente do que o SO tem instalado (ver docs/plano/
   /// D01-2-controle-de-fonte-na-comparacao.md). Precisa bater o nome de
   /// família de uma fonte já carregada via --font.
   #[arg(long)]
   pin_serif_family: Option<String>,
   ```
   e em `svg_to_png` (assinatura ganha `pin_serif_family: Option<&str>`),
   logo depois do laço que carrega `--font` (L172-176) e antes de
   `usvg::Tree::from_data`:
   ```rust
   if let Some(family) = pin_serif_family {
       opt.fontdb_mut().set_serif_family(family);
   }
   ```
2. Repassar o novo parâmetro em `main()` (`Command::SvgToPng { input,
   output, fonts, pin_serif_family } => svg_to_png(&input, &output, &fonts,
   pin_serif_family.as_deref())`).
3. Em `compare-page.sh` e `compare-corpus.sh`: adicionar ao array `FONTS`
   já existente (mesmo padrão dos 4 `--font` de SMuFL) os 3 arquivos
   `"$REPO_ROOT/verovio/data/text/LiberationSerif-Regular.ttf"`,
   `-Italic.ttf`, `-Bold.ttf`; e na chamada de `svg-to-png` (só a que
   renderiza a partir de um arquivo de partitura, não a que reaproveita PNG
   de um `.lottie` pronto), acrescentar `--pin-serif-family
   "Liberation Serif"`.
4. Rebuild: `cd compare && cargo build --release`.

## Fora de escopo

- Bug do `resvg` com `<title>` aninhado + `text-anchor` (medição de largura
  errada pra título/nome de compositor) — problema de medição, não de
  fonte; não resolvido por este passo. Ver `compare/README.md`, seção
  "Pegadinha do `resvg`: texto comum centralizado/à direita com `<title>`
  aninhado".
- Remedir/atualizar `docs/plano/relatorio-paridade.md` ou os números de
  `docs/plano/D01-texto-comum.md` com os valores corrigidos — é trabalho de
  acompanhamento depois que este passo estiver de pé (e depois do bug do
  `<title>`, se for resolvido, pra não misturar duas correções na mesma
  leitura de números). Deixar registrado no CSV de uma rodada nova de
  `compare-corpus.sh`, mas não precisa reescrever a análise de A13 aqui.
- Qualquer mudança no Verovio (`--font-text-liberation` ou outra), no
  formato do `.lottie`, ou nas fontes vendorizadas de D01.
- Fixar outros genéricos CSS (`sans-serif`, `monospace`, etc.) — não usados
  hoje pelo SVG do Verovio pra texto comum.

## Critérios de aceite

- Compila (`cd compare && cargo build --release`).
- Rodar duas vezes o mesmo `svg-to-png` sobre o mesmo SVG, uma vez sem
  `--pin-serif-family` e outra com — os dois PNGs devem ser **diferentes**
  (prova de que a flag teve efeito real; hoje `--font` sozinho não muda
  nada, então esse é o teste que hoje falharia).
- Com `--pin-serif-family "Liberation Serif"` + as 3 fontes de D01
  carregadas via `--font`: inspeção visual de "Allegro molto agitato."
  (negrito) e "legatissimo"/"peu à peu cresc. et animé" (itálico) confirma
  que a fonte renderizada é a Liberation Serif de verdade (não mais Nimbus
  Roman/outra substituta) e que os estilos itálico/negrito continuam
  corretos (fontdb escolhendo a face certa dentro da família).
- `compare/scripts/compare-page.sh`/`compare-corpus.sh` rodam sem erro com
  as mudanças (as fontes de D01 sendo encontradas e carregadas sem falha).
- `compare-corpus.sh 32` rodado de novo sobre o corpus inteiro: documentar
  os números novos (mín/máx/média) comparando com os de
  `docs/plano/D01-texto-comum.md` (antes: 0,1115%–0,8206%, média 0,3601%)
  — não precisa "bater a categoria 2 de A13" sozinho (o bug do `<title>`
  ainda está de pé, fora de escopo daqui), só documentar se a % caiu e por
  quanto, isolando o efeito de ter a fonte certa.

## Notas de execução

Implementado exatamente como planejado (`--pin-serif-family` em
`svg-to-png`, chamando `fontdb.set_serif_family()` depois do laço de
`--font` e antes de `usvg::Tree::from_data`; os 3 `.ttf` de D01 e a flag
adicionados aos dois scripts). Nenhum ajuste de desenho foi necessário.

Critérios de aceite verificados:

- `cargo build --release` compila sem warnings novos.
- Mesmo SVG (`Chopin_Etude_Op10_No9`, p.1) renderizado com e sem
  `--pin-serif-family "Liberation Serif"` (mesmas 7 fontes carregadas via
  `--font` nos dois casos): PNGs diferem em 0,4977% dos pixels a
  tolerância 0 — mesma magnitude da edição manual de SVG documentada no
  "Problema" acima, confirmando que a flag tem efeito real (ao contrário
  de `--font` sozinho).
- Inspeção visual por crop confirma Liberation Serif de verdade nos dois
  estilos: "Allegro molto agitato." (Chopin, negrito) e "legatissimo"
  (Chopin, itálico) e "peu à peu cresc. et animé" (Clair de Lune, itálico,
  acentos preservados) — todos com a face correta da família (negrito
  visivelmente mais pesado, itálico visivelmente inclinado, não Regular).
- `compare-page.sh` e `compare-corpus.sh` (corpus inteiro, 5 peças MEI + 5
  MusicXML, 34 páginas) rodam sem erro com as mudanças.
- `compare-corpus.sh 32` sobre o corpus inteiro (pacotes `dotlottie`,
  já com fonte comum embutida desde D01): **mín 0,1002%, máx 0,6646%,
  média 0,3154%** — contra mín 0,1115%/máx 0,8206%/média 0,3601% medido em
  `docs/plano/D01-texto-comum.md` antes deste passo. Queda visível no
  máximo (−0,156 p.p.) e na média (−0,045 p.p.); mínimo essencialmente
  igual. Como esperado, **não** chega ao nível de ruído puro de
  antialiasing (categoria 2 de `relatorio-paridade.md`) — o bug do
  `<title>` aninhado com `text-anchor` (fora de escopo aqui, ver
  `compare/README.md`) continua contribuindo divergência de texto em
  título/compositor independente de qual fonte é usada. CSV completo em
  `compare/out/corpus/resultado.csv` (não versionado, gerado localmente).
