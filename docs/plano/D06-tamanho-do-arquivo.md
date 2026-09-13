# D06 — Tamanho do arquivo (reuso de glifos repetidos)

**Depende de:** D01 (texto comum — muda completamente o quadro de tamanho),
A13 (varredura de referência) · **Decisão necessária:** SIM, mas em duas
etapas — primeiro remedir com o usuário decidindo se vale a pena continuar
(ver "Passo 0"), e só then, se a resposta for sim, uma segunda decisão sobre
a técnica (ver "Decisão necessária (técnica)").

## Objetivo

Investigar se o tamanho dos pacotes `.lottie` do corpus é um problema real
**depois** de D01 (texto comum) estar implementado, e só então (se for)
reduzir reaproveitando glifos SMuFL repetidos (e possivelmente caracteres de
texto comum repetidos) via precomps do Lottie, em vez de "assar" a geometria
completa em cada uso — que é o que o exportador faz hoje (risco 3 do
`README.md`).

## Passo 0 — remedir antes de decidir (obrigatório, faça isso primeiro)

`docs/plano/relatorio-paridade.md` (A13) mediu 77-430 KB por pacote, **sem
texto comum** (D-TEXTO ainda não implementado naquela varredura). D01
acrescenta um custo **fixo** de ~600-650 KB por peça (3 fontes embutidas,
Regular+Italic+Bold), independente do tamanho da partitura — ou seja, о
quadro que motivou originalmente "não parece crítico" em A13 muda
completamente depois de D01: pacotes pequenos (ex. Gymnopédie, 77 KB antes)
passariam a ~680-730 KB, um aumento de 9-10×.

1. Depois de D01 implementado, rodar `compare/scripts/compare-corpus.sh 32`
   de novo e olhar `compare/out/corpus/tamanhos.txt` (mesmo script/saída de
   A13).
2. Levar os números reais (não estimados) para o usuário e perguntar
   explicitamente se o tamanho resultante é um problema para o caso de uso
   do zywny (ex.: existe algum limite de tamanho de download/cache do
   player, alguma meta de KB/peça?) — **não decidir sozinho que "é grande
   demais"** nem seguir para a técnica de reuso sem essa resposta, o mesmo
   princípio de D-DESTAQUE/D-TEXTO (não reabrir decisão, mas essa em
   particular nunca foi tomada).
3. Se o usuário decidir que o tamanho pós-D01 é aceitável: **este passo
   termina aqui**, documentar os números medidos e não implementar nada.

## Se o usuário decidir que vale a pena reduzir: contexto técnico

## Ler antes (só isto)

- `docs/plano/README.md`, risco 3 ("Tamanho do arquivo") — a causa raiz já
  identificada: "a fase A 'assa' as coordenadas de cada glifo em cada uso
  (não há `<use>` em Lottie)".
- `verovio/src/lottiedevicecontext.cpp` L385-426 (`MakeGlyphShape`) — já
  cacheia o **parse** do glifo por codepoint (`m_glyphCache`, evita
  reparsear o XML do path), mas cada chamada ainda produz um `LottieShape`
  novo com vértices já escalados/posicionados — ou seja, o cache existente
  só economiza CPU no export, **não** economiza bytes no JSON de saída.
- `verovio/include/vrv/lottiegeometry.h` L42-58 (`LottieShape`) — hoje não
  existe nenhum conceito de "referência a um asset compartilhado" na IR;
  toda forma é sempre inline.
- `verovio/src/lottiewriter.cpp` L637-638 (`"assets":[]` sempre vazio hoje —
  mesmo ponto que D01 usa para `fonts`, mas para precomps o array `assets`
  do Lottie é o mecanismo certo: um asset tipo precomposição
  (`"id"`, `"layers":[...]`), referenciado por uma camada `"ty":0` com
  `"refId"` + seu próprio `ks` (posição/escala/rotação da instância) — é o
  equivalente Lottie ao `<use>` do SVG que a arquitetura atual não usa).
- `compare/out/corpus/tamanhos.txt` (gerado no Passo 0) — dados reais de
  quais peças/quantos glifos repetidos existem, para estimar o ganho antes
  de implementar (contar glifos SMuFL por codepoint em cada peça: os que
  mais se repetem são cabeças de nota, hastes/beams já são shapes simples
  desenhadas por linha/não por glifo, claves e acidentes).

## Decisão necessária (técnica)

Se o Passo 0 confirmar que vale investir, perguntar ao usuário **antes** de
implementar:

1. **Escopo do reuso**: só glifos SMuFL (maior volume, `path` 8025 +
   `use` 5444 ocorrências no corpus inteiro por A13) ou também caracteres
   de texto comum (D01 usa camada de texto nativa, não shapes — não se
   beneficia do mesmo mecanismo; só entraria se D01 tivesse sido implementado
   como contornos, T2, que **não** foi a decisão tomada em B03. Ou seja, na
   prática o escopo de D06 seria só SMuFL, a menos que D01 seja revisitado).
2. **Granularidade da chave de reuso**: um precomp por
   `(codepoint, pointSize arredondado)` (glifos do mesmo tamanho exato
   compartilham; tamanhos diferentes — grace/cue vs. normal — geram
   precomps separados) é a opção mais simples e seria a recomendação
   inicial; confirmar que o ganho de tamanho compensa a complexidade antes
   de generalizar para agrupar por família de tamanho com fator de escala
   na instância (o que reduziria ainda mais, mas é mais código).
3. **Risco de regressão visual**: a mudança toca a estrutura de todo shape
   SMuFL do exportador (praticamente 100% das notas/claves/acidentes/
   articulações) — precisa do mesmo nível de validação de A08/A09/A13
   (corpus inteiro, não só 1-2 peças), o que é bem mais caro que os outros
   passos D. Confirmar que o usuário aceita esse custo de validação antes
   de começar.

## O que fazer (esboço, só se aprovado — detalhar mais ao decidir)

1. IR: novo tipo de filho em `LottieChild` (ao lado de `group`/`shape`/
   `text` de D01) — uma referência `{glyphKey, x, y, scale}` em vez de um
   `LottieShape` inline, quando o glifo já foi emitido antes na mesma
   página (ou na composição inteira, se os precomps forem globais — decidir
   no item 2 acima).
2. Writer: ao serializar, colecionar o conjunto de `glyphKey`s realmente
   usados, emitir um asset de precomp por chave em `"assets"` (cada um com
   um único shape layer contendo aquele path, na escala canônica), e trocar
   cada uso repetido por uma camada `"ty":0"` com `"refId"` + transform da
   instância.
3. Medir de novo com `compare-corpus.sh` (tamanho **e** paridade visual —
   um precomp mal transformado quebra posição/escala de forma sutil, então
   o critério de aceite visual de A08/A09 continua valendo).

## Fora de escopo

Otimizar/comprimir o `.ttf` embutido por D01 (subsetting de glifos) — é uma
técnica diferente, específica de fonte, não de glifo SMuFL; se o custo fixo
de D01 isolado for o problema (não o de glifos repetidos), essa é uma
conversa separada com o usuário, não coberta pela técnica de precomp deste
passo. Comprimir/reotimizar o JSON gerado (minificação além do que já é
natural, remover espaços) — ganho marginal frente ao zip já usado pelo
pacote `.lottie`.

## Critérios de aceite

- Números reais do Passo 0 documentados nas notas de execução, com a
  decisão do usuário registrada (seguir ou não).
- Se seguir: tamanho médio KB/página cai de forma mensurável nas peças com
  mais repetição de glifo (comparar antes/depois com o mesmo
  `compare-corpus.sh`).
- Nenhuma regressão visual: `compare-corpus.sh 32` no corpus inteiro com as
  mesmas médias de divergência por página de antes (dentro de uma margem
  desprezível, mesmo padrão de tolerância de A13/C04).
