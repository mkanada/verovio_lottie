# A08 — Parser de path SVG dos glifos

**Depende de:** A02 (usa `lottiegeometry.h`) · **Decisão:** nenhuma ·
Pode ser feito em paralelo a A03–A07.

## Objetivo

Converter o XML de um glifo (`Glyph::GetXML()`) em `std::vector<LottieBezier>`,
em unidades do glifo, já aplicando o `transform="scale(1,-1)"` do `<path>`.

## Ler antes (só isto)

- `verovio/src/glyph.cpp` L150-L161 — `GetXML` (string em memória ou arquivo).
- `verovio/data/Leipzig/E0A4.xml` — exemplo (cabeça de semínima), uma linha.
- `verovio/include/vrv/lottiegeometry.h`.

## Arquivos

- Criar: `verovio/include/vrv/svgpathparser.h`, `verovio/src/svgpathparser.cpp`
  (rodar `cmake ../cmake`).

## API sugerida

```cpp
// Atributo "d" de um path SVG (sem transform) → subpaths Lottie.
bool ParseSvgPathData(const std::string &d, std::vector<LottieBezier> &paths);
// XML de um glifo do Verovio (<g><path transform="scale(sx,sy)" d="..."/></g>),
// aplicando o scale do path.
bool ParseGlyphXml(const std::string &xml, std::vector<LottieBezier> &paths);
```

## Regras

1. Comandos: `M m L l H h V v C c S s Q q T t Z z`. Nos dados do Verovio só
   aparecem `M c h l s v z`, mas fontes customizadas podem trazer outros.
   `A`/`a` → `LogWarning` e ignorar o segmento.
2. Coordenadas repetidas continuam o último comando; depois de `M`/`m`, pares
   extras são `L`/`l` implícitos.
3. Tokenização: o sinal também separa números (`-54 0 -97`, `10-5`); aceitar
   `.5` e expoente `e`.
4. `S`/`s` refletem o segundo ponto de controle da cúbica anterior (se o comando
   anterior foi `C/c/S/s`); senão, o controle é o ponto corrente. `T`/`t` idem
   para quadráticas. Quadráticas viram cúbicas (fórmula em A07).
5. Cada `M`/`m` inicia um novo `LottieBezier`. `Z`/`z` marca `closed = true` e o
   ponto corrente volta ao início do subpath.
6. Ao fechar: se o último vértice coincide com o primeiro, remover o último e
   passar a tangente de entrada dele para `i[0]` (senão o Lottie cria um segmento
   de comprimento zero).
7. `ParseGlyphXml`: usar pugixml (`#include "pugixml.hpp"`, já no projeto);
   percorrer todos os `<path>` descendentes; aceitar só `transform` ausente ou
   `scale(sx,sy)`; aplicar o scale a vértices **e** tangentes. Outro transform →
   `LogWarning`.

## Fora de escopo

Uso no `DrawMusicText` (A09).

## Critérios de aceite

- Compila.
- Verificação manual temporária (remover depois): logar o resultado de
  `ParseGlyphXml` para `data/Leipzig/E0A4.xml`. Esperado:
  - 1 subpath, `closed = true`, **4 vértices**: (97, 125), (0, 42), (198, −125), (295, −42);
  - `o[0] = (−54, 0)`, `i[0] = (89, 0)` (vinda do último segmento), `i[1] = (0, 52)`.

## Notas de execução

- Criados `svgpathparser.h`/`.cpp` conforme a API sugerida. `ParseSvgPathData`
  usa uma classe local `PathBuilder` (namespace anônimo) que percorre a string
  uma vez, com um tokenizador de números próprio (`ReadNumber`) que não exige
  separador entre números — cobre `-54 0 -97`, `10-5` e `.5.5`.
- Reflexão de `S`/`s` e `T`/`t`: implementada guardando o tipo do último
  comando (`Cubic`/`Quad`/`None`) e o ponto de controle correspondente,
  resetados no topo de cada comando e sobrescritos só em `C/S` (Cubic) e
  `Q/T` (Quad) — assim qualquer outro comando no meio (inclusive `M`) quebra a
  cadeia de reflexão, como no SVG real.
- `A`/`a`: `LogWarning` e ignora o segmento **sem mover o ponto corrente**
  (interpretação literal de "ignorar o segmento" — não ocorre nos dados reais
  do Verovio, só listado por robustez a fontes customizadas).
- Guarda anti-loop-infinito: um número solto logo após `Z`/`z` (que não tem
  parâmetros) é tratado como dado malformado (`LogWarning` + `return false`)
  em vez de tentar repetir `Z` indefinidamente.
- `ParseGlyphXml` usa `pugi::xml_document::load_buffer` + travessia manual
  recursiva (`CollectPathNodes`), no mesmo estilo de `resources.cpp` (sem
  XPath). `ParseScaleTransform` aceita `scale(sx,sy)` (formato real dos dados)
  e também `scale(s)` por robustez; qualquer outro `transform` gera
  `LogWarning` e usa escala identidade (mantém o path em vez de descartá-lo).
- **Discrepância nos valores "Esperado" deste documento**: a verificação
  manual (`ParseGlyphXml` sobre `data/Leipzig/E0A4.xml`, código temporário em
  `tools/main.cpp` atrás de `--debug-svgpath`, removido depois de conferir)
  deu:
  - 1 subpath, `closed = true`, 4 vértices — bate com o esperado;
  - vértices: `(0, 39)`, `(200, −133)`, `(314, −38)`, `(96, 133)`;
  - tangentes: `i[0] = (0, 64)`, `o[0] = (0, −68)`, `i[1] = (−127, 0)`,
    `o[1] = (66, 0)`, `i[2] = (0, −58)`, `o[2] = (0, 84)`, `i[3] = (112, 0)`,
    `o[3] = (−64, 0)`.
  - Isso **difere** dos valores numéricos listados acima em "Critérios de
    aceite" (`(97, 125)`, `(0, 42)`, `(198, −125)`, `(295, −42)`,
    `o[0] = (−54, 0)`, `i[0] = (89, 0)`, `i[1] = (0, 52)`). Recalculei à mão a
    partir do `d` real do arquivo (`M0 -39c0 68 73 172 200 172c66 0 114 -37
    114 -95c0 -84 -106 -171 -218 -171c-64 0 -96 30 -96 94z`) seguindo a
    semântica padrão do SVG (`c` relativo: os três pares de coordenadas são
    todos relativos ao ponto **antes** do segmento, não encadeados entre si) e
    conferi de forma independente com um script Python; os dois métodos batem
    exatamente com a saída do parser acima. A contagem de subpaths/vértices e
    a estrutura (fechado, 4 vértices, união do último com o primeiro) batem
    com o esperado — só os números concretos do exemplo do plano parecem ter
    sido calculados à mão de forma aproximada/incorreta ao escrever o passo.
    Não ajustei o parser para "bater" com esses números por não haver
    fundamento para eles; a validação de verdade (critério visual do
    `CLAUDE.md`) fica para A09, quando o glifo entra no `DrawMusicText` e
    passa pelo diff de PNG.
- Build limpo (`cmake ../cmake && make -j4`, sem warnings novos).
