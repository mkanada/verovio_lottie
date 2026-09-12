# C00 — Fase C (animações): esboço a detalhar

**Status:** bloqueado até D-DESTAQUE e D-LAYOUT-PAGINAS (memorando B02).

Este arquivo **não** é executável como está. Depois da decisão do usuário, o
primeiro trabalho da fase C é reescrever este esboço em arquivos `C01…Cn` no
mesmo formato dos passos A (ler antes / arquivos / o que fazer / fora de escopo /
critérios de aceite) e atualizar a tabela do README do plano.

## Esboço provisório (ajustar ao mecanismo escolhido)

- **C01 — Writer de state machine**: gerar `s/<id>.json` e incluir
  `stateMachines` no `manifest.json` (formato na "Referência rápida" do README).
- **C02 — Propriedades animadas das notas**: para cada grupo com `id` de nota
  (`className` `note`; avaliar `chord`), gerar as cores keyframed do fade. MVP:
  cor fixa com retorno gradual ao preto; cor e duração como constantes. O `id`
  já está na IR desde A02.
- **C03 — Eventos = `xml:id`**: validar unicidade e caracteres aceitos em nomes de
  input/estado; se houver restrição, definir e documentar a codificação (o host
  precisa derivar o nome a partir do timemap).
- **C04 — Páginas e virada estilo Synthesia** (segundo D-LAYOUT-PAGINAS),
  disparada por evento do host, com a duração da transição dentro do Lottie.
  Substitui a timeline provisória "página k+1 no frame k" de A03; atualizar
  `compare-page.sh` e `compare-corpus.sh`.
- **C05 — Host simulado com timemap**: `verovio -t timemap` (notas `on`/`off` por
  instante) gera um roteiro para `compare sm-render` (B01); salvar frames em
  instantes-chave para validação visual.
- **C06 — Opções de cor/duração** (só se o usuário pedir): registrar como as
  opções `svg*` (`verovio/src/options.cpp` L1177-L1189).
