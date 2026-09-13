# D01-3 — Corrigir medição de largura do `resvg` com `<title>` aninhado

**Depende de:** D01 (achou o bug), D01-2 (resolveu a outra causa raiz, fonte
física diferente) · **Decisão necessária:** nenhuma nova — é a segunda (e
última) causa raiz já identificada e documentada em
`docs/plano/D01-texto-comum.md` ("Achado importante") e em
`compare/README.md` ("Pegadinha do `resvg`: texto comum centralizado/à
direita com `<title>` aninhado"), explicitamente listada como próximo passo
lá e deixada "fora de escopo" em D01-2 pra não misturar duas correções na
mesma leitura de números.

Aberto a pedido do usuário ("volte ao D01 e trabalhe mais pra melhorar os
números da comparação entre o svg e o dotlottie"), depois de D01-2 já ter
resolvido a causa da fonte física.

## Problema (já medido e reproduzido em D01, não hipotético)

Todo elemento do SVG do Verovio com `@label` (título de página, nome do
compositor — `AttLabelled`/`ATT_LABELLED` em
`verovio/src/svgdevicecontext.cpp:322-330,389-397`) sai como:

```xml
<tspan x=".." text-anchor="middle"><title class="labelAttr">title</title>
  <tspan>...texto real (ex.: "Etude in F Minor")...</tspan>
</tspan>
```

`<title>` é puramente metadado (nome acessível/tooltip) — nenhum
renderizador compatível com a spec SVG o desenha. Mas o `resvg` 0.48, ao
calcular a largura total do texto pra resolver `text-anchor="middle"/"end"`,
**inclui o texto do `<title>` aninhado na medição**, produz uma largura
maior que a real e desloca o texto de verdade pra fora da página (cortado à
esquerda). O `.lottie` gerado pelo exportador está correto (mesma
coordenada `x` que o SVG declara); é o PNG de *referência* (`svg-to-png`)
que sai errado — daí a divergência medida em `compare diff` ser enganosa
justamente nesses elementos.

Reproduzido isoladamente em D01 (sem/com `<title>` aninhado, mesmo
`text-anchor`) e medido: excluir só a faixa do título (200px do topo) do
diff do Chopin Étude p.1 derruba a divergência de 0,7166% pra 0,5462% —
o título sozinho responde por ~24% do "diferente" medido ali.

## Objetivo

Fazer `compare svg-to-png` remover os nós `<title>` do SVG **antes** de
`usvg::Tree::from_data`, pra que a medição de largura do `resvg` pare de
contar um texto que nunca é desenhado — sem tocar no Verovio nem no
exportador dotLottie (o `<title>` continua sendo emitido normalmente no SVG
"de produção"; só a cópia usada internamente pelo `compare` é
pré-processada).

## Decisões de escopo tomadas aqui

- **Remoção incondicional, sem flag nova.** Ao contrário de
  `--pin-serif-family` (D01-2, que escolhe uma fonte específica — uma
  troca de comportamento com trade-off real), remover `<title>` não muda
  nada visualmente em nenhum SVG válido (elemento não-renderizável por
  definição da spec) — não há cenário em que alguém rodando `compare
  svg-to-png` quisesse preservar o bug de medição. Sempre ativo, sem gate.
- **Remoção por nome de elemento (`<title>`), não só `class="labelAttr"`**:
  o Verovio só emite `<title>` com essa classe (confirmado — únicas duas
  ocorrências em `svgdevicecontext.cpp`), mas a correção é mais robusta
  (e mais simples) removendo qualquer `<title>` do documento, já que a
  spec SVG nunca renderiza esse elemento em lugar nenhum.
- **Baseado em `roxmltree` (já resolvido transitivamente via `usvg`
  0.48.1, sem conflito de versão — `Cargo.lock` já tinha `roxmltree
  0.20.0`), não regex.** `Node::range()` (feature `positions`, default)
  dá o range exato em bytes de cada `<title>...</title>` no texto
  original; remoção por fatiamento de string nesses ranges evita
  reserialização (sem risco de mexer em escapes/entidades do resto do
  documento) e não quebra em casos como `<title/>` vazio ou texto com
  caracteres especiais dentro de outros elementos.
- **Não mexer no Verovio nem no exportador dotLottie** — mesmo princípio
  de D01-2, o problema é inteiramente da ferramenta de referência
  (`resvg`).

## Arquivos

- Modificar: `compare/src/main.rs` (`svg_to_png`: nova função
  `strip_title_elements`, chamada antes de `usvg::Tree::from_data`).
- Modificar: `compare/Cargo.toml`/`Cargo.lock` (nova dependência direta
  `roxmltree = "0.20"`, feature `positions` já default).
- Não modificar: `verovio/`, `compare/scripts/*.sh` (nenhuma flag nova pra
  propagar).

## O que fazer

1. Em `compare/src/main.rs`, adicionar:

   ```rust
   /// Remove todo nó `<title>` do SVG antes do usvg processar — o resvg 0.48
   /// inclui erroneamente o texto de `<title>` aninhado ao medir a largura
   /// para `text-anchor`, mesmo esse elemento nunca sendo desenhado (ver
   /// docs/plano/D01-3-titulo-aninhado-resvg.md).
   fn strip_title_elements(svg: &str) -> String {
       let Ok(doc) = roxmltree::Document::parse(svg) else {
           return svg.to_string();
       };
       let mut ranges: Vec<_> = doc
           .descendants()
           .filter(|n| n.has_tag_name("title"))
           .map(|n| n.range())
           .collect();
       ranges.sort_by_key(|r| r.start);

       let mut result = String::with_capacity(svg.len());
       let mut last_end = 0;
       for range in ranges {
           result.push_str(&svg[last_end..range.start]);
           last_end = range.end;
       }
       result.push_str(&svg[last_end..]);
       result
   }
   ```

2. Em `svg_to_png`, ler o SVG como `String` (não só `Vec<u8>`) e aplicar
   `strip_title_elements` antes de `usvg::Tree::from_data`:

   ```rust
   let svg_data = std::fs::read(input).with_context(...)?;
   let svg_text = String::from_utf8(svg_data)
       .with_context(|| format!("SVG não é UTF-8 válido: {}", input.display()))?;
   let svg_text = strip_title_elements(&svg_text);
   // ...
   let tree = usvg::Tree::from_data(svg_text.as_bytes(), &opt)...
   ```

3. `cd compare && cargo add roxmltree@0.20` (ou editar `Cargo.toml` a mão)
   e `cargo build --release`.

## Fora de escopo

- Qualquer outra causa de divergência não documentada em D01/D01-2/D01-3
  (ex.: kerning residual, hinting de fonte) — se sobrar divergência
  perceptível depois deste passo, é investigação nova, não coberta aqui.
- Reportar o bug upstream no `resvg` — fica registrado aqui como pista,
  não é bloqueante pro projeto.
- Mudar `compare-page.sh`/`compare-corpus.sh` — nenhuma flag nova, nada a
  propagar nos scripts.

## Critérios de aceite

- Compila (`cd compare && cargo build --release`).
- `svg-to-png` num SVG com `<title class="labelAttr">` aninhado (ex.: p.1
  de qualquer peça do corpus) produz um PNG onde o título/nome do
  compositor aparece **inteiro e centralizado/alinhado corretamente** (não
  mais cortado à esquerda) — inspeção visual por crop.
- `compare-corpus.sh 32` no corpus inteiro: documentar mín/máx/média novos
  contra os de D01-2 (0,1002%–0,6646%, média 0,3154%) — esperado queda
  adicional visível, concentrada nas peças cujo p.1 tem título/compositor
  centralizado (todas, já que é o cabeçalho padrão do Verovio).
- Nenhuma peça sem `@label`/título deveria mudar de valor (nada pra
  remover nelas) — útil como checagem de que a remoção é cirúrgica.

## Notas de execução

Implementado exatamente como planejado (`strip_title_elements` via
`roxmltree::Document::parse` + `Node::range()`, chamado antes de
`usvg::Tree::from_data`; `roxmltree = "0.20"` adicionado como dependência
direta, sem conflito de versão com a que `usvg` já resolvia
transitivamente). Nenhum ajuste de desenho foi necessário.

Critérios de aceite verificados:

- `cargo build --release` compila sem warnings novos.
- Inspeção visual (Chopin Étude p.1): antes do fix o crop da faixa do
  título mostrava só "...in F Minor" (o "Etude " cortado à esquerda,
  exatamente o bug descrito); depois do fix mostra "Etude in F Minor"
  inteiro, centralizado.
- `compare-page.sh`/`compare-corpus.sh` rodam sem erro.
- `compare-corpus.sh 32` no corpus inteiro (34 páginas): **mín 0,1002%,
  máx 0,5997%, média 0,2977%** — contra mín 0,1002%/máx 0,6646%/média
  0,3154% de D01-2. Comparação página a página confirma que a mudança é
  **cirúrgica**: só a página 1 das 5 peças de `corpus/mei` melhorou
  (Chopin Étude 0,6646%→0,5323%, Mazurka 0,5575%→0,3146%, Grieg Butterfly
  0,2335%→0,1475%, Grieg Little Bird 0,2652%→0,1779%, Scarlatti
  0,2161%→0,1624%); as 29 páginas restantes (as 5 peças `corpus/musicxml`
  inteiras + páginas 2+ das peças MEI) ficaram **byte a byte iguais** —
  nenhuma regressão em lugar nenhum.
- **Achado não previsto**: a melhoria só apareceu nas peças de
  `corpus/mei`, não nas de `corpus/musicxml` (Nocturne, Clair de Lune,
  Gymnopédie, Maple Leaf Rag, Prelúdio BWV 846) — a suposição no critério
  de aceite ("esperado queda... em todas, já que é o cabeçalho padrão do
  Verovio") **não se confirmou**. Causa provável (não investigada a
  fundo): o importador MusicXML do Verovio aparentemente não popula
  `@label` (`ATT_LABELLED`) no cabeçalho de página (título/compositor) do
  jeito que o MEI popula, então esses SVGs nunca tinham o `<title>`
  aninhado pra começar — nada pra remover, resultado idêntico é o
  comportamento correto, não uma falha do fix.
- Queda no máximo (Chopin Étude p.1, que também é o máximo do corpus
  inteiro nas duas rodadas) de 0,6646% para 0,5323% — em linha com a
  estimativa de D01 de que o título respondia por ~24% da divergência
  daquela página. CSV completo em `compare/out/corpus/resultado.csv` (não
  versionado, gerado localmente).
