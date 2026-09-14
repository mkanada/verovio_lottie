# verovio_lottie

Fork do [Verovio](https://www.verovio.org/) (motor de gravação musical em
C++, MEI/MusicXML → SVG) que adiciona um **exportador nativo de arquivos
dotLottie** (`.lottie`), para que uma partitura renderizada pelo Verovio
possa ser vista em qualquer visualizador de Lottie com:

- **Destaque de notas** individual (mudança de cor com fade), disparado por
  um host externo.
- **Virada de página** animada (estilo Synthesia), também disparada pelo
  host.

É a base de um projeto maior de e-learning musical, o **zywny**, que consome
esses `.lottie` junto com o `timemap` que o Verovio já produz.

Descrição completa do projeto (motivação, decisões arquiteturais, escopo):
[`docs/descricao-do-projeto.md`](docs/descricao-do-projeto.md) e
[`CLAUDE.md`](CLAUDE.md). O histórico passo a passo da implementação está em
[`docs/plano/`](docs/plano/README.md).

## Estrutura do repositório

- **`verovio/`** — o fork do Verovio vendorizado (tag `version-6.3.0`), com
  sua própria estrutura interna (`src/`, `include/`, `tools/`, `data/` etc.
  e seu próprio `README.md`/`.gitignore`). É onde o exportador dotLottie
  está implementado (`src/lottie*.cpp`, `include/vrv/lottie*.h`), seguindo
  as convenções do restante do Verovio.
- **`compare/`** — ferramenta de linha de comando em Rust para comparação
  visual SVG vs. dotLottie (`resvg` para SVG→PNG, `dotlottie-rs` para
  Lottie/dotLottie→PNG, diff pixel a pixel). Ver
  [`compare/README.md`](compare/README.md).
- **`corpus/`** — partituras de domínio público (MEI/MusicXML) usadas como
  material de teste, e os `.lottie` gerados a partir delas. Ver
  [`corpus/README.md`](corpus/README.md).
- **`docs/`** — documentação do processo e das decisões deste projeto.
- Histórico git próprio e independente do upstream do Verovio (sem
  submodule/subtree).

## Build

```sh
cd verovio/tools
cmake ../cmake
make -j4
```

Gera o binário `verovio/tools/verovio`. Sempre que um `.cpp` novo for
adicionado em `verovio/src/`, rode `cmake ../cmake` de novo (o CMake coleta
os fontes por glob no momento da configuração).

O binário precisa do diretório de recursos do Verovio para carregar a fonte
musical:

```sh
verovio/tools/verovio --resource-path verovio/data ...
```

## Uso

A interface de linha de comando segue o padrão já existente do Verovio
(`-t`/`--output-to <formato>`), com três novos valores de formato de saída
além dos já existentes (`svg`, `midi`, `timemap` etc.):

```sh
verovio --resource-path verovio/data -t dotlottie -o saida.lottie entrada.mei
```

| `--output-to`/`-t` | Saída | Descrição |
| --- | --- | --- |
| `dotlottie` | `.lottie` (pacote zip) | **Formato de produção.** Toda a partitura (todas as páginas) numa única composição `score`, com destaque de nota automático (`sm_highlight`) e, se houver mais de uma página, virada de página (`sm_page`). É o análogo dotLottie do `-t svg --all-pages`. |
| `dotlottie-highlight` | `.lottie` (pacote zip) | Uma única página (`--page N`), com destaque de nota automático (`sm_highlight`), sem virada de página. Útil para depurar/validar o destaque isoladamente antes de montar a peça inteira. |
| `lottie` | `.json` por página | JSON puro do Lottie clássico (sem pacote dotLottie, sem state machine, sem fontes embutidas). Ferramenta de depuração interna usada durante o desenvolvimento do exportador (ver `docs/plano/A04-toolkit-e-cli-lottie-json.md`) — não é o formato recomendado para consumo pelo zywny. |

`-a`/`--all-pages`, `-p`/`--page N` e `-o`/`--outfile` funcionam como nos
demais formatos. `dotlottie`/`dotlottie-highlight` são pacotes binários e
não podem ser escritos em stdout (`-o -`).

(O Verovio deriva o nome longo de cada opção a partir da chave interna em
camelCase, inserindo um `-` antes de cada maiúscula — por isso
`lottieHighlightColor` vira `--lottie-highlight-color`, `outputTo` vira
`--output-to` etc. Use `verovio -h` para conferir a lista exata.)

### Opções específicas do exportador dotLottie

Essas opções controlam a animação embutida no `.lottie` (todo o *disparo*
continua vindo do host — ver `docs/descricao-do-projeto.md`). Animação
fixada em 30fps.

| Opção | Padrão | Faixa | Descrição |
| --- | --- | --- | --- |
| `--lottie-highlight-color` | `E53935` | 6 dígitos hex, sem `#` | Cor usada para "acender" uma nota (ou grupo M2, notas simultâneas do mesmo instante do timemap) no destaque; o fade de volta é sempre para a cor própria resolvida da nota (normalmente preto). |
| `--lottie-highlight-duration` | `20` | `1`–`300` frames | Duração do fade de destaque de nota. |
| `--lottie-page-peek-duration` | `15` | `1`–`300` frames | Duração da fase de "espreitar" da virada de página (a câmera insinua a próxima página). |
| `--lottie-page-cover-duration` | `20` | `1`–`300` frames | Duração da fase de "cobrir" da virada de página (a câmera completa o movimento até a próxima página). |
| `--lottie-page-peek-fraction` | `0.08` | `0.0`–`1.0` | Fração da distância até a próxima página que a câmera percorre durante o "espreitar", antes de pausar e "cobrir" o resto. Valores acima de ~`0.15` começam a deslizar a página atual para fora da vista em vez de só insinuar a próxima na borda. |

Exemplo:

```sh
verovio --resource-path verovio/data -t dotlottie \
  --lottie-highlight-color 2E7D32 --lottie-highlight-duration 30 \
  --lottie-page-peek-fraction 0.12 \
  -o saida.lottie entrada.mei
```

Use `verovio -h` (ou `--help`) para a lista completa de opções do Verovio;
as opções acima aparecem no grupo geral de opções, junto com `outputTo`,
`page`, `allPages` etc.

## Validação

O critério de correção é **visual**: o `.lottie` gerado deve renderizar
igual ao SVG equivalente. A ferramenta `compare/` faz esse diff (SVG→PNG via
`resvg`, Lottie→PNG via `dotlottie-rs`, diff pixel a pixel). Ver
[`compare/README.md`](compare/README.md).
