# corpus/lottie

Pacotes `.lottie` gerados a partir de cada partitura em `corpus/` (ver
`corpus/README.md`), um por peça inteira (todas as páginas numa única
composição, com `sm_highlight` e `sm_page` — formato final decidido em
C04/`docs/plano/C04-paginas-virada.md`).

Gerados com:

```sh
verovio/tools/verovio -t dotlottie -x 42 -o <prefixo> \
  --resource-path verovio/data <arquivo-do-corpus>
```

`-x 42` fixa a seed dos `xml:id` gerados pelo Verovio (ver
`docs/plano/C05-host-simulado-timemap.md`, seção "Achado") — mesma seed usada
nos exemplos de `docs/exemplos/`, para que os `xml:id` citados ali continuem
batendo com estes pacotes caso sejam regenerados.

Regenerar todos: rode o mesmo comando para cada arquivo listado em
`corpus/README.md` (mei e musicxml), com prefixo sem pontos (o `-o` do
Verovio trunca a partir do primeiro `.` do caminho).
