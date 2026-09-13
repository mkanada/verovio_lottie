# Relatório de paridade visual — SVG vs. dotLottie

**Data:** 2026-09-13 · **Commit:** `27066a2` (com as alterações de A12 —
`Toolkit::RenderToLottieAnimation`/`RenderToDotLottieFile`, CLI `dotlottie` — e
o script `compare-corpus.sh` desta varredura ainda não commitadas no
working tree; ver `git diff` para o estado exato).

Gerado por `compare/scripts/compare-corpus.sh 32` (tolerância 32/255, a mesma
usada nos passos A06-A12). Saídas completas (SVG, JSON, `.lottie`, PNGs e
diffs por página) em `compare/out/corpus/` (não versionado). CSV bruto em
`compare/out/corpus/resultado.csv`.

## Resumo

- **34/34 páginas** processadas sem crash (5 partituras MEI + 5 MusicXML do
  corpus, breakdown abaixo).
- Divergência por página: **min 0,1128% – max 0,7658% – média 0,3269%**
  (todas as páginas bem abaixo de 1%).
- Nenhuma divergência estrutural encontrada (posição/forma errada de nota,
  cor errada, rotação, contorno extra) — toda a divergência observada se
  explica por duas causas conhecidas, ambas detalhadas em "Divergências
  categorizadas" abaixo.

## Tabela: peça, página, % divergente

| Peça | Pág. | % divergente | Peça | Pág. | % divergente |
| --- | --- | --- | --- | --- | --- |
| Chopin Étude Op.10 No.9 | 1 | 0,4842% | Chopin Nocturne Op.9 No.1 | 1 | 0,5819% |
| Chopin Étude Op.10 No.9 | 2 | 0,3793% | Chopin Nocturne Op.9 No.1 | 2 | 0,4935% |
| Chopin Étude Op.10 No.9 | 3 | 0,2879% | Chopin Nocturne Op.9 No.1 | 3 | 0,7055% |
| Chopin Étude Op.10 No.9 | 4 | 0,1483% | Chopin Nocturne Op.9 No.1 | 4 | 0,7225% |
| Chopin Mazurka Op.6 No.1 | 1 | 0,3468% | Chopin Nocturne Op.9 No.1 | 5 | 0,5366% |
| Chopin Mazurka Op.6 No.1 | 2 | 0,1899% | Chopin Nocturne Op.9 No.1 | 6 | 0,5476% |
| Chopin Mazurka Op.6 No.1 | 3 | 0,1398% | Chopin Nocturne Op.9 No.1 | 7 | 0,2399% |
| Grieg Butterfly Op.43 No.1 | 1 | 0,2057% | Clair de Lune (Debussy) | 1 | 0,7658% |
| Grieg Butterfly Op.43 No.1 | 2 | 0,1464% | Clair de Lune (Debussy) | 2 | 0,5260% |
| Grieg Butterfly Op.43 No.1 | 3 | 0,1157% | Clair de Lune (Debussy) | 3 | 0,3385% |
| Grieg Little bird Op.43 No.4 | 1 | 0,2009% | Clair de Lune (Debussy) | 4 | 0,4578% |
| Grieg Little bird Op.43 No.4 | 2 | 0,1194% | Clair de Lune (Debussy) | 5 | 0,1744% |
| Scarlatti Sonata in C major | 1 | 0,1680% | Gymnopédie No.1 (Satie) | 1 | 0,3649% |
| Scarlatti Sonata in C major | 2 | 0,1450% | Gymnopédie No.1 (Satie) | 2 | 0,1162% |
| Scarlatti Sonata in C major | 3 | 0,1128% | Maple Leaf Rag (Joplin) | 1 | 0,3530% |
| | | | Maple Leaf Rag (Joplin) | 2 | 0,2889% |
| | | | Maple Leaf Rag (Joplin) | 3 | 0,1799% |
| | | | Prelúdio BWV 846 No.1 | 1 | 0,3258% |
| | | | Prelúdio BWV 846 No.1 | 2 | 0,2052% |

Médias por peça (min–max): Étude 0,32% (0,15–0,48), Mazurka 0,23%
(0,14–0,35), Butterfly 0,16% (0,12–0,21), Little bird 0,16% (0,12–0,20),
Scarlatti 0,14% (0,11–0,17), Nocturne 0,55% (0,24–0,72), Clair de Lune 0,45%
(0,17–0,77), Gymnopédie 0,24% (0,12–0,36), Maple Leaf Rag 0,27% (0,18–0,35),
Prelúdio BWV 846 0,27% (0,21–0,33).

Padrão notado: quanto mais texto comum (títulos longos, muitas indicações
italianas, dedilhados, números de compasso), maior a % — consistente com a
causa raiz dominante (ver categoria 1 abaixo), não com um problema por peça.

## Tamanho dos pacotes `.lottie`

| Peça | Páginas | Tamanho | KB/página |
| --- | --- | --- | --- |
| Chopin Nocturne Op.9 No.1 | 7 | 429,4 KB | 61,3 |
| Clair de Lune (Debussy) | 5 | 366,2 KB | 73,2 |
| Chopin Étude Op.10 No.9 | 4 | 315,4 KB | 78,8 |
| Grieg Butterfly Op.43 No.1 | 3 | 248,2 KB | 82,7 |
| Maple Leaf Rag (Joplin) | 3 | 242,0 KB | 80,7 |
| Chopin Mazurka Op.6 No.1 | 3 | 227,4 KB | 75,8 |
| Scarlatti Sonata in C major | 3 | 147,0 KB | 49,0 |
| Grieg Little bird Op.43 No.4 | 2 | 155,3 KB | 77,7 |
| Prelúdio BWV 846 No.1 | 2 | 137,5 KB | 68,7 |
| Gymnopédie No.1 (Satie) | 2 | 77,2 KB | 38,6 |

Nenhum arquivo passa de ~430 KB (7 páginas). Risco conhecido #3 do
`README.md` (coordenadas "assadas" por glifo, sem `<use>`) ainda não parece
crítico neste corpus — ~50-83 KB/página —, mas o corpus é pequeno (5-16
compassos por peça em geral) e texto comum ainda nem está sendo desenhado
(vai aumentar o tamanho quando D-TEXTO for resolvido). Reavaliar quando a
fase D adicionar texto e, se houver, a fase C adicionar a state machine.

## Divergências categorizadas

### 1. Texto comum ausente (causa dominante, ~95%+ dos pixels divergentes)

Esperado e já documentado em A10/D-TEXTO: `LottieDeviceContext` não desenha
texto de fonte comum (só conta e emite `LogWarning`). Cobre: títulos,
compositor, andamento (ex. "Andante très expressif", "Tempo Di Marcia"),
indicações italianas/francesas (*cresc.*, *a tempo*, *con sordina*, *m.g.*,
*poco rall.*), dedilhados, números de compasso/ensaio, os ícones de "mão
apontando" (☛) que marcam pedal una corda (são glifos de texto comum, não
SMuFL, então também ficam de fora), e o rodapé "MEI engraved with Verovio".
Nenhuma dinâmica SMuFL (*p*, *f*, *sfz* etc.) nem glifo musical aparece
incompleto — só texto Times.

- **Exemplo 1:** Clair de Lune p.1 (0,77% — a maior % da varredura): todo o
  vermelho do diff é título/subtítulo, "Andante très expressif", "Piano",
  "con sordina" + os dois ícones ☛☛, "Tempo rubato", "peu à peu cresc. et
  animé" e os números de compasso em itálico (49, 50, 51...). Confirmado por
  crop lado a lado SVG vs. Lottie: "con sordina ☛☛" simplesmente não existe
  no frame do Lottie.
- **Exemplo 2:** Chopin Étude Op.10 No.9 p.1 (0,48%, já registrado nas notas
  de execução de A10): título, "Allegro molto agitato.", indicações
  italianas, dedilhados, números de compasso e o rodapé — 26 "common text
  run(s) not rendered" no log.
- **Primitiva/elemento provável:** `LottieDeviceContext::DrawText` (ramo de
  fonte comum, `m_skippedTextRuns`) — sem mudança de código aqui; é o escopo
  do memorando B03 / decisão D-TEXTO.

### 2. Ruído de antialiasing (canal alfa) em traços finos e curvas

Categoria nova, não é um bug de geometria do exportador — é uma limitação da
**comparação**: `compare diff` (`compare/src/main.rs` `channel_diff` L259-265)
inclui os 4 canais RGBA na diferença por pixel. Nas bordas anti-aliased de
traços finos e curvas (acolada/chave do piano, ligaduras de nota *tie*,
ligaduras de frase *slur*, feixes curvos de colcheia, linhas de pedal
horizontais com gancho), o resvg (SVG) e o ThorVG/dotlottie-rs (Lottie)
produzem RGB **idêntico** (preto puro) mas alfa (opacidade da borda)
ligeiramente diferente — o suficiente pra passar da tolerância 32 e pintar um
rastro pontilhado vermelho ao longo do contorno, mesmo sem qualquer diferença
visual real.

- **Exemplo 1:** acolada (chave) do piano em Gymnopédie No.1 p.1 — no diff
  aparece como uma linha pontilhada vermelha acompanhando toda a curva; crop
  lado a lado (SVG vs. Lottie, ambos ampliados 3×) mostra a chave
  **visualmente idêntica**. Amostragem de pixel confirmou: RGB `(0,0,0)` nos
  dois, alfa 89 (SVG) vs. 56 (Lottie) num ponto, 197 vs. 255 noutro.
- **Exemplo 2:** linha de pedal (compassos 49-54) em Clair de Lune p.1 —
  mesmo padrão: traço horizontal com gancho vertical, RGB idêntico, alfa
  divergente na borda.
- **Primitiva/elemento provável:** rasterização de `DrawBezier`/traços finos
  em geral — não é específico de uma primitiva do `LottieDeviceContext`, é o
  renderizador de comparação (ThorVG via `dotlottie-rs`) fazendo
  antialiasing um pouco diferente do resvg. Não é uma divergência que a fase
  D precise corrigir no exportador; se incomodar a leitura dos relatórios
  futuros, a correção é no `compare` (ex.: ignorar alfa no `channel_diff`,
  ou comparar sobre fundo opaco antes do diff) — fora do escopo deste passo.

## Fora de escopo (não corrigido aqui)

Nenhuma correção foi feita — A13 é só medição. As duas categorias acima
alimentam B03 (categoria 1, já é o gatilho esperado) e uma nota para quem
pegar a fase D ou mexer no `compare` (categoria 2).

## Notas de execução

- Rodado com `compare/scripts/compare-corpus.sh 32` contra os 10 arquivos de
  `corpus/mei` + `corpus/musicxml` (34 páginas, contagem batendo com a
  tabela do enunciado do passo).
- **Bug encontrado e corrigido no script**: a primeira versão usava um
  diretório de trabalho por peça (nomeado a partir do arquivo de entrada,
  ex. `compare/out/corpus/Chopin_-_Nocturne_Op._9_No._1/`) como prefixo do
  `-o` passado ao `verovio`. Para peças cujo nome já tem "." (caso de vários
  `.mxl` do corpus), o `RemoveExtension` do verovio (`tools/main.cpp`)
  trunca o **caminho inteiro** a partir do último "." — inclusive pontos que
  vêm do nome do diretório, não só da extensão do arquivo — mesma pegadinha
  já documentada em `compare-page.sh`. Resultado observado: o `.lottie` da
  Nocturne saiu como `compare/out/corpus/Chopin_-_Nocturne_Op._9_No.lottie`
  (fora do diretório da peça, nome truncado), e o `mv` seguinte falhava.
  Corrigido usando um prefixo fixo e sem pontos
  (`compare/out/corpus/_compare-corpus-tmp`) para tudo que é gerado via
  `-o` do verovio, só renomeando/movendo para o nome final (com pontos) via
  `mv` do próprio script — igual à solução já usada em `compare-page.sh`.
  Rerun completo depois do fix: sem erros.
- Nenhum crash, nenhum "Unable to write", nenhum warning de asserção nos
  10 arquivos × (SVG todas as páginas + dotLottie + diff por página).
  Avisos vistos são esperados: "N common text run(s) not rendered (pending
  D-TEXTO)" (uma vez por página) e o aviso inofensivo de
  `set_frame(0)`/`render()` no primeiro frame de cada `.lottie` (pegadinha
  documentada em `compare/README.md`, um por peça já que cada `.lottie` é
  carregado do zero).
- Script novo `compare/scripts/compare-corpus.sh` segue o padrão de
  `compare-page.sh` (resolve a raiz do repo pela própria localização,
  valida os dois binários, mesma lista de fontes `--font`). Saídas em
  `compare/out/corpus/<peça>/` (SVG/PNG/JSON por página, `.lottie` único) +
  `compare/out/corpus/resultado.csv` (uma linha por página) +
  `compare/out/corpus/tamanhos.txt`.
