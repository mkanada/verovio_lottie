# B03 — Memorando: texto comum

**Insumos:** `docs/plano/B03-memorando-texto.md`, `docs/plano/A13-varredura-do-corpus.md`,
`docs/plano/relatorio-paridade.md`, `compare/README.md`, código-fonte de
`verovio/src/lottiedevicecontext.cpp` (DrawText, MakeGlyphShape) e
`verovio/src/filereader.cpp` (ZipFileWriter).

**Status:** aguardando decisão do usuário (D-TEXTO).

## Recapitulando (não reabrir)

- `verovio/data/text/Times*.xml` só tem métricas — zero `<path>`. O layout de
  texto comum do Verovio (posição/largura de cada glifo) já é calculado com
  essas métricas, independente da opção escolhida aqui.
- Hoje `LottieDeviceContext::DrawText` (`verovio/src/lottiedevicecontext.cpp:443-503`)
  já separa dois caminhos: fonte SMuFL vira shape (`MakeGlyphShape`,
  `verovio/src/lottiedevicecontext.cpp:385-426`) igual a qualquer outra
  primitiva do exportador; fonte comum só avanha o pixel-pen e conta em
  `m_skippedTextRuns` (`verovio/include/vrv/lottiedevicecontext.h:213`), sem
  desenhar nada — aviso agregado por página em `EndPage`
  (`verovio/src/lottiedevicecontext.cpp:740-750`, "N common text run(s) not
  rendered (pending D-TEXTO)").
- A13 (`docs/plano/relatorio-paridade.md`) mediu o impacto real: **texto
  comum ausente é a causa de ~95%+ dos pixels divergentes** em **34/34**
  páginas do corpus (divergência por página 0,11%–0,77%, nenhuma divergência
  estrutural). Quanto mais texto comum a peça tem (títulos longos,
  indicações, dedilhados), maior a %. Nenhum glifo SMuFL/musical está
  incompleto — só texto Times.
- `MakeGlyphShape` já implementa o pipeline completo que T2 reaproveitaria:
  parseia o path do glifo uma vez (`ParseGlyphXml`), guarda em cache por
  glifo (`m_glyphCache`), escala por `font->GetPointSize()/UnitsPerEm`, e
  herda fill/stroke de `currentColor` — o mesmo padrão já validado em
  A08/A09/A10 e confirmado por A13 sem nenhuma divergência estrutural.
- `LottieBezier` (`verovio/include/vrv/lottiegeometry.h:33-38`) é um formato
  **cúbico** (vértices + tangente de entrada/saída relativa). Contornos
  TrueType são **quadráticos** — a conversão é uma elevação de grau em forma
  fechada (tangente = 2/3 do vetor até o ponto de controle), não um problema
  de pesquisa.
- `ZipFileWriter::AddFile(archivePath, content)`
  (`verovio/src/filereader.cpp:140-143`) empacota via `miniz_cpp::writestr`
  usando `std::string` como buffer de bytes — binário-seguro (não depende de
  terminador nulo), então embutir um `.ttf` bruto no pacote é só ler o
  arquivo e chamar `AddFile`, sem infraestrutura nova.
- `Resources`/`Toolkit::SetResourcePath` já carregam fontes de um
  `--resource-path` em tempo de execução (`verovio/src/toolkit.cpp:123-144`)
  — vendorizar um `.ttf` em `verovio/data/` e lê-lo na hora de exportar seria
  uma extensão natural desse padrão, para T1 ou T2.
- O repositório já tem Liberation (família metricamente compatível com
  Times, licença SIL OFL) referenciada via a flag `fontTextLiberation`
  (`verovio/src/options.cpp:1322-1324`), mas só como **woff2 embutido em
  base64** dentro de `verovio/data/Liberation.css` — não há um `.ttf` avulso
  vendorizado ainda. T1 e T2 precisariam vendorizar um `.ttf` de verdade
  (trivial: mesma fonte, mesma licença, só formato de arquivo diferente).

## Spike executado (T1) — resultado

Seguindo o "Spike mínimo" do plano: montei à mão um `.lottie` com uma única
camada de texto (`ty:5`) + `fonts.list` (`origin:3`, `fPath:
"f/LiberationSerif-Regular.ttf"`) + o `.ttf` do sistema
(`/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf`,
Liberation Serif, SIL OFL) embutido no pacote, com o texto `"Andante très
expressif"` (mesma frase do exemplo 1 de A13, incluindo o acento agudo em
"très" para testar diacríticos). Renderizado com `compare lottie-to-png
--width 800 --height 200 --frame 0` (mesmo binário validado no resto do
projeto).

**Resultado: funciona.** `compare` termina com exit 0 (só os dois avisos
inofensivos de `set_frame`/`render` já documentados em `compare/README.md`)
e o PNG mostra "Andante très expressif" corretamente renderizado em
Liberation Serif, acento incluído — confirma que T1 funciona ponta a ponta
no `dotlottie-rs`/ThorVG, o renderizador já validado no resto do projeto.
Achei também, no próprio código do `dotlottie-rs` (não só na fixture citada
no plano), um teste unitário equivalente
(`src/renderer/thorvg.rs:1264-1310`,
`asset_resolver_memoizes_loaded_fonts_and_failures`) que já exercita
exatamente esse mesmo formato de JSON (`fonts.list` com `origin:3` +
`fPath`) — reforça que não é um comportamento incidental da versão vendorizada.

**Custo de tamanho medido:** o pacote com essa única camada de texto +
`LiberationSerif-Regular.ttf` (388.226 bytes crus) ficou com **212.959 bytes
comprimidos** (~46% de compressão) — isto é, **cada estilo de fonte
embutido custa ~208 KB comprimidos, fixos, por arquivo**, independente do
tamanho da peça.

## Fato novo: o corpus já usa itálico/negrito, não só regular

Checando o `font-style`/`font-weight` nos SVGs já gerados pelo A13
(ex. Clair de Lune): **25 ocorrências de `font-style="italic"`** só na
página 1 ("Andante très expressif", "con sordina", "Tempo rubato" etc.) e
algumas de `font-weight="bold"` (títulos). Ou seja, T1 em produção
precisaria embutir **pelo menos Regular + Italic** (não um único arquivo),
o que ~dobra o custo fixo medido acima: **~400-450 KB fixos por peça**,
somando Regular + Italic.

Comparando com os tamanhos medidos em A13 (pacotes completos, todas as
páginas, **sem** texto comum ainda): 77 KB (Gymnopédie, 2 páginas) a 430 KB
(Nocturne, 7 páginas). Ou seja, **o custo fixo de T1 (Regular+Italic) já é
da mesma ordem de grandeza que o maior pacote do corpus inteiro hoje**, e
maior que a maioria dos pacotes pequenos — um acréscimo de 2× a 5×+
dependendo da peça, sem nem contar o texto em si (que T1 nem adiciona bytes
de geometria por ser fonte, ao contrário de T2).

## Opções avaliadas

| Critério | T1 (fonte TTF + camada de texto) | T2 (contornos via stb_truetype) | T3 (não renderizar) |
| --- | --- | --- | --- |
| Fidelidade ao SVG | ✅ fonte real, mesma família já usada (fontTextLiberation) | ✅ mesmo pipeline já sem divergência estrutural em A13 (SMuFL) | ❌ é a causa de ~95%+ da divergência medida em A13 |
| Consistência entre players | ⚠️ só validado no `dotlottie-rs` (spike desta sessão); depende do player de destino suportar asset de fonte + camada de texto do dotLottie — não testado no runtime real do zywny | ✅ vira shape comum — mesmo primitivo usado em 100% do resto da partitura, já provado consistente resvg×ThorVG em A13 | n/a |
| Suporte no ThorVG/dotlottie-rs | ✅ confirmado nesta sessão (spike) | ✅ não usa camada de texto — reaproveita o caminho de shape já provado em A09/A10/A13 | n/a |
| Tamanho do arquivo | ❌ medido: ~208 KB/estilo comprimido; Regular+Italic ≈ 400-450 KB **fixos**, já maior que o maior pacote do corpus hoje | ⚠️ não medido (sem código ainda), mas escala com nº de glifos distintos usados — mesma ordem de grandeza do custo de SMuFL, que A13 chamou de "não alarmante" | ✅ custo zero |
| Licença da fonte | ✅ Liberation Serif, SIL OFL, já usada no projeto | ✅ mesma fonte, só como fonte de contorno, mesma licença | n/a |
| Complexidade no exportador | baixa — reaproveita `ZipFileWriter::AddFile` + padrão de `Resources`/`SetResourcePath`, sem parser novo | média, mas limitada — vendorizar `stb_truetype.h` (domínio público, header único) + uma função de elevação quadrática→cúbica bem conhecida, plugada no mesmo `DrawText`/`MakeGlyphShape` que já existe | nenhuma — já é o comportamento atual |
| Impacto medido em A13 | resolve a causa dominante (~95%+ dos pixels) | idem | não resolve nada — mantém a causa dominante aberta |

## Recomendação

**T2**, por quatro razões:

1. **Consistência arquitetural**: todo o resto do exportador (notas, hastes,
   ligaduras, texto SMuFL) já é um shape vetorial comum — nenhuma primitiva
   nativa do Lottie além de shape é usada em lugar nenhum do código hoje.
   T2 faz texto comum seguir a mesma regra, em vez de introduzir uma segunda
   forma de renderizar coisas no meio de um exportador cujo critério de
   correção (CLAUDE.md) é puramente visual/PNG-diff, não "o motor de texto
   do player bateu por acaso".
2. **Remove o maior risco não testado de T1** — suporte do player de
   destino a asset de fonte + camada de texto do dotLottie — sem precisar de
   spike contra o runtime real do zywny (que já não existe hoje; B02 deixou
   em aberto um risco parecido, de compositing multi-instância, que só vai
   ser investigado em C00 — empilhar um segundo risco de suporte de player
   não testado, quando existe caminho mais seguro, parece evitável).
3. **A matemática de tamanho favorece T2** na escala já medida: o custo
   fixo por estilo de fonte de T1 (~208 KB comprimidos, medido) já é da
   ordem do maior pacote do corpus inteiro hoje (430 KB); T2 escala com
   glifos distintos realmente desenhados, mesma ordem de grandeza do que
   A13 já mediu para SMuFL e chamou de "não alarmante".
4. **A implementação é limitada, não exploratória**: `stb_truetype.h` é um
   header único de domínio público bem conhecido; contornos TrueType são
   quadráticos e o formato `LottieBezier` já usado no projeto é cúbico
   (v/i/o) — quadrática cabe exatamente numa cúbica por uma elevação de grau
   em forma fechada. É o mesmo tipo de trabalho que A08 já fez (parser de
   path de glifo), não uma incógnita nova.

**Contraponto, para ser transparente**: T1 é a opção mais testada agora —
esta sessão já a validou ponta a ponta com um spike de verdade, enquanto a
estimativa de complexidade de T2 vem de leitura de código, não de um spike
funcionando. Se a prioridade for "ir com o que já demonstrou funcionar" em
vez de "investir na opção mais consistente/barata em escala", T1 é
defensável — especialmente limitando o MVP a um único estilo (Regular),
coerente com "paridade visual incremental, casos simples antes de
complexos" do CLAUDE.md, mesmo sabendo que isso deixa itálico (bem presente
no corpus) sem cobertura por enquanto.

## Riscos e itens não testados

1. Nenhuma das duas opções foi medida contra o corpus inteiro ainda — o
   spike desta sessão foi uma única camada isolada, não uma integração real
   no exportador. Qualquer opção escolhida deveria ser remedida com
   `compare-corpus.sh` depois de implementada, como A13 já fez para os
   glifos.
2. Negrito aparece pouco no corpus amostrado (só títulos) — vale confirmar
   se o MVP precisa cobrir itálico **e** negrito, ou se dá para adiar um dos
   dois.
3. T2 precisa decidir a cobertura de caracteres do parser (pelo menos
   Latin-1 com acentos, já visto em "très"; dedilhados/números também
   aparecem) — não é um problema em aberto tecnicamente, só uma escolha de
   escopo do MVP.
4. `f/LiberationSerif-Regular.ttf` usado no spike veio das fontes do
   sistema operacional, não do repositório — qualquer opção escolhida
   precisa vendorizar o `.ttf` de verdade em `verovio/` (mesma licença já
   usada, só um arquivo novo).

## Notas de execução

- Spike em `/tmp/.../scratchpad/b03-spike/` (fora do repo, não commitado):
  `pkg/manifest.json`, `pkg/a/texto.json` (camada de texto à mão), `pkg/f/
  LiberationSerif-Regular.ttf` (copiado do sistema), zipado em
  `b03-spike.lottie` (212.959 bytes) e renderizado com `compare
  lottie-to-png` — PNG confere visualmente "Andante très expressif" em
  Liberation Serif.
- Achado extra não previsto no plano original: teste unitário do próprio
  `dotlottie-rs` (`asset_resolver_memoizes_loaded_fonts_and_failures`,
  `src/renderer/thorvg.rs:1264-1310`) já cobre o mesmo formato de
  `fonts.list`/`origin:3` — evidência independente de que T1 não depende de
  comportamento incidental da versão vendorizada.
- Achado extra: font-style italic é comum no corpus real (25 ocorrências só
  em Clair de Lune p.1) — muda a conta de custo fixo de T1 de "~208 KB" para
  "~400-450 KB" (Regular+Italic), o que não estava quantificado no plano
  original e pesou na recomendação.

## Decisão do usuário

Registrado em 2026-09-13.

1. **Mecanismo: T1** — fonte TTF embutida no pacote + camada de texto nativa
   do Lottie (`ty:5` + `fonts.list`), não T2. O usuário priorizou a opção já
   validada ponta a ponta nesta sessão (spike funcionando de verdade no
   `dotlottie-rs`/ThorVG) em vez da opção mais barata/consistente em escala
   mas ainda não implementada (T2) — decisão consciente do contraponto
   levantado na recomendação. **T2 fica descartado para o MVP** (pode
   voltar a ser considerado depois, se o custo de tamanho de T1 virar
   problema real em produção — não é uma decisão irreversível, mas não é
   revisitada sem novo motivo concreto).
2. **Cobertura de estilos: Regular + Italic + Bold** (os três, não só
   Regular). Custo fixo aceito: ~600-650 KB comprimidos por peça (3 ×
   ~208-220 KB/estilo, medido no spike só para Regular) — bem acima da
   maioria dos pacotes do corpus hoje (77-430 KB, A13, sem texto comum
   ainda). Aceito conscientemente para cobrir os casos reais já vistos no
   corpus (itálico em indicações/andamento, negrito em títulos). **Bold
   Italic não fica coberto** (não fazia parte da pergunta nem foi pedido) —
   texto que precisasse dos dois ao mesmo tempo simultaneamente ficaria sem
   um estilo exato; não visto no corpus amostrado até agora, mas é um limite
   a anotar caso apareça.

   **Atualização (D01-5):** apareceu — números de tempo/rubato bold+italic
   em Clair de Lune (D01 já tinha achado e documentado o caso em suas
   próprias "Notas de execução", mas como limitação aceita). O usuário
   pediu para investigar e corrigir ao notar o sintoma numa sessão
   posterior; Bold Italic foi adicionado (4º arquivo `.ttf`, ~+200 KB
   fixos por peça). Ver [D01-5](../D01-5-bold-italico-tempo.md).
3. **Fonte: Liberation Serif**, confirmada — mesma família já usada pelo
   projeto via `fontTextLiberation`, licença SIL OFL. Precisa ser vendorizada
   como `.ttf` de verdade em `verovio/` (hoje só existe embutida em woff2
   dentro de `verovio/data/Liberation.css`) — 4 arquivos: Regular, Italic,
   Bold, e (mesmo não estritamente pedido) confirmar se Bold Italic deveria
   ser incluído ao implementar D01, dado que já se decidiu cobrir Bold e
   Italic separadamente.

## Notas de execução (pós-decisão)

- D-TEXTO **decidido**: T1 (Liberation Serif Regular+Italic+Bold, camada de
  texto nativa do dotLottie). Replicar em `CLAUDE.md` ("Decisões
  arquiteturais já tomadas") e na tabela de decisões/passos de
  `docs/plano/README.md`, depois seguir para D01 (ver D00) para detalhar a
  implementação real no exportador (hoje `DrawText` só conta
  `m_skippedTextRuns` — ver `verovio/src/lottiedevicecontext.cpp:493-502`).
- Itens que D01 precisa decidir/fazer, ainda não cobertos aqui: (a)
  vendorizar os `.ttf` de Liberation Serif (Regular/Italic/Bold, e decidir
  sobre Bold Italic) em `verovio/data/`; (b) mapear `font-style`/`font-weight`
  do texto comum do Verovio para o `fName`/`fFamily` correto de cada estilo
  embutido; (c) montar a estrutura de camada de texto (`ty:5`, `t.d.k[].s`)
  a partir do texto e posição já calculados por `View::DrawText`, em vez do
  branch atual que só avança o pen; (d) confirmar cobertura de caracteres
  (acentos/diacríticos já vistos em "très") funciona igual no corpus
  inteiro, não só na frase testada no spike; (e) remedir tamanho de pacote e
  paridade visual com `compare-corpus.sh` depois de implementado, como A13
  já fez para os glifos.
