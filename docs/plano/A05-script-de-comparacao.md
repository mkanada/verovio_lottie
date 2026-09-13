# A05 — Script de comparação por página + fontes do Verovio no `compare`

**Depende de:** A04 · **Decisão:** nenhuma

## Objetivo

Um único comando que, para um arquivo e uma página, gera o PNG do SVG, o PNG do
Lottie e a imagem de diferença — reutilizado como critério de aceite de A06 em
diante. E tornar a comparação justa carregando as fontes do Verovio no `resvg`.

## Ler antes (só isto)

- `compare/README.md`.
- `compare/src/main.rs` — função `svg_to_png` (hoje carrega só as fontes do
  sistema com `opt.fontdb_mut().load_system_fonts()`).
- `verovio/tools/main.cpp` L201-L203 — flags `-o` (arquivo de saída) e `-p` (página).

## Arquivos

- Modificar: `compare/src/main.rs`, `compare/.gitignore`, `compare/README.md`.
- Criar: `compare/scripts/compare-page.sh`.

## O que fazer

1. `compare svg-to-png`: nova opção repetível `--font <arquivo>` que chama
   `opt.fontdb_mut().load_font_file(caminho)` além das fontes do sistema.
   Motivo: o SVG do Verovio usa `font-family="Leipzig"` (ou a fonte escolhida)
   para texto SMuFL através de um `@font-face` woff2 embutido, que o `resvg` não
   carrega. Confirme esse comportamento; se o `resvg` usar o `@font-face`,
   registre nas notas e a opção vira dispensável.
   Fontes no repositório: `verovio/fonts/Leipzig/Leipzig.ttf`,
   `verovio/fonts/Bravura/Bravura.otf`, `verovio/fonts/Leland/Leland.otf`,
   `verovio/fonts/Gootville/Gootville.otf`.
2. `compare/.gitignore`: acrescentar `/out`.
3. `compare/scripts/compare-page.sh <arquivo> <página> [tolerância]`:
   - resolver a raiz do repositório a partir da localização do script (funcionar
     de qualquer diretório);
   - usar `verovio/tools/verovio` e `compare/target/release/compare`; se algum não
     existir, sair com mensagem dizendo como compilar;
   - gerar em `compare/out/`: `<nome>-p<N>.svg`, `<nome>-p<N>.json`,
     `<nome>-p<N>-svg.png`, `<nome>-p<N>-lottie.png`, `<nome>-p<N>-diff.png`;
   - `verovio -t svg -p N` e `verovio -t lottie -p N`, sempre com
     `--resource-path verovio/data` e as mesmas opções;
   - `compare svg-to-png` com `--font` para as quatro fontes acima;
   - ler largura/altura do PNG do SVG (ex.: `python3` lendo o cabeçalho IHDR com
     `struct`) e repassar a `compare lottie-to-png --width --height`;
   - `compare diff` com a tolerância (padrão 32), imprimindo as estatísticas.
4. `compare/README.md`: seção curta sobre o script.

## Fora de escopo

Varredura do corpus inteiro (A13); suporte a `.lottie` no script (A12).

## Critérios de aceite

- `cd compare && cargo build --release` compila.
- `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`
  gera os 3 PNGs. Nesta etapa o PNG do Lottie está vazio e o diff é grande —
  esperado.
- Renderizando o SVG de `corpus/mei/Chopin_Etude_Op10_No9.mei` página 1 com e sem
  `--font`, as dinâmicas (ex.: *p*, *f*) mudam para a forma da fonte Leipzig
  quando `--font` é usado.
- `git status` não mostra `compare/out/`.

## Notas de execução

- **`--font` implementado, mas dispensável no corpus atual**: verificado nos 5
  MEI do corpus (página 1 de cada) que o SVG do Verovio **nunca** usa
  `@font-face`/`font-family` de fonte musical — nenhum `@font-face` aparece no
  SVG gerado, e o único `font-family` presente é `Times, serif` (texto comum:
  títulos, indicações, letra). Dinâmicas e demais glifos SMuFL saem sempre
  como `<use xlink:href="#...">` referenciando `<path>` vetorial em `<defs>`
  (consistente com o que já constava no mapa do código do README do plano).
  Por isso o critério de aceite "as dinâmicas mudam de forma com/sem
  `--font`" **não se verifica**: rodei
  `compare svg-to-png` no SVG da página 1 do `Chopin_Etude_Op10_No9.mei` com e
  sem `--font` (as 4 fontes) e o diff pixel a pixel deu 0 pixels diferentes
  (tolerância 32). A opção foi mantida mesmo assim (implementada exatamente
  como pedido) por segurança/futuro — não custa nada e cobre o caso de algum
  MEI vir a produzir texto solto com fonte SMuFL. Documentado em
  `compare/README.md`.
- **Truncamento de nome de saída do Verovio**: `tools/main.cpp`
  (`RemoveExtension`) corta o `-o <caminho>` a partir do **último** `.` do
  caminho inteiro — inclusive um `.` inicial de "dotfile". Isso quebraria
  nomes de saída para entradas cujo nome já tem pontos (vários `.mxl` do
  corpus, ex. `Chopin_-_Nocturne_Op._9_No._1.mxl`) e também quebrou uma
  primeira tentativa de usar um prefixo temporário `.compare-page-tmp` (virou
  `compare/out/.svg`, vazio). Corrigido usando um prefixo temporário sem
  nenhum ponto (`_compare-page-tmp`) e renomeando para o nome final
  (`<nome>-p<N>.svg`/`.json`) depois — assim o script funciona mesmo para
  nomes de entrada com pontos, embora isso esteja fora do escopo de teste
  deste passo (só corpus MEI, sem pontos no nome).
- Critérios de aceite restantes confirmados: `cargo build --release` compila;
  `compare/scripts/compare-page.sh corpus/mei/Grieg_Little_bird_Op43_No4.mei 1`
  gera os 5 arquivos esperados em `compare/out/` (PNG do Lottie vazio/diff
  grande, como esperado — ainda não há exportador dotLottie de verdade); rodei
  também de fora do repositório (`cd /tmp && .../compare-page.sh <caminho
  absoluto> 1`) para confirmar que o script funciona de qualquer diretório;
  `git status` não mostra `compare/out/`.
