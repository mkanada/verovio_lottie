# D00 — Fase D (paridade visual completa): esboço a detalhar

**Status:** bloqueado até A13 (relatório de paridade) e D-TEXTO (memorando B03).

Antes de executar, transformar cada item abaixo — e cada categoria de
`docs/plano/relatorio-paridade.md` — num passo `D01…Dn`, no formato dos passos A,
e atualizar a tabela do README do plano.

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
