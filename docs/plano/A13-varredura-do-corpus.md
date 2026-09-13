# A13 — Varredura do corpus e relatório de paridade

**Depende de:** A12 · **Decisão:** nenhuma

## Objetivo

Medir a paridade visual em todas as páginas do corpus e produzir a lista de
divergências que alimenta a fase D e o memorando B03. Nada é corrigido aqui.

Páginas por peça no layout padrão (para conferência): Chopin Étude 4, Chopin
Mazurka 3, Grieg Butterfly 3, Grieg Little bird 2, Scarlatti 3, Chopin Nocturne
7, Clair de Lune 5, Gymnopédie 2, Maple Leaf Rag 3, Prelúdio BWV 846 2 — total 34.

## Ler antes (só isto)

- `compare/README.md`.
- `compare/scripts/compare-page.sh` (A05/A12).

## Arquivos

- Criar: `compare/scripts/compare-corpus.sh`, `docs/plano/relatorio-paridade.md`.

## O que fazer

1. `compare/scripts/compare-corpus.sh`: para cada arquivo de `corpus/mei` e
   `corpus/musicxml`, gerar o SVG de todas as páginas (`-a`) e o `.lottie`; para
   cada página: PNG do SVG (com as fontes do Verovio), PNG do frame
   correspondente, `compare diff` com `--tolerance 32` (parâmetro do script);
   coletar a porcentagem de pixels divergentes. Saídas em `compare/out/corpus/`.
2. `docs/plano/relatorio-paridade.md` com:
   - data e commit (`git rev-parse --short HEAD`);
   - tabela: peça, página, % divergente;
   - tamanho de cada `.lottie` (risco de tamanho de arquivo);
   - lista **categorizada** de divergências observadas nas imagens de diff (ex.:
     "texto comum ausente", "contorno extra em ligadura", "rotação", "cor"), cada
     categoria com 1-2 exemplos (peça/página) e a primitiva/elemento provável.

## Fora de escopo

Corrigir divergências (viram passos D).

## Critérios de aceite

- A varredura roda sem crash nas 34 páginas.
- Relatório criado, com categorias acionáveis.

## Notas de execução

- `compare/scripts/compare-corpus.sh [tolerância]` criado seguindo o padrão
  de `compare-page.sh`: para cada arquivo de `corpus/mei`/`corpus/musicxml`,
  gera SVG de todas as páginas (`-t svg -a`) e o pacote `-t dotlottie`, e por
  página faz `svg-to-png` (com as fontes do Verovio), `lottie-to-png --frame
  N-1` e `diff --tolerance 32`. Saídas em `compare/out/corpus/<peça>/` +
  `resultado.csv` (peça, página, % divergente, pixels) + `tamanhos.txt`.
- **Pegadinha nova (mesma raiz da já documentada em `compare-page.sh`)**:
  usar o diretório de trabalho por peça (nome vindo do arquivo de entrada,
  com pontos para `.mxl` como `Chopin_-_Nocturne_Op._9_No._1`) como prefixo
  do `-o` do verovio corrompe o nome de saída, porque o `RemoveExtension`
  trunca a partir do **último ponto do caminho inteiro**, não só da extensão
  do arquivo — pontos no nome do diretório contam. Corrigido gerando sempre
  num prefixo fixo sem pontos (`compare/out/corpus/_compare-corpus-tmp`) e
  só movendo pro nome final depois, via `mv` do script.
- Rodado nas 34 páginas (5 MEI + 5 MusicXML, contagem batendo com a tabela
  do enunciado) sem crash, `Unable to write` ou warning de asserção.
  Relatório em `docs/plano/relatorio-paridade.md`: divergência por página
  entre 0,11% e 0,77% (média 0,33%), toda explicada por duas categorias —
  texto comum ausente (dominante, esperado, D-TEXTO/B03) e ruído de
  antialiasing no canal alfa em traços finos/curvas (acolada, ties, slurs,
  linha de pedal) causado pelo próprio `compare diff` incluir alfa no
  `channel_diff`, não por divergência geométrica real (confirmado por
  amostragem de pixel: RGB idêntico, alfa diferente na borda). Nenhuma
  divergência estrutural (posição, forma, cor, rotação) encontrada.
- Tamanho dos `.lottie`: 77 KB (Gymnopédie, 2 páginas) a 430 KB (Nocturne, 7
  páginas), ~40-83 KB/página — não alarmante ainda, mas o corpus é pequeno e
  texto comum nem está sendo desenhado; reavaliar nas fases C/D.
