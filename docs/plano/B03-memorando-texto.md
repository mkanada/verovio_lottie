# B03 — Memorando: texto comum

**Depende de:** A13 (usa o relatório para medir impacto) · **Decisão:** produz
D-TEXTO, **tomada pelo usuário**.

## Objetivo

Escolher como renderizar texto comum (títulos, compositor, andamento,
dedilhados, letra) no `.lottie`.

## Fatos já verificados

- `verovio/data/text/Times*.xml` só tem métricas (zero `<path>`): o Verovio não
  tem contornos de texto comum.
- No SVG o texto é `<text>`/`<tspan>` com `font-family` (padrão "Times, serif";
  a opção `fontTextLiberation` embute Liberation em woff2 via
  `verovio/data/Liberation.css`). O visual depende da fonte disponível no
  visualizador.
- O layout do Verovio usa as métricas de `data/text` para posicionar o texto.
- dotLottie v2 aceita fontes empacotadas: a fixture `elapsed_time.lottie` do
  `dotlottie-rs` traz `f/Bebas-Regular.ttf`, referenciada na animação em
  `fonts.list[].fPath` com `"origin": 3`.

## Opções a avaliar

- **T1** — camadas de texto Lottie com fonte TTF empacotada em `f/`.
- **T2** — converter texto em contornos na exportação (ex.: vendorizar
  `stb_truetype.h`, domínio público, + uma TTF de métricas compatíveis com Times,
  como Liberation Serif, licença SIL OFL) e gerar shapes como os glifos.
- **T3** — não renderizar texto comum e documentar a limitação.

## Critérios

Fidelidade ao SVG; consistência entre players; suporte a texto no
ThorVG/`dotlottie-rs`; tamanho do arquivo; licença da fonte; complexidade;
impacto medido no relatório de A13 (quantas páginas/trechos afetados).

## Spike mínimo

Um `.lottie` feito à mão com uma camada de texto e fonte embutida, renderizado
pelo `compare lottie-to-png`, para confirmar se T1 funciona no `dotlottie-rs`.

## Entregável

`docs/plano/decisoes/B03-texto.md` com recomendação e pergunta ao usuário.
Pare para a decisão; depois, detalhe D01 (ver D00).

## Notas de execução

- Memorando produzido em `docs/plano/decisoes/B03-texto.md`, com spike de T1
  executado de verdade (não só leitura de código): `.lottie` feito à mão com
  camada de texto + `LiberationSerif-Regular.ttf` embutido renderiza
  corretamente via `compare lottie-to-png` no `dotlottie-rs`/ThorVG,
  diacríticos incluídos.
- Recomendação: **T2** (contornos via `stb_truetype`, reaproveitando o
  pipeline de `MakeGlyphShape`/`LottieBezier` já usado para SMuFL), com T1
  como alternativa defensável mas de custo de tamanho medido maior (~208
  KB/estilo comprimido; corpus já usa itálico com frequência, então T1 real
  precisaria de ~400-450 KB fixos por peça). Aguardando decisão do usuário —
  ver perguntas explícitas no memorando.
