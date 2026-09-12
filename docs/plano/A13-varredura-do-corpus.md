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

_(preencher ao executar)_
