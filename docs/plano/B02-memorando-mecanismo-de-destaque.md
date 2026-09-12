# B02 — Memorando: mecanismo de destaque e virada de página

**Depende de:** B01 · **Decisão:** produz D-DESTAQUE e D-LAYOUT-PAGINAS,
**tomadas pelo usuário**.

## Objetivo

Comparar mecanismos possíveis com base na evidência de B01 e nos requisitos,
recomendar um e parar para a decisão do usuário.

## Ler antes (só isto)

- `CLAUDE.md` — decisões já tomadas.
- `docs/descricao-do-projeto.md` — seções "Destaque de notas" e "Virada de página".
- `docs/plano/spikes/B01-resultado.md`.

## Candidatos (acrescente outros que B01 revelar)

- **M1** — um segmento (marker) por nota numa timeline única; um estado por nota.
- **M2** — um segmento por **instante** do timemap (notas que começam juntas); o
  evento continua sendo um `xml:id` (ex.: o da primeira nota do grupo), com
  mapeamento publicado para o host.
- **M3** — cores por slots/temas (`SetTheme`, slots de cor), sem depender do playhead.
- **M4** — várias animações no mesmo pacote, compostas pelo host (várias instâncias do player).
- **M5** — timeline que codifica a sequência musical; o host dirige o playhead
  (`SetFrame`/`SetProgress`) e o Lottie controla a duração do fade em frames.

## Critérios

Tabela candidato × critério:

- endereçamento por `xml:id`;
- acesso direto a qualquer nota;
- notas simultâneas;
- fades sobrepostos;
- fade controlado pelo Lottie;
- convivência com a virada de página;
- tamanho e tempo de carga (E5 de B01);
- compatibilidade com players dotLottie comuns (web, mobile);
- complexidade no exportador e no host (zywny).

## Entregável

`docs/plano/decisoes/B02-mecanismo-destaque.md` com:

- a tabela e a recomendação;
- **perguntas explícitas ao usuário**, incluindo qualquer requisito que precise
  ser flexibilizado (por exemplo, se nenhum mecanismo atende ao mesmo tempo
  "notas simultâneas + fade controlado pelo Lottie + evento por `xml:id`");
- uma proposta para D-LAYOUT-PAGINAS compatível com o mecanismo recomendado.

## Depois da resposta do usuário

Registrar a decisão no memorando e na seção de decisões do `CLAUDE.md`, atualizar
a tabela de decisões do README do plano e seguir para C00.

## Notas de execução

_(preencher ao executar)_
