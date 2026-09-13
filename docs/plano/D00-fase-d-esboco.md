# D00 — Fase D (paridade visual completa): esboço a detalhar

**Status:** A13 e D-TEXTO concluídos. Este esboço foi reescrito nos passos
executáveis [D01](D01-texto-comum.md)…[D06](D06-tamanho-do-arquivo.md), no
formato dos passos A/C, e a tabela do `README.md` do plano já reflete isso.
Este arquivo fica só como registro histórico do levantamento original.

D01-D06 verificados nesta sessão contra o estado real do código vendorizado
(`verovio/`) e do corpus (`corpus/`), não só por leitura do esboço abaixo —
em particular: opacidade/tracejado/visibilidade (D05) e rotação (D04) já têm
os campos correspondentes implementados na IR e no writer desde A06/A02, mas
nunca foram exercitados visualmente por nenhum critério de aceite anterior;
`DrawSvgShape`/`DrawGraphicUri`/`RotateGraphic` (D02-D04) não têm nenhuma
cobertura no corpus atual (`corpus/mei`, `corpus/musicxml`), então dependem
de MEIs mínimos criados em `compare/out/`.

## Itens já conhecidos

- **D01 — Texto comum** conforme D-TEXTO (substitui o contador de A10).
- **D02 — `DrawSvgShape`** (1 chamada no `View`; SVG embutido): reaproveitar o
  parser de A08 para `path` e acrescentar outros elementos se o corpus exigir.
- **D03 — `DrawGraphicUri`** (1 chamada; imagens): exigiria assets de imagem no
  pacote — confirmar com o usuário se vale a pena.
- **D04 — `RotateGraphic`** (2 chamadas): confirmar sinal e pivô da rotação
  contra o SVG.
- **D05 — Casos de borda de estilo**: opacidades, tracejados, `visibility`,
  tamanho de nota de cue (usar peças do corpus que os contenham ou MEIs mínimos
  criados em `compare/out/`).
- **D06 — Tamanho do arquivo**: se o relatório indicar arquivos grandes,
  reutilizar glifos repetidos (ex.: precomp por glifo em `assets`), medindo antes
  e depois.

## Fora de escopo por ora

Fac-símile (`Doc::IsFacs`), saída em mm (`mmOutput`) e opções específicas do SVG
(`svgBoundingBoxes`, `svgHtml5`, CSS adicional).
