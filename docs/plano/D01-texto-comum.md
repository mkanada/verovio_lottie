# D01 — Texto comum (Liberation Serif embutida, mecanismo T1)

**Depende de:** A10 (modelo de chunk de texto), B03/D-TEXTO (decisão) ·
**Decisão necessária:** nenhuma nova — T1 (fonte TTF embutida + camada de
texto nativa `ty:5`), Regular+Italic+Bold, Liberation Serif, já decididos em
`docs/plano/decisoes/B03-texto.md`. Este passo só resolve os itens (a)-(e)
que B03 deixou explicitamente para cá (ver a seção final daquele memorando)
e toma decisões de MVP dentro do mecanismo já aprovado, sem reabrir a
arquitetura — mesmo padrão de C02/C04.

## Objetivo

Substituir o ramo de "fonte comum" em `LottieDeviceContext::DrawText` (hoje
só conta `m_skippedTextRuns` e avança a caneta) por uma camada de texto
nativa do Lottie (`ty:5`) por trecho de texto comum, referenciando um dos
três `.ttf` vendorizados (Regular/Italic/Bold) conforme
`font->GetStyle()`/`GetWeight()`, embutidos no pacote `.lottie` via
`fonts.list` (`origin:3`) + `f/*.ttf`. É o item que A13 identificou como
causa de ~95%+ da divergência visual medida (`docs/plano/relatorio-paridade.md`).

## Ler antes (só isto)

- `docs/plano/decisoes/B03-texto.md` inteiro — em especial a "Decisão do
  usuário" e os itens (a)-(e) de "Notas de execução (pós-decisão)", que este
  passo resolve um a um.
- `verovio/src/lottiedevicecontext.cpp` L443-503 (`DrawText` atual, o ramo
  `else` em L493-502 é o que muda), L532-566 (`StartText`/`MoveTextTo`/
  `MoveTextVerticallyTo`/`FinalizeTextChunk` — modelo de chunk de A10),
  L385-426 (`MakeGlyphShape`, para copiar o padrão de escala por
  `font->GetPointSize()`), L740-750 (`EndPage`, aviso a ajustar), L55-96
  (`ApplyFillFromBrush`/`ApplyStrokeFromPen` no namespace anônimo — como o
  fill "herda de `currentColor`" hoje).
- `verovio/include/vrv/lottiedevicecontext.h` L203-213 (estado do modelo de
  texto: `m_textPenX/Y`, `m_textAlignment`, `m_textChunkWidth`,
  `m_textChunkShapes`, `m_skippedTextRuns`).
- `verovio/include/vrv/devicecontextbase.h` L137-162 (`FontInfo::GetStyle`,
  `GetWeight`, `GetFaceName`, `GetPointSize`, `GetLetterSpacing`) — `GetStyle`
  devolve `data_FONTSTYLE` (`FONTSTYLE_NONE`/`_italic`/...), `GetWeight`
  devolve `data_FONTWEIGHT` (`FONTWEIGHT_NONE`/`_bold`/...).
- `verovio/include/vrv/lottiegeometry.h` L42-58 (`LottieShape`), L66-80
  (`LottieChild`, `LottieNode`) — a IR que este passo estende com um terceiro
  tipo de filho (texto nativo), ao lado de `group`/`shape`.
- `verovio/src/lottiewriter.cpp` L241-255 (`WriteTransformDefault`/
  `WriteTransformWithRotation` — confirma que grupos **não** carregam
  deslocamento próprio; todo vértice de shape já chega em coordenadas
  absolutas de página, "assadas" no `DrawX`/`MakeGlyphShape` — por isso uma
  camada de texto nativa não precisa herdar transform de grupo, só a mesma
  `px`/`py`/`s` que a camada da própria página já usa), L573-695
  (`WriteAnimation` — `"assets":[]` hoje sempre vazio, sem chave `"fonts"`
  nenhuma; L651-691 é o laço por página cujo `px`/`py`/`s`/`parent`/`ip`/`op`
  este passo reaproveita para as novas camadas de texto), procure
  `ResolveColor`/`ActiveHighlight` (resolução de cor herdada e de destaque em
  tempo de escrita — o mesmo padrão a replicar para texto).
- `verovio/src/filereader.cpp` L140-143 (`ZipFileWriter::AddFile`, binário-
  seguro).
- `verovio/src/toolkit.cpp` L123-144 (`Toolkit::SetResourcePath`/
  `GetResourcePath`), L1866-1943 (`RenderToDotLottieFile`), L1945-1990
  (`RenderToDotLottieHighlightFile`) — onde embutir os 3 `.ttf`.
- Fontes disponíveis no sistema para vendorizar (mesma fonte do spike de
  B03): `/usr/share/fonts/truetype/liberation/LiberationSerif-{Regular,
  Italic,Bold}.ttf` (não usar `-BoldItalic.ttf`, fora de escopo por decisão
  do usuário). Confirmado presentes neste ambiente.
- Formato de camada de texto nativa do Lottie/dotLottie v2 (`ty:5` +
  `fonts.list` com `origin:3`): **não há doc formal no repositório** — o
  spike de B03 validou isso manualmente e não foi commitado (viveu em
  `/tmp/.../scratchpad/b03-spike/`, sessão anterior). Antes de implementar,
  refazer um spike mínimo equivalente (mesma receita do memorando B03,
  seção "Spike executado (T1)") para confirmar o schema exato de
  `t.d.k[].s` (`s`/`f`/`t`/`j`/`tr`/`lh`/`fc`) contra o `dotlottie-rs`
  vendorizado — não adivinhar o schema por memória.

## Decisões de escopo tomadas aqui

- **`fonts.list`/`f/*.ttf` só nos pacotes zipados (`dotlottie`,
  `dotlottie-highlight`)**. O formato `lottie` (JSON cru de uma página, sem
  zip — CLI de depuração, A04) não tem onde embutir o `.ttf`: `fPath`
  apontaria pra um arquivo que não existe fora de um pacote dotLottie.
  `RenderToLottieAnimation()`/CLI `lottie` continuam com texto comum
  **ausente** (mesmo comportamento de hoje), já que é só uma ferramenta de
  depuração de geometria, não um entregável. Mecanicamente: a IR
  (`LottieDeviceContext`) sempre constrói os nós de texto (não sabe nem
  precisa saber qual formato de saída vai consumi-la, mesmo princípio já
  usado por `highlightGroups`/`interactiveIds`/`pageTurn`); é
  `LottieWriter::WriteAnimation` quem ganha um novo parâmetro
  `bool embedCommonText = false` — `false` preserva a saída **byte-idêntica**
  de hoje pra todo chamador que não o passa (mesmo padrão de não-regressão
  de C04), `true` (só em `RenderToDotLottieFile`/
  `RenderToDotLottieHighlightFile`) emite as camadas de texto + `fonts.list`.
- **As 3 fontes são sempre embutidas juntas quando `embedCommonText=true`**,
  mesmo que a peça não use itálico/negrito — é a leitura literal da decisão
  do usuário em B03 ("custo fixo aceito: ~600-650 KB... por peça"), evita
  ter que escanear com antecedência quais estilos aparecem, e mantém o
  tamanho do pacote previsível.
- **Aviso de contagem**: como o `LottieDeviceContext` não sabe mais se o
  texto será de fato embutido (isso agora é decisão do writer/`Toolkit`), o
  aviso "N common text run(s) not rendered" sai do `DeviceContext::EndPage`
  e passa a ser responsabilidade do `Toolkit`: ao gerar `RenderToLottieAnimation`
  (formato `lottie`, sem `embedCommonText`), contar nós de texto na IR já
  construída e emitir o mesmo `LogWarning` de hoje por página, uma vez, ali.
  `RenderToDotLottieFile`/`RenderToDotLottieHighlightFile` não emitem mais
  esse aviso (o texto é de fato renderizado nesses caminhos).
- **Combinação itálico+negrito não coberta** (mesma limitação já registrada
  em B03): quando `GetStyle()==FONTSTYLE_italic` **e**
  `GetWeight()==FONTWEIGHT_bold` ao mesmo tempo, cai para **Bold** (perde o
  itálico) — decisão local deste passo, com `LogWarning` uma vez por
  combinação vista (mesmo padrão do parser de cor CSS de A06:
  "nome desconhecido → aviso uma vez + fallback").
- **Posicionamento sem deslocamento manual de vértice**: ao contrário do
  texto SMuFL (que precisa de `FinalizeTextChunk` pra deslocar cada vértice
  conforme o alinhamento, porque um path não tem noção nativa de
  "justificado"), o texto comum usa a justificação nativa do Lottie
  (propriedade `j` da `TextDocument`: 0=left, 1=right, 2=center — mapear a
  partir do mesmo `data_HORIZONTALALIGNMENT` já usado). Por isso um trecho de
  texto comum é inserido **direto** em `DrawText` (não participa de
  `m_textChunkShapes`/`FinalizeTextChunk`); só precisa da âncora
  `(m_textPenX, m_textPenY)` no momento da chamada, igual ao que já é
  calculado hoje.
- **Cor resolvida em tempo de escrita, como já é feito pra shapes**: o
  `LottieTextRun` guarda `color = COLOR_NONE` (herdar) a menos que o brush
  corrente tenha `HasColor()`, exatamente como `ApplyFillFromBrush` já faz
  pra shapes; `LottieWriter` resolve a cor herdada da cadeia de ancestrais
  no mesmo passeio de árvore que hoje resolve `colorCss` pra shapes (não
  introduzir um segundo mecanismo de herança).
- **Hidden herda o mesmo tratamento**: se um nó `LottieNode` ancestral tem
  `hidden==true` (D05 trata o caso geral), o passeio de árvore que coleta
  camadas de texto pula esse subtree, igual ao que já acontece pra shapes.

## Arquivos

- Adicionar: `verovio/data/text/LiberationSerif-Regular.ttf`,
  `-Italic.ttf`, `-Bold.ttf` (mesma licença SIL OFL já referenciada por
  `fontTextLiberation`; incluir o arquivo de licença, ex.
  `verovio/data/text/LiberationSerif-OFL.txt`, se ainda não existir
  vendorizado em nenhum lugar do fork).
- Modificar: `verovio/include/vrv/lottiegeometry.h` (`LottieTextRun`,
  `LottieChild`), `verovio/include/vrv/lottiedevicecontext.h`/`.cpp`
  (`DrawText`, `AddTextRun`), `verovio/include/vrv/lottiewriter.h`/`.cpp`
  (`WriteAnimation` com `embedCommonText`, `WriteTextLayer`, `WriteFontsList`,
  resolução de cor pra texto), `verovio/src/toolkit.cpp`
  (`RenderToLottieAnimation` conta+avisa; `RenderToDotLottieFile`/
  `RenderToDotLottieHighlightFile` passam `embedCommonText=true` e embutem
  os 3 `.ttf` via `zip.AddFile("f/LiberationSerif-<estilo>.ttf", ...)`).

## O que fazer

1. **IR** (`lottiegeometry.h`): acrescentar

   ```cpp
   struct LottieTextRun {
       std::u32string text;
       Point origin;                              // âncora (page px), antes de qualquer alinhamento
       data_HORIZONTALALIGNMENT alignment = HORIZONTALALIGNMENT_left;
       double pointSize = 0.0;                     // mesma unidade de MakeGlyphShape (já page px)
       double letterSpacing = 0.0;
       data_FONTSTYLE style = FONTSTYLE_NONE;
       data_FONTWEIGHT weight = FONTWEIGHT_NONE;
       int color = COLOR_NONE;                     // COLOR_NONE = herdar (igual a LottieShape::fillColor)
   };
   ```

   e um terceiro caso em `LottieChild` (ao lado de `group`/`shape`):
   `std::optional<LottieTextRun> text;`. Convenção: exatamente um de
   `group`/`text` é não-nulo, senão é `shape` (mesma convenção implícita já
   usada entre `group` e `shape`).

2. **`LottieDeviceContext::DrawText`** (`lottiedevicecontext.cpp` L493-502):
   trocar o ramo `else` (fonte comum) por:
   - montar um `LottieTextRun` com `text = chars`,
     `origin = Point(m_textPenX, m_textPenY)`, `alignment = m_textAlignment`,
     `pointSize = font->GetPointSize()`, `letterSpacing = letterSpacing`,
     `style = font->GetStyle()`, `weight = font->GetWeight()`,
     `color = m_brushStack.top()->HasColor() ? m_brushStack.top()->GetColor() : COLOR_NONE`
     (mesmo brush corrente que `ApplyFillFromBrush` usaria);
   - inserir com um novo `AddTextRun(std::move(run))` (espelha `AddShape`,
     insere no `m_nodeStack.back()`);
   - manter o avanço de caneta (`GetTextExtent` + `m_textPenX`/
     `m_textChunkWidth`) exatamente como hoje, para trechos SMuFL
     subsequentes no mesmo chunk continuarem alinhados;
   - remover `++m_skippedTextRuns` (deixa de existir — ver "Decisões de
     escopo").
3. **`EndPage`**: remover o bloco do aviso de `m_skippedTextRuns` (deixa de
   fazer sentido no `DeviceContext` — ver "Decisões de escopo"); remover o
   próprio membro `m_skippedTextRuns`.
4. **`LottieWriter`**: novo parâmetro em `WriteAnimation`
   (`bool embedCommonText = false`, no fim da lista, com default — mesmo
   padrão de `pageTurn`/`peekFraction`):
   - passeio de árvore (reaproveitar/estender o passeio que já resolve
     `colorCss`/`hidden` para shapes) coleta, por página, os `LottieTextRun`
     encontrados junto com a cor resolvida (herdada se `COLOR_NONE`);
   - se `!embedCommonText`: nenhuma camada de texto é escrita (saída
     idêntica a hoje);
   - se `embedCommonText`: para cada run coletado da página `i`, emitir uma
     camada `"ty":5` **irmã** da camada de shape da página, reaproveitando
     exatamente `px`/`py`/`s`/`parent`(câmera)/`ip`/`op` já calculados pro
     `"ty":4` daquela página (mesmos valores, só um `"ind"` novo por run,
     alocado depois do maior `ind` já em uso); dentro de `ks.p`, usar
     `origin` do run (mais o mesmo `px`/`py`/escala da página, do mesmo jeito
     que as camadas de página já posicionam seu conteúdo); `t.d.k[0].s`
     carrega `s` (= `pointSize`), `f` (nome da fonte — ver passo 5), `t`
     (texto, UTF-8), `j` (0/1/2 a partir de `alignment`), `tr` (=
     `letterSpacing`), `fc` (cor resolvida, RGB 0-1 ou 0-255 conforme o
     schema confirmado no spike do "Ler antes").
   - `"fonts":{"list":[...]}` no nível raiz do JSON (hoje inexistente):
     3 entradas fixas (Regular/Italic/Bold) sempre que `embedCommonText`,
     cada uma com `fName` (a mesma string usada em `f` acima), `fFamily`
     ("Liberation Serif"), `fStyle` ("Regular"/"Italic"/"Bold"),
     `fPath: "f/LiberationSerif-<Estilo>.ttf"`, `"origin":3` (formato
     confirmado no spike do "Ler antes" e no `README.md`, seção
     "Referência rápida").
5. **Seleção de arquivo por estilo**: função pura (`lottiewriter.cpp` ou
   `lottiedevicecontext.cpp`, tanto faz — não depende de estado) que mapeia
   `(style, weight)` → um dos 3 nomes fixos, com o fallback Bold-sobre-
   Italic descrito em "Decisões de escopo".
6. **`Toolkit::RenderToLottieAnimation`**: depois de montar a IR de cada
   página, contar `LottieTextRun`s (mesmo passeio simples, sem resolver
   cor) e emitir o `LogWarning` "N common text run(s) not rendered" por
   página (substitui o que `EndPage` fazia).
7. **`Toolkit::RenderToDotLottieFile`/`RenderToDotLottieHighlightFile`**:
   - `LottieWriter::WriteAnimation(..., /*embedCommonText=*/true)`;
   - ler os 3 `.ttf` de `GetResourcePath() + "/text/LiberationSerif-<Estilo>.ttf"`
     (novo subdiretório `text/` dentro do resource path, paralelo a
     `data/text/Times*.xml` já existente) e `zip.AddFile("f/LiberationSerif-<Estilo>.ttf", bytes)`
     pros 3, sempre (mesmo se a peça não usar todos os estilos — ver
     "Decisões de escopo").

## Fora de escopo

Bold Italic (não decidido por B03; adicionado depois em
[D01-5](D01-5-bold-italico-tempo.md)), `DrawRotatedText` (sem chamadas no
`View`, já registrado em A10), qualquer configuração de CLI para
cor/tamanho/fonte de texto comum (não pedido), otimizar o tamanho do
`.ttf` embutido (ex. subsetting de glifos) — se o custo de ~600-650 KB
virar problema real, é D06 quem revisita.

## Critérios de aceite

- Compila (`cd verovio/tools && cmake ../cmake && make -j4`).
- `LottieWriter::WriteAnimation` chamado sem `embedCommonText` (como
  `RenderToLottieAnimation()`/CLI `lottie` já fazem) produz saída
  **byte-idêntica** à de antes deste passo (mesmo teste de não-regressão de
  C04: gerar antes/depois pra 2-3 peças do corpus, diff byte a byte).
- `python3 -m json.tool` valida `a/score.json` de um `dotlottie` gerado com
  `embedCommonText=true`.
- `unzip -l` no pacote confirma os 3 `f/LiberationSerif-*.ttf` presentes;
  `unzip -t` passa.
- `compare/scripts/compare-page.sh corpus/mei/Chopin_Etude_Op10_No9.mei 1`
  contra o **pacote `dotlottie`** (não mais o formato `lottie` cru — ajustar
  o script se ele hoje só sabe gerar/comparar o formato de depuração; ver
  também D-LAYOUT-PAGINAS/C04 pra como ler o frame de repouso de uma página
  dentro do pacote final): a % de divergência cai de 0,4842% (medido em
  A13, causa quase toda texto) pra um valor consistente com a categoria 2 de
  `docs/plano/relatorio-paridade.md` (ruído de antialiasing), não mais
  dominado por texto ausente. Confirmar visualmente por crop que título,
  "Allegro molto agitato.", indicações italianas e dedilhados aparecem no
  Lottie.
- Repetir em Clair de Lune p.1 (`corpus/musicxml/Clair_de_Lune__Debussy.mxl`),
  o caso com mais itálico do corpus (25 ocorrências só nessa página) — itálico
  precisa aparecer visualmente inclinado, não como Regular.
- Achar (ou criar um MEI mínimo em `compare/out/`) um caso com negrito
  (título) e confirmar visualmente o peso correto.
- `compare/scripts/compare-corpus.sh 32` rodado de novo sobre o corpus
  inteiro contra pacotes `dotlottie`: nenhum crash; CSV resultante com médias
  bem abaixo das de A13 (texto deixa de ser a causa dominante).
- Tamanho medido de 2-3 pacotes antes/depois: confirma o acréscimo fixo
  (~600-650 KB) previsto por B03, documentar os números reais nas notas de
  execução.

## Notas de execução

Implementado nesta sessão. Todos os itens de "O que fazer" (1-7) e o
critério de não-regressão (byte-idêntico com `embedCommonText=false`) foram
concluídos e verificados. Dois ajustes de implementação, não previstos
literalmente no plano, foram necessários:

1. **`EscapeJsonString` não escapava caracteres de controle** (só `"`/`\`).
   Texto comum de verdade (ex.: uma linha de crédito do MusicXML de Clair de
   Lune, `"from “Suite Bergamasque” L. 75\n"`) pode conter um `\n` literal,
   o que gerava um `a/score.json` sintaticamente inválido
   (`json.decoder.JSONDecodeError: Invalid control character`). Corrigido
   para escapar todo `c < 0x20` (`\n`/`\r`/`\t` nomeados, resto via
   `\u00XX`) — função usada por todo o writer, não só por texto, então o
   fix é geral.
2. **Estilo/peso efetivo de texto comum depende de uma regra CSS global que
   o SVG já embute**, não só do `FontInfo` (`GetStyle()`/`GetWeight()`):
   `SvgDeviceContext::Commit` escreve
   `"g.ending, g.fing, g.reh, g.tempo {font-weight:bold;} g.dir, g.dynam,
   g.mNum {font-style:italic;} g.label {font-weight:normal;}"` no `<style>`
   do SVG. Pelo menos um caso real do corpus (a marca numérica de tempo
   "49" em Clair de Lune, `class="tempo"`) só declara `font-style="italic"`
   no `FontInfo` e depende inteiramente dessa regra CSS para sair em
   negrito no SVG — sem replicar a regra, esse texto saía só itálico no
   Lottie (peso errado). Corrigido com um passeio de ancestrais
   (`TextStyleContext`/`ApplyClassStyleRule` em `lottiewriter.cpp`) que
   casa `LottieNode::className` (mesma string usada como atributo `class`
   do SVG, ver `StartGraphic`) contra os mesmos nomes de classe, cumulativo
   pela árvore (mesmo padrão de herança de `colorCss`/`hidden`). Esse caso
   específico também é bold+italic simultâneo → cai no fallback de Bold já
   previsto pela decisão de B03, com o aviso "bold italic... falling back
   to Bold" (confirmado nos logs).

   **Atualização (D01-5):** esse fallback deixou de ser aceito como
   limitação permanente — é exatamente o "49"/"50"/"51"... de Clair de
   Lune que o usuário reportou depois, numa sessão futura, como "números
   dos compassos em itálico no SVG mas não no Lottie". Bold Italic foi
   adicionado; ver [D01-5](D01-5-bold-italico-tempo.md).
3. **Não-regressão**: `xml:id`s não explícitos no MEI/MusicXML são gerados
   com `std::random_device` (`Object::GenerateHashID`), então rodar o
   binário duas vezes com o mesmo comando produz JSONs *diferentes* mesmo
   sem nenhuma mudança de código — um diff ingênuo antes/depois dá falso
   positivo. Usei `-x <seed>` (`--xmlIdSeed`, ex. `-x 42`) nas duas rodadas
   para tornar a geração determinística; com isso, `lottie`/CLI (sem
   `embedCommonText`) ficou **byte-idêntico** antes/depois em todas as
   páginas testadas (Chopin Étude, Clair de Lune) — vale registrar esse
   truque para qualquer não-regressão futura do exportador.
4. **Estilos itálico/negrito confirmados visualmente**: "legatissimo"
   (Chopin, itálico), "Allegro molto agitato." (Chopin, negrito, via
   `FontInfo` normal), "49" (Clair de Lune, negrito via a regra CSS do
   item 2), "peu à peu cresc. et animé" (Clair de Lune, itálico) e o nome
   do compositor/título (centralizado/à direita) — todos com o glifo,
   estilo e posição corretos por inspeção visual direta de recortes PNG.

   **Correção (D01-4):** para o itálico, "estilo e posição corretos" não se
   sustentou. A face selecionada era a certa (`LiberationSerif-Italic`),
   mas o ThorVG do `dotlottie-rs` aplicava um itálico sintético por cima
   (inclinação dupla + deslocamento horizontal, letras "grudadas"), e a
   inspeção de recortes não pegou. O exportador sempre esteve certo; a
   correção foi no ThorVG. Ver
   [D01-4](D01-4-italico-sintetico-thorvg.md).

### Achado importante: a % de divergência do corpus **não caiu** como A13/B03
### previam — precisa de leitura antes de decidir os próximos passos

Critério de aceite esperado: "a % de divergência cai... pra um valor
consistente com a categoria 2 (ruído de antialiasing)". **Medido, não
aconteceu**: rodando `compare-corpus.sh 32` (mesmo corpus, mesma
tolerância 32 de A13):

| | mín | máx | média |
| --- | --- | --- | --- |
| Antes (A13, sem texto) | 0,1128% | 0,7658% | 0,3269% |
| Depois (D01, com texto) | 0,1115% | 0,8206% | **0,3601%** |

A média **subiu** ligeiramente em vez de cair; por peça o resultado é misto
(Gymnopédie e Maple Leaf Rag melhoraram um pouco; Chopin Étude, Mazurka,
Clair de Lune e Prelúdio BWV 846 pioraram um pouco; Nocturne ficou
essencialmente igual). Nenhuma peça piorou drasticamente e a inspeção
visual direta (item 4 acima) confirma que o texto está correto — ou seja,
**o exportador está certo; a métrica de PNG diff é que ficou enganosa**,
por duas causas raiz identificadas e confirmadas com reprodução isolada,
ambas do lado da ferramenta de referência (`resvg`/fontconfig deste
ambiente), não do exportador:

1. **Bug real do `resvg` com `text-anchor` + `<title>` aninhado.** Todo
   texto do SVG do Verovio com `@label` (títulos de página, nome do
   compositor) sai como
   `<tspan x=".." text-anchor="middle|end"><title class="labelAttr">..</title>
   <tspan>...texto real...</tspan></tspan>`. Reproduzido isoladamente
   (`/tmp/.../scratchpad/d01-spike/anchor-test.svg` vs `anchor-test2.svg`,
   não commitados): **sem** o `<title>` aninhado o `resvg` centraliza
   corretamente; **com** ele, o `resvg` mede a largura do texto errado e
   renderiza a maior parte do título fora da página (canto esquerdo,
   cortado) — exatamente o que se via no PNG de referência do Chopin Étude
   e do Clair de Lune. Meu texto no Lottie está corretamente centralizado/
   alinhado à direita (confirmado nas mesmas posições x que o próprio SVG
   declara); é o PNG de *referência* que está errado para esse elemento
   específico. Excluindo só a faixa do título (200px do topo) do diff do
   Chopin Étude p.1, a % cai de 0,7166% pra 0,5462% — o título sozinho é
   ~24% do "diferente" medido ali.
2. **Fontes fisicamente diferentes**: `fc-match "Times, serif"` neste
   ambiente resolve pra **Nimbus Roman** (não Liberation Serif) — é essa
   fonte que o `resvg` usa pra desenhar texto comum no SVG de referência,
   enquanto o Lottie usa a Liberation Serif de verdade (embutida, T1).
   Ao contrário de glifos SMuFL/vetores (mesma geometria "assada" nos dois
   exportadores, por isso zero divergência estrutural em A13), texto comum
   agora compara **contornos de fontes fisicamente diferentes**: mesmo
   perfeitamente posicionado/dimensionado/com o estilo certo, duas fontes
   metricamente parecidas mas não idênticas produzem uma faixa de pixels
   divergentes bem maior que ruído de antialiasing simples — visível como
   um "contorno duplo" grosso em vez de fino nos recortes com zoom.
   Tentei forçar o `resvg` a usar a Liberation Serif via
   `compare svg-to-png --font <LiberationSerif-Regular.ttf>`: **sem
   efeito** (o resultado do diff não mudou nem 1 pixel) — o fontconfig/
   `resvg` deste ambiente continua preferindo a Nimbus Roman já registrada
   no sistema para a família genérica "Times, serif" independente das
   fontes carregadas via `--font`; não investiguei mais fundo (fora do
   escopo do D01 em si).

**Isso não bloqueia nem reabre D01** (a decisão T1/B03 está implementada
corretamente, byte-a-byte e visualmente confirmada) mas é importante para
quem for revisitar `docs/plano/relatorio-paridade.md`/rodar
`compare-corpus.sh` de novo (D06 depende disso: "remedir tamanho depois de
D01"): comparar contra este `resvg`/fontconfig específico não é mais um
proxy fiel de "o texto está certo" pra elementos centralizados/à direita
com `label`, nem um proxy de precisão sub-pixel pra texto comum em geral.
Se isso importar pra decisões futuras (ex. D06 decidir se o tamanho do
pacote é um problema), vale um passo dedicado pra consertar a ferramenta
de comparação (fixar a fonte usada pelo `resvg` via uma config de
fontconfig isolada para `compare`, e/ou contornar o bug de
`text-anchor`+`<title>`) antes de tirar conclusões numéricas de futuras
rodadas de `compare-corpus.sh` envolvendo texto comum.

**Atualização:** as duas causas foram resolvidas — fonte física em
[D01-2](D01-2-controle-de-fonte-na-comparacao.md) (`--pin-serif-family`) e
o bug de `<title>` aninhado em
[D01-3](D01-3-titulo-aninhado-resvg.md) (remoção do nó antes do `usvg`
processar). Corpus final (`compare-corpus.sh 32`, ambas as correções
aplicadas): 0,1002%–0,5997%, média 0,2977% — ainda não no nível de ruído
puro de antialiasing (categoria 2 de `relatorio-paridade.md`), mas as duas
causas conhecidas e não relacionadas ao exportador já foram isoladas e
corrigidas do lado da ferramenta.

**Atualização 2 (D01-4):** havia uma terceira causa, também fora do
exportador: o ThorVG usado pelo `dotlottie-rs` aplicava itálico sintético por
cima da `LiberationSerif-Italic` embutida (inclinação dupla em todo texto
itálico). Corrigido numa cópia local do ThorVG
([D01-4](D01-4-italico-sintetico-thorvg.md)). Corpus, medido de novo com e
sem a correção: 0,0196%–0,5219%/média 0,2293% → **0,0135%–0,4153%/média
0,1318%**.

**Tamanho do pacote** (10 peças do corpus, antes eram os números "sem
texto" de A13):

| Peça | Antes (A13) | Depois (D01) | Acréscimo |
| --- | --- | --- | --- |
| Nocturne Op.9 No.1 | 429,4 KB | 1133 KB | +703 KB |
| Clair de Lune | 366,2 KB | 1059 KB | +693 KB |
| Chopin Étude Op.10 No.9 | 315,4 KB | 983 KB | +668 KB |
| Maple Leaf Rag | 242,0 KB | 914 KB | +672 KB |
| Grieg Butterfly | 248,2 KB | 904 KB | +656 KB |
| Chopin Mazurka | 227,4 KB | 877 KB | +650 KB |
| Prelúdio BWV 846 | 137,5 KB | 785 KB | +648 KB |
| Grieg Little bird | 155,3 KB | 793 KB | +638 KB |
| Scarlatti | 147,0 KB | 788 KB | +641 KB |
| Gymnopédie | 77,2 KB | 704 KB | +627 KB |

Acréscimo fixo de ~627-703 KB por peça, batendo com a estimativa de B03
(~600-650 KB) — um pouco acima, mas da mesma ordem de grandeza.
