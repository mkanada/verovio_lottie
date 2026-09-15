# D01-5 — Cobrir Bold Italic (números de tempo/compasso)

**Depende de:** D01 (texto comum), B03 (decisão original de escopo) ·
**Decisão necessária:** reabrir o escopo de B03 ("Regular + Italic + Bold,
Bold Italic não coberto") — feito a pedido explícito do usuário ao relatar
o sintoma.

## Problema

Usuário reportou, olhando Clair de Lune: "os números dos compassos, na
versão SVG, estão em itálico, e na versão lottie, não". Os elementos em
questão não são `mNum` (número de compasso de verdade — esses já saíam
italicizados corretamente, confirmado por comparação lado a lado) — são as
marcas numéricas de andamento/rubato (classe `tempo`: "49", "50", "51"...
"57", "60"...) que aparecem acima de cada compasso em passagens de tempo
flutuante. Já tinham sido identificadas em D01 (ver "Notas de execução",
item 2) como bold+italic simultâneo, com o aviso "bold italic... falling
back to Bold" — mas D01 tratou isso como uma limitação aceita (herdada de
B03), não um bug a corrigir.

Confirmado visualmente (`corpus/musicxml/Clair_de_Lune__Debussy.mxl`, p.1,
região do "49"): SVG mostra o número claramente inclinado (itálico) e em
negrito; Lottie mostra o mesmo número em negrito, mas ereto.

## Causa

`SelectFontStyleName(bold, italic)` em `verovio/src/lottiewriter.cpp`
tratava `bold && italic` como não suportado e caía para `"Bold"` — decisão
explícita de B03 ("MVP cobre só Regular/Italic/Bold, Bold Italic fica pra
depois"), não um bug de implementação.

## Correção

- Vendorizada a quarta face, `verovio/data/text/LiberationSerif-BoldItalic.ttf`
  — bytes idênticos (`sha256sum`) ao pacote `fonts-liberation` já instalado
  neste ambiente (`/usr/share/fonts/truetype/liberation/`), mesma origem/
  licença SIL OFL das outras três já vendorizadas em B03/D01.
- `SelectFontStyleName` (`lottiewriter.cpp`) passa a retornar `"BoldItalic"`
  quando `bold && italic`; removido o aviso de fallback.
- `WriteFontsList` (`lottiewriter.cpp`) e `EmbedCommonTextFonts`
  (`toolkit.cpp`) passam de 3 para 4 estilos fixos (`kStyles`), sempre
  embutidos juntos — mesmo critério de B03 ("previsibilidade de tamanho" >
  "escanear quais estilos a peça usa").
- `compare/scripts/{compare-page,compare-corpus,compare-layout-matrix}.sh`
  passam a carregar também `LiberationSerif-BoldItalic.ttf` via `--font`,
  para que o PNG de referência do `compare` use a face real de Bold Italic
  em vez de um bold-itálico sintetizado pelo `resvg` a partir da face Bold
  sozinha (o `resvg`, sem uma face Bold Italic carregada, sintetiza um
  itálico artificial sobre a Bold quando o CSS pede os dois — o que
  também não é o resultado que um navegador real, com a família Liberation
  Serif completa instalada, mostraria).

## Verificação

- Warning "bold italic common text is not covered" não aparece mais no log
  de `verovio -t dotlottie` para Clair de Lune.
- `unzip -l` confirma os 4 `f/LiberationSerif-*.ttf` no pacote.
- Recorte do "49" (Clair de Lune p.1): SVG e Lottie agora pixel-a-pixel
  quase idênticos (6 px divergentes em 4800, tolerância 32 — ruído de
  antialiasing) — antes, a região inteira do "49" divergia (336/4800).
- Corpus completo (`compare-corpus.sh 32`, com D01-6 já incluído — ver esse
  documento pro efeito isolado de D01-6): ver "Notas de execução" abaixo.
- Matriz de layout (`compare-layout-matrix.sh`, Chopin Étude, que não tem
  bold+italic): sem mudança de conteúdo (só ruído — Chopin Étude não usa
  esse padrão de tempo flutuante), critério de não-regressão satisfeito.

## Fora de escopo

- Enviar a fonte Bold Italic pra algum outro consumidor do projeto além do
  exportador dotLottie (ex.: se o zywny algum dia precisar da mesma fonte
  em outro contexto, é decisão dele).
- Revisitar o custo de tamanho total (D06) — agora ~4 estilos ao invés de
  3, adicionando outros ~200-220 KB comprimidos fixos por peça (mesma
  ordem de grandeza medida em B03/D01 por estilo). Documentado aqui como
  novo dado de entrada pra D06, não decidido aqui.

## Notas de execução

Implementado exatamente como descrito acima. `cmake ../cmake && make -j4`
limpo. Ver `docs/plano/D01-6-glifo-smufl-em-texto-comum.md` pros números
consolidados do corpus (as duas correções foram medidas juntas, numa única
rodada de `compare-corpus.sh`/`compare-layout-matrix.sh`, já que as duas
foram implementadas na mesma sessão a partir do mesmo pedido do usuário).
