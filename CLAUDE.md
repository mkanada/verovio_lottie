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

## Estado atual do repositório

Este repositório está em fase de definição — ainda **não contém** o
código-fonte do Verovio nem histórico git. Antes de qualquer implementação,
o código-fonte oficial do Verovio (série **6.3.x**) precisa ser baixado e
vendorizado aqui, com um histórico git **próprio e independente** do
upstream (não submodule/subtree).

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

## O que ainda está em aberto (não decida sozinho, pesquise/pergunte)

Ver seção "Questões técnicas em aberto" em
`docs/descricao-do-projeto.md`. Resumo:

- Ferramenta de renderização Lottie→PNG para os testes de comparação visual.
- Desenho detalhado do grafo da State Machine (estados/inputs/listeners).
- Disposição dos layers de página dentro da composição única (ex.: trilha
  horizontal) e como o host recorta o viewport visível.
- Nome da flag de CLI e estrutura de arquivos do novo formato de exportação.

## Convenções de trabalho

- Siga o estilo de código e as convenções já usadas no restante do
  Verovio (nomenclatura de classes `Io*`, organização de headers/source,
  etc.) em vez de introduzir um estilo novo isolado.
- Ao adicionar o exportador, espelhe a interface de linha de comando dos
  formatos já existentes (ex.: análogo a `--to svg`) até que haja uma razão
  concreta pra divergir.
- Priorize sempre atingir paridade visual incremental (casos simples antes
  de complexos) em vez de tentar cobrir toda a superfície do SVG de uma vez.
- Este repositório usa [graft](graft/INDEX.md) para navegação de código —
  uma vez que o código-fonte do Verovio for vendorizado, prefira `graft ask`
  / `graft grep` / `graft skeleton` a `grep`/leitura manual de arquivos, e
  rode `graft build` depois de mudanças grandes.
