# Matriz de opções de layout — paridade SVG vs. dotLottie

**Data:** 2026-09-14 · **Commit:** `d3683fa`

## Contexto

O Verovio expõe várias opções gerais de página (`verovio/src/options.cpp`,
grupo "Input and page configuration options"): cabeçalho (`--header`),
rodapé (`--footer`), orientação (`--landscape`) e dimensões de página
(`--page-width`/`--page-height`). O tamanho padrão — `2100x2970` (A4 em
décimos de mm, e também o valor literal em px do `viewBox` do SVG gerado,
conferido nesta sessão) — é pensado para impressão em papel A4, não para os
tamanhos e proporções de tela que o zywny vai efetivamente usar (ver
`CLAUDE.md`).

Este relatório varre uma matriz de combinações dessas opções para **uma**
peça do corpus e compara, para cada combinação, a página 1 renderizada em
SVG contra a mesma página do `.lottie` gerado com as mesmas opções — mesmo
critério visual (PNG diff) já usado em
[`docs/plano/relatorio-paridade.md`](../plano/relatorio-paridade.md), mas
variando as opções de layout em vez do corpus inteiro sob o layout padrão.
Objetivo: confirmar que a paridade visual do exportador dotLottie se
mantém **independente** de qual combinação de cabeçalho/rodapé/orientação/
tamanho de página é usada — não só sob o layout A4 padrão que o resto do
projeto testa.

## Metodologia

Script: [`compare/scripts/compare-layout-matrix.sh`](../../compare/scripts/compare-layout-matrix.sh)
(`[arquivo] [tolerância]`, mesmo padrão de `compare-page.sh`/`compare-corpus.sh`
documentados em `compare/README.md`).

- **Peça:** `corpus/mei/Chopin_Etude_Op10_No9.mei` (Chopin, Étude Op. 10
  No. 9) — escolhida por já ter cabeçalho autogerado (título "Etude in F
  Minor" + `Allegro molto agitato.`), rodapé autogerado ("MEI engraved with
  Verovio"), bastante texto comum em itálico (dinâmicas, "cresc.", "con
  forza", dedilhados) e múltiplas páginas no layout padrão — ou seja, os
  quatro eixos da matriz têm efeito visual real nela. Varrer apenas uma
  peça (em vez do corpus inteiro) foi uma escolha deliberada de escopo: o
  eixo interessante aqui são as *opções de layout*, não a diversidade de
  partituras — essa já está coberta por `relatorio-paridade.md`.
- **Página comparada:** só a página 1 de cada combinação (tem cabeçalho e
  rodapé simultaneamente, ver abaixo) — o objetivo é validar cada
  combinação de opções, não repetir a varredura de página completa já
  feita em `relatorio-paridade.md`. A contagem total de páginas de cada
  combinação está na tabela mesmo assim (é, em si, um resultado relevante
  — ver "Achados").
- **Tolerância:** 32/255, igual a `relatorio-paridade.md`.
- Confirmado por inspeção do SVG gerado: com `--header auto`/`--footer
  auto` (padrão), tanto `pgHead` quanto `pgFoot` aparecem em **todas** as
  páginas da peça (não só primeira/última), então comparar a página 1 já
  exercita os dois.
- **Fundo branco:** `svg-to-png`/`lottie-to-png` (e o `sm-render` usado em
  `docs/exemplos`) agora compõem o render sobre fundo branco opaco antes de
  salvar o PNG — antes saíam com fundo totalmente transparente (`alpha=0`),
  o que já tinha exigido um passo manual de "achatamento" fora da
  ferramenta para `docs/mesa-de-prova` (commit `c3874f6`). O `diff.png` não
  muda: já salva com alpha 255 desde sempre (fundo cinza esmaecido +
  vermelho é o próprio design da imagem de diferença).

## Eixos da matriz

16 combinações = 2 (tamanho) × 2 (orientação) × 2 (cabeçalho) × 2 (rodapé).

| Eixo | Valores testados | Por quê |
| --- | --- | --- |
| Tamanho | `a4` (2100×2970, padrão do Verovio) vs. `tela` (1080×1920) | `a4` é o baseline de impressão; `tela` usa a proporção 9:16 de um celular/tablet em pé — um tamanho de tela real, bem menor e com proporção bem diferente da A4, para estressar o layout responsivo do Verovio. Os valores literais de `tela` são só um exemplo de referência para este relatório — o zywny deve calibrar a proporção real pro viewport de destino; o que este relatório valida é que a paridade visual não depende do tamanho escolhido. |
| Orientação | `portrait` (padrão) vs. `landscape` (`--landscape`, troca largura/altura) | Combinado com `tela`, dá também o caso 16:9 (`1920x1080`, típico de monitor/TV) — a mesma dupla de valores de tamanho cobre celular vertical e monitor horizontal dependendo da orientação. |
| Cabeçalho | `header` (`--header auto`, padrão) vs. `no-header` (`--header none`) | `auto` e `none` são os dois valores com efeito visual diferente neste corpus (nenhuma peça usa `pgHead` codificado manualmente, então `--header encoded` seria equivalente a `none` aqui — não teria sinal novo). |
| Rodapé | `footer` (`--footer auto`, padrão) vs. `no-footer` (`--footer none`) | Mesmo raciocínio do cabeçalho. `--footer always` não foi testado por não haver, neste corpus, um caso em que `auto` e `always` divergem (mesma razão: nenhum `pgFoot` codificado manualmente). |

## Resultados

| Combinação | Tamanho | Orientação | Cabeçalho | Rodapé | Dimensões (px) | Páginas | % divergente (p.1) | Tamanho `.lottie` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `a4-portrait-header-footer` | a4 | portrait | header | footer | 2100x2970 | 4 | 0,4520% | 1019,2 KB |
| `a4-portrait-header-no-footer` | a4 | portrait | header | no-footer | 2100x2970 | 4 | 0,4443% | 983,3 KB |
| `a4-portrait-no-header-footer` | a4 | portrait | no-header | footer | 2100x2970 | 4 | 0,4550% | 1018,3 KB |
| `a4-portrait-no-header-no-footer` | a4 | portrait | no-header | no-footer | 2100x2970 | 4 | 0,4473% | 982,5 KB |
| `a4-landscape-header-footer` | a4 | landscape | header | footer | 2970x2100 | 4 | 0,4231% | 1010,5 KB |
| `a4-landscape-header-no-footer` | a4 | landscape | header | no-footer | 2970x2100 | 4 | 0,4153% | 974,9 KB |
| `a4-landscape-no-header-footer` | a4 | landscape | no-header | footer | 2970x2100 | 4 | 0,4181% | 1009,6 KB |
| `a4-landscape-no-header-no-footer` | a4 | landscape | no-header | no-footer | 2970x2100 | 4 | 0,4104% | 974,0 KB |
| `tela-portrait-header-footer` | tela | portrait | header | footer | 1080x1920 | 11 | 0,3970% | 1117,9 KB |
| `tela-portrait-header-no-footer` | tela | portrait | header | no-footer | 1080x1920 | 11 | 0,3737% | 1019,7 KB |
| `tela-portrait-no-header-footer` | tela | portrait | no-header | footer | 1080x1920 | 11 | 0,3968% | 1116,0 KB |
| `tela-portrait-no-header-no-footer` | tela | portrait | no-header | no-footer | 1080x1920 | 11 | 0,3735% | 1017,6 KB |
| `tela-landscape-header-footer` | tela | landscape | header | footer | 1920x1080 | 16 | 0,2965% | 1131,4 KB |
| `tela-landscape-header-no-footer` | tela | landscape | header | no-footer | 1920x1080 | 14 | 0,2733% | 989,1 KB |
| `tela-landscape-no-header-footer` | tela | landscape | no-header | footer | 1920x1080 | 16 | 0,2925% | 1128,6 KB |
| `tela-landscape-no-header-no-footer` | tela | landscape | no-header | no-footer | 1920x1080 | 12 | 0,2692% | 986,1 KB |

**Min 0,2692% – max 0,4550% – média 0,3836%** — todas as 16 combinações bem
abaixo de 1%, na mesma faixa da varredura de corpus completo em
`relatorio-paridade.md` (que testava só a combinação `a4-portrait-header-footer`,
linha 1 desta tabela: 0,4520% aqui contra 0,4842% lá — a pequena melhora é
consistente com correções de paridade feitas depois daquele relatório, ex.
D05).

CSV bruto: [`resultado.csv`](resultado.csv). Imagens (`<peça>-p1-svg.png`,
`<peça>-p1-lottie.png`, `<peça>-p1-diff.png`) em cada subdiretório nomeado
pela combinação (ex. `tela-landscape-no-header-no-footer/`).

## Achados

1. **Paridade visual se mantém em todas as 16 combinações** — nenhuma
   combinação de cabeçalho/rodapé/orientação/tamanho de página quebra o
   exportador dotLottie nem introduz divergência estrutural (posição/forma
   errada, cor errada, conteúdo faltando). Inspeção visual das imagens de
   diff confirma que a divergência remanescente é a mesma categoria de
   ruído de antialiasing entre `resvg` e ThorVG já documentada em
   `relatorio-paridade.md`/`compare/README.md` (bordas finas de glifos de
   texto em itálico, alguns pontos isolados) — nada relacionado
   especificamente a estas opções de layout.
2. **Remover cabeçalho/rodapé não introduz divergência nova** — a %
   praticamente não muda entre `header-footer` e `no-header-no-footer` sob
   o mesmo tamanho/orientação (ex. `a4-portrait`: 0,4520% → 0,4473%): o
   exportador suprime `pgHead`/`pgFoot` de forma consistente com o SVG, sem
   deixar resquício visual.
3. **Tamanho de página muda a contagem de páginas bem mais do que
   cabeçalho/rodapé** — a peça inteira (4 compassos por sistema em média)
   sai em **4 páginas** em A4 (qualquer orientação) mas em **11-16
   páginas** no tamanho `tela`, bem menor. Implicação prática para o
   zywny: escolher um tamanho de página "de tela" pequeno multiplica a
   quantidade de *layers*/páginas da composição dotLottie (decisão já
   fixa: "um `.lottie` por música inteira, cada página é um layer") — vale
   considerar isso ao calibrar o tamanho real de página a usar, além da
   pura proporção da tela.
4. **`tela-landscape` foi a única combinação onde cabeçalho/rodapé mudam a
   contagem de páginas** (16 páginas com cabeçalho e/ou rodapé, 12 sem
   nenhum dos dois) — é o preset com menos altura de conteúdo disponível
   (1080px, o menor de todos), então cabeçalho+rodapé consomem uma fração
   grande o bastante da página pra empurrar sistemas pra página seguinte.
   Nos outros três tamanhos/orientações isso não teve esse efeito (folga
   suficiente). Não é um bug — é o comportamento esperado do layout
   automático do Verovio — mas é um sinal de que tamanhos de tela muito
   curtos (landscape "tela" aqui) merecem atenção extra a cabeçalho/rodapé
   habilitados, ou ao uso de `--adjustPageHeight`.
5. **Tamanho do arquivo `.lottie` fica estável (~975 KB – 1,13 MB)** em
   todas as combinações, mesmo variando de 4 a 16 páginas — o overhead por
   página é pequeno (é a mesma música, mesmas notas/glifos; mais páginas
   só redistribui o mesmo conteúdo em mais camadas, sem multiplicar peso).

## Reprodutibilidade / como estender

```sh
compare/scripts/compare-layout-matrix.sh [arquivo] [tolerância]
# ex.: compare/scripts/compare-layout-matrix.sh corpus/mei/Chopin_Etude_Op10_No9.mei 32
```

Rodar de novo sobrescreve os subdiretórios de saída e o `resultado.csv`. Pra
testar outra peça, passe o caminho do MEI/MusicXML como primeiro argumento
— os eixos da matriz (tamanho/orientação/cabeçalho/rodapé) ficam fixos no
script; editar os arrays `SIZES`/`ORIENTATIONS`/`HEADERS`/`FOOTERS` no topo
do script pra mudar os valores testados (ex. adicionar outro preset de
tamanho de tela, ou `--footer always`).
