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

_(preencher ao executar)_
