# CLAUDE.md

Guia para trabalhar no `verovio_lottie`. Contexto completo do projeto está em
[`docs/descricao-do-projeto.md`](docs/descricao-do-projeto.md) — leia esse
arquivo antes de propor mudanças estruturais; este `CLAUDE.md` resume só o
essencial pra decisões do dia a dia.

## O que é este projeto

Fork do [Verovio](https://www.verovio.org/) que adiciona um exportador nativo
de arquivos **dotLottie** (`.lottie`), pra que uma partitura renderizada pelo
Verovio possa ser vista em um visualizador de Lottie com:

- **Destaque de notas** individual, disparado por um host externo.
- **Virada de página** animada (estilo Synthesia), também disparada pelo
  host.

É a base de um projeto maior de e-learning musical, o **zywny**, que vai
consumir esses `.lottie` junto com o `timemap` do Verovio.

## Estrutura do repositório

- **`verovio/`** — o fork do Verovio vendorizado (tag `version-6.3.0`),
  com sua própria estrutura interna (`src/`, `include/`, `bindings/`,
  `fonts/`, `tools/`, etc., e seu próprio `.gitignore`). É onde o novo
  exportador dotLottie será implementado, seguindo as convenções do
  Verovio.
- **`compare/`** — crate Rust com a ferramenta de comparação visual SVG vs.
  dotLottie (`resvg` para SVG→PNG, `dotlottie-rs` para Lottie/dotLottie→PNG,
  diff pixel a pixel). Ver `compare/README.md` antes de mexer nela — tem uma
  pegadinha documentada sobre `set_frame`/`render` do dotlottie-rs.
- **Raiz do repositório** — tudo que é deste projeto e não do Verovio em
  si: `docs/`, `compare/`, e outros utilitários de suporte conforme o
  projeto avançar. Não misture esse tooling dentro de `verovio/`.
- Histórico git **próprio e independente** do upstream do Verovio (sem
  submodule/subtree) — atualizações do Verovio original precisam ser
  incorporadas manualmente se necessário.

## Decisões arquiteturais já tomadas

Trate estas decisões como fixas — não as reabra sem confirmar com o usuário:

- **Exportador nativo em C++ dentro do Verovio**, no mesmo padrão dos
  exportadores existentes (ex.: exportador SVG). Nada de pipeline externo
  convertendo SVG em Lottie.
- **Critério de correção é visual**: o `.lottie` gerado deve renderizar
  visualmente igual ao SVG equivalente. Validação por PNG diff (SVG vs.
  Lottie renderizado), não por comparação estrutural de JSON.
- **Identificadores = `xml:id`**: todo estado/evento de nota no dotLottie
  usa o mesmo `xml:id` que o Verovio já atribui (herdado do MEI) e que
  aparece no `timemap`. Não invente um esquema de IDs paralelo.
- **Interatividade via State Machine do dotLottie v2** (não os `markers` do
  Lottie clássico).
- **Topologia em estrela obrigatória**: qualquer nota deve ser destacável
  diretamente pelo evento do seu `xml:id`, sem passar por estados
  intermediários. Nunca modele isso como uma cadeia sequencial nota-a-nota.
- **Todo o disparo de animação (nota e virada de página) vem do host** — o
  Verovio não embute tempo/andamento absoluto. A duração/curva de cada
  animação (fade de cor, transição de página) fica dentro do próprio Lottie.
- **Um `.lottie` por música inteira**, não por página. Cada página é um
  *layer* dentro de uma única composição.
- **MVP do destaque de nota é mudança de cor com fade** de volta ao preto.
  Cursor/bounding boxes seguindo notas é explicitamente **fora de escopo**
  por enquanto — não implemente isso preventivamente.
- **Mecanismo de destaque e de virada de página (decidido em B02, ver
  `docs/plano/decisoes/B02-mecanismo-destaque.md` — a decisão passou por
  uma correção do usuário na própria sessão; o texto abaixo é a versão
  final):**
  - Destaque de nota usa **dois mecanismos diferentes por modo de uso,
    mutuamente exclusivos** (nunca os dois controlando a mesma nota ao
    mesmo tempo):
    - **Modo automático (playback):** state machine em estrela (padrão já
      validado: `PlaybackState`+`segment`+`GlobalState`), com notas do
      **mesmo instante do timemap** agrupadas num único estado/segmento,
      endereçável por qualquer `xml:id` do grupo. Fade com curva autorada
      no Lottie. Limitação aceita: vozes com onsets *diferentes* que se
      sobrepõem no tempo ainda se cancelam entre si.
    - **Modo interativo (aluno tocando ao vivo):** um slot de cor
      (`set_color_slot`) por nota, nomeado pelo `xml:id`, sem pool nem
      limite de quantas notas podem estar acesas ao mesmo tempo. Fade
      **não** precisa ser autorado no Lottie nesse modo — o host liga/
      desliga a cor diretamente (requisito relaxado conscientemente só
      para este modo, por não haver como testar slots animados sem
      reintroduzir a dependência de playhead único).
    - Trocar de modo exige um protocolo de handoff (resetar a state
      machine do automático / limpar os slots do interativo) — **ainda
      não desenhado**, é escopo de C01/C03.
  - Virada de página usa um **engine totalmente separado** do mecanismo de
    destaque automático (nunca a mesma state machine/instância — B01
    provou que dividir engine faz a virada cancelar um fade em
    andamento). Mecanismo: **dois eventos discretos por fronteira de
    página**, cada um endereçável diretamente (mesma topologia em estrela
    das notas) — (A) playhead entra no último compasso da página atual →
    dispara animação pré-autorada de "espreitar" (revelação parcial da
    próxima página, para no frame final); (B) playhead entra no primeiro
    compasso da próxima página → dispara animação pré-autorada que
    cobre/remove o que restava da página anterior, completando a
    transição. Ambas são segmentos comuns (`PlaybackState`+`segment`), sem
    precisar da semântica de `Tweened`.
  - Risco aberto, aceito conscientemente sem spike dedicado (decisão do
    usuário): não está confirmado se o(s) runtime(s) de player que o
    zywny vai usar suportam rodar **2 instâncias simultâneas** (o engine
    principal `score`, que já serve pro modo automático e pro interativo
    via slots, + o engine separado de página) e compositar a posição de
    câmera de uma na renderização visível da outra — primeiro ponto de
    atenção prático ao implementar C00.

## O que ainda está em aberto (não decida sozinho, pesquise/pergunte)

Ver seção "Questões técnicas em aberto" em
`docs/descricao-do-projeto.md`. Resumo:

- Grafo *detalhado* da State Machine (nomes exatos de estados/inputs/
  listeners) — o mecanismo já foi decidido em B02 (ver acima), falta só a
  autoria concreta, prevista para C01.
- Curva/timing exatos da animação de "espreitar" e de "cobrir" na virada
  de página, e como as coordenadas do overlay de destaque acompanham a
  página/câmera corrente — a topologia já foi decidida em B02, falta a
  autoria concreta (Fase C).
- Nome da flag de CLI e estrutura de arquivos do novo formato de exportação.

## Notas operacionais

- Ao rodar `git push`/`git pull` neste ambiente, o git emite o aviso
  `git: 'credential-manager' não é um comando git`. É inofensivo — só um
  `credential.helper` mal configurado no git global deste ambiente,
  não impede push/pull nem indica problema de autenticação real.

## Convenções de trabalho

- Todo código do exportador dotLottie vive dentro de `verovio/` (em
  `verovio/src` e `verovio/include`), seguindo o estilo de código e as
  convenções já usadas no restante do Verovio (nomenclatura de classes
  `Io*`, organização de headers/source, etc.) em vez de introduzir um
  estilo novo isolado.
- Ao adicionar o exportador, espelhe a interface de linha de comando dos
  formatos já existentes (ex.: análogo a `--to svg`) até que haja uma razão
  concreta pra divergir.
- Priorize sempre atingir paridade visual incremental (casos simples antes
  de complexos) em vez de tentar cobrir toda a superfície do SVG de uma vez.
- Este repositório usa [graft](graft/INDEX.md) para navegação de código —
  uma vez que o código-fonte do Verovio for vendorizado, prefira `graft ask`
  / `graft grep` / `graft skeleton` a `grep`/leitura manual de arquivos, e
  rode `graft build` depois de mudanças grandes.
