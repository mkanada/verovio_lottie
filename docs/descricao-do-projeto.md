# verovio_lottie — Descrição do Projeto

## Visão geral

O `verovio_lottie` é um fork do [Verovio](https://www.verovio.org/) (motor de
gravação musical em C++, que renderiza MEI/MusicXML para SVG) com o objetivo
de adicionar uma nova capacidade de exportação: gerar arquivos **dotLottie**
(`.lottie`) além dos formatos já suportados.

O resultado deve permitir que uma partitura renderizada pelo Verovio seja
aberta em qualquer visualizador de Lottie, com dois comportamentos animados
controlados pela aplicação hospedeira (host):

1. **Destaque de notas** — cada nota pode ser "acesa" e apagada
   individualmente, sob comando do host.
2. **Virada de página** — a transição entre páginas da partitura é uma
   animação, também disparada pelo host, no estilo usado pelo Synthesia.

Este projeto é a base de um projeto maior de e-learning musical chamado
**zywny**, que consumirá os arquivos `.lottie` gerados aqui, cruzando-os com o
`timemap` que o Verovio já produz (lista de notas ativas a cada instante) para
sincronizar a partitura animada com áudio/prática do aluno.

## Motivação

Hoje o Verovio já resolve muito bem o layout e a renderização estática da
partitura (SVG). O que falta é uma camada de **interatividade temporal**: like
destacar o que está tocando agora e conduzir o olhar do estudante pela
página, sem precisar reimplementar o motor de layout musical em outra
tecnologia. Gerar Lottie diretamente a partir das estruturas internas do
Verovio (em vez de pós-processar o SVG externamente) mantém uma única fonte
de verdade para o layout musical.

## Escopo e visão de longo prazo

- **Meta de longo prazo:** tudo o que o Verovio hoje renderiza em SVG deve,
  eventualmente, também ser visível/equivalente no `.lottie` gerado. Não se
  sabe ainda quantas iterações isso vai exigir — o desenvolvimento será
  incremental.
- **Critério de "certo ou errado":** comparação **visual**. O Lottie
  correspondente a uma partitura deve ser visualmente idêntico ao SVG gerado
  pelo mesmo input. A validação será automatizada via renderização de ambos
  para PNG e comparação de imagem (ver seção de Validação).
- **MVP (primeira fase):**
  - Exportador novo, nativo, dentro do próprio código-fonte do Verovio.
  - Geometria/visual do Lottie equivalente ao SVG para um caso simples
    (uma pauta, poucas notas, monofonia) antes de expandir para partituras
    complexas (múltiplos instrumentos/vozes).
  - Destaque de nota por **mudança de cor com fade** (a nota assume uma cor
    e depois retorna gradualmente ao preto).
  - Virada de página com o efeito descrito abaixo.
- **Fora do MVP (planejado para depois):** cursores ou caixas
  delimitadoras (bounding boxes) que acompanham as notas visualmente, além do
  simples destaque de cor. Também fica em aberto tornar o estilo de destaque
  totalmente configurável.

## Estrutura do repositório

O repositório `verovio_lottie` não é apenas o fork do Verovio — ele também
abriga as ferramentas e a documentação criadas especificamente para este
projeto. Por isso a raiz do repositório é dividida assim:

- **`verovio/`** — o fork do Verovio propriamente dito: todo o código-fonte
  vendorizado (C++, bindings, fontes, dados) mais o novo exportador
  dotLottie que será desenvolvido dentro dele. É o único diretório que
  espelha a estrutura do projeto Verovio original.
- **`docs/`** — documentação do processo e das decisões deste projeto
  (este documento, decisões de arquitetura, notas de progresso).
- **`compare/`** — ferramenta de linha de comando (Rust) para comparação
  visual SVG vs. dotLottie: renderiza o SVG do Verovio para PNG (via
  `resvg`), renderiza um frame de um Lottie/dotLottie para PNG (via
  `dotlottie-rs`, o runtime oficial da LottieFiles) e gera uma imagem de
  diferença pixel a pixel entre os dois. Ver `compare/README.md` para uso e
  detalhes/pegadinhas da implementação.
- Outros diretórios a criar conforme o projeto avança, na raiz (fora de
  `verovio/`): qualquer utilitário adicional de suporte ao desenvolvimento
  que não faça parte do Verovio em si.

Essa separação existe para deixar claro o que é código vendorizado/fork
(dentro de `verovio/`, sujeito às convenções do próprio Verovio) e o que é
tooling e documentação próprios deste projeto (na raiz).

## Base técnica

- Ponto de partida: código-fonte oficial do Verovio, última versão da série
  **6.3.x** (tag `version-6.3.0`), baixado diretamente (sem manter vínculo de
  submodule/subtree com o upstream) e vendorizado em `verovio/`.
- O `verovio_lottie` é um **repositório git completamente independente** do
  repositório oficial do Verovio — um fork "solto", não uma cópia rastreada
  via submódulo. Isso significa que atualizações futuras do Verovio upstream
  precisarão ser incorporadas manualmente, se necessário.
- A exportação para dotLottie será implementada como um **exportador nativo
  em C++ dentro do código do Verovio** (ou seja, dentro de `verovio/src` e
  `verovio/include`), seguindo o mesmo padrão arquitetural dos exportadores
  já existentes (como o exportador SVG). Não haverá uma camada externa/
  pós-processamento em outra linguagem convertendo SVG em Lottie.
- Espera-se expor essa exportação pela mesma interface de linha de comando
  já usada pelos outros formatos do Verovio (ex.: um novo valor de formato
  de saída, análogo a `--to svg`), mas o nome exato da flag e a estrutura de
  arquivos gerados (manifest, assets) ainda serão definidos durante a
  implementação.

## Destaque de notas (interatividade)

- Cada nota, ao ser exportada, já carrega um `xml:id` (herdado do MEI), que o
  Verovio também usa em outras saídas, como o `timemap`. Esse mesmo `xml:id`
  será usado como **identificador do estado/evento correspondente à nota
  dentro do dotLottie**, garantindo que o projeto zywny consiga cruzar
  diretamente o `timemap` (que já fala em `xml:id`) com os disparos de
  animação no Lottie, sem precisar de uma tabela de tradução de IDs.
- A interatividade usará a **State Machine do dotLottie v2** (não os simples
  *markers* do Lottie clássico), pois oferece um modelo mais rico de estados,
  entradas (inputs) e listeners nomeados.
- **Requisito de topologia:** deve ser possível transicionar para o estado de
  qualquer nota diretamente, disparando o evento correspondente ao seu
  `xml:id`, **sem precisar passar por estados intermediários**. Ou seja, a
  máquina de estados não pode ser uma cadeia sequencial nota-a-nota; o
  destaque de qualquer nota tem que ser endereçável diretamente a qualquer
  momento (topologia em estrela / todos os estados alcançáveis diretamente a
  partir de qualquer outro, ou a partir de um estado-base).
- O disparo do destaque (quando e qual nota acender) é responsabilidade do
  **host** (a aplicação que incorpora o player de Lottie). O Verovio não
  precisa embutir tempo absoluto/andamento musical dentro do `.lottie` — ele
  só precisa expor os "ganchos" (estados/eventos por `xml:id`) que o host vai
  disparar.
- A duração e a curva do fade (cor → preto) ficam definidas dentro da própria
  animação Lottie de cada estado.

## Virada de página

- Referência visual: o efeito usado pelo **Synthesia**. Conforme o
  acompanhamento se aproxima do fim do último compasso que o estudante
  precisa seguir na página atual, a próxima página começa a aparecer
  parcialmente (um "espreitar" antecipado), até que esse compasso termine.
  Nesse momento, a transição se completa e a página nova cobre totalmente o
  final da página anterior.
- Assim como o destaque de notas, a virada de página é **disparada pelo
  host** (um evento de "próxima página"), mas a **velocidade/curva da
  transição em si é controlada pela animação Lottie**, não pelo host.
- **Decisão estrutural importante:** cada **música inteira** corresponde a
  um único arquivo/composição Lottie — não um `.lottie` por página. Cada
  página da partitura é representada como um **layer** dentro dessa única
  composição. O host não troca de arquivo `.lottie` ao virar página; ele
  dispara eventos dentro da mesma composição.
  - Consideração conhecida em aberto: isso significa que músicas longas
    (muitas páginas) resultam em composições com muitos layers dentro de um
    único arquivo. O impacto disso em tamanho de arquivo e desempenho de
    renderização ainda não foi avaliado e deve ser investigado conforme o
    projeto avança para partituras maiores.
- O número de páginas continua sendo determinado pelo próprio algoritmo de
  layout do Verovio, a partir das dimensões de página fornecidas — isso é
  ortogonal à lógica de animação da virada.

## Estratégia de validação

- Critério de aceite visual: renderizar a mesma partitura via SVG (caminho
  já existente do Verovio) e via `.lottie` (novo exportador), extrair um PNG
  de cada resultado e comparar as imagens.
- ✅ Ferramenta escolhida e implementada em `compare/`: `resvg` para SVG→PNG e
  `dotlottie-rs` (runtime oficial da LottieFiles, com renderer de software
  via ThorVG) para Lottie/dotLottie→PNG. A comparação em si (`compare diff`)
  gera uma imagem de diferença (fundo esmaecido + pixels divergentes em
  vermelho) mais estatísticas no terminal — a decisão de "passou/falhou"
  continua sendo visual/manual, sem limiar automático definido ainda.

## Contexto do projeto maior: zywny

- O `verovio_lottie` existe para servir de base a um projeto de e-learning
  musical chamado **zywny**.
- O zywny usará o `.lottie` gerado aqui junto com o `timemap` (já produzido
  pelo Verovio, listando notas ativas por instante de tempo) para decidir,
  em tempo real, quais eventos de destaque de nota e de virada de página
  disparar no player de Lottie — por isso a exigência de que os
  identificadores usados no Lottie sejam os mesmos `xml:id` do `timemap`.

## Questões técnicas em aberto (a decidir durante a implementação)

Estes pontos foram deliberadamente deixados em aberto na fase de definição
do projeto e devem ser resolvidos/pesquisados durante o desenvolvimento:

1. ~~Ferramenta exata de renderização Lottie → PNG usada nos testes de
   comparação visual.~~ Resolvido: `dotlottie-rs` (ver `compare/README.md`).
2. ~~Desenho detalhado do grafo da State Machine do dotLottie (estados,
   inputs, listeners) que satisfaça o requisito de "acesso direto a qualquer
   nota".~~ Resolvido: topologia em estrela via `GlobalState`+`PlaybackState`
   por segmento, agrupamento M2 por instante do timemap, slots M3 por nota
   (ver `docs/plano/C01-writer-state-machine.md`,
   `docs/plano/C02-notas-animadas.md`, `docs/plano/C03-slots-interativos.md`).
3. ~~Como organizar/posicionar os layers de página dentro de uma única
   composição (ex.: disposição horizontal tipo "trilha de filme") e como o
   host lida com o viewport/recorte visível, especialmente para peças
   longas com muitas páginas.~~ Resolvido: trilha horizontal + camada-câmera
   nula (`ty:3`) parent de todas as páginas, recorte pelo próprio viewport
   da composição (`w`/`h` = tamanho de uma página) — ver
   `docs/plano/C04-paginas-virada.md`. Como o host lê a posição de câmera de
   uma instância `Player` e composita na renderização visível de outra
   continua em aberto (ver risco 1 em `docs/plano/README.md`).
4. Nome da flag de CLI e estrutura de arquivos (manifest, assets) do novo
   formato de exportação.
5. Estratégia de configurabilidade futura do estilo de destaque (cursor,
   bounding boxes) mencionada como fora do escopo do MVP.

## Roadmap sugerido (fases)

1. ✅ Baixar/vendorizar o código-fonte do Verovio 6.3.0 em `verovio/` e
   inicializar o repositório git independente do `verovio_lottie`.
1.5. ✅ Construir a ferramenta de comparação visual (`compare/`), validada
   contra fixtures de exemplo (SVG real do Verovio + arquivos `.lottie`/
   `.json` de exemplo) — falta apenas o exportador real do Verovio para
   fechar o ciclo completo.
2. Implementar um exportador dotLottie mínimo, capaz de reproduzir
   visualmente uma pauta simples e monofônica, validado por comparação
   PNG contra o SVG equivalente.
3. Adicionar o destaque de nota (fade de cor) via State Machine v2,
   endereçável por `xml:id`, com acesso direto a qualquer nota.
4. Adicionar a animação de virada de página (efeito Synthesia) como parte
   da mesma composição, disparável pelo host.
5. Expandir a cobertura de elementos suportados até alcançar paridade
   visual completa com a saída SVG existente do Verovio, incluindo
   partituras com múltiplos instrumentos/vozes.
6. (Fora deste projeto) Integração com o zywny, usando `timemap` +
   eventos de `xml:id` para dirigir a reprodução animada em tempo real.
