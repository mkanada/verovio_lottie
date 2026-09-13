# A02 — Árvore interna (IR) de grupos e páginas

**Depende de:** A01 · **Decisão:** nenhuma

## Objetivo

Fazer o `LottieDeviceContext` montar, durante o desenho, uma árvore equivalente
ao DOM que o `SvgDeviceContext` monta: grupos com `xml:id`, classe, cor,
visibilidade e rotação, filhos ordenados, separados por página. Ainda sem formas
— só a estrutura e um ponto único de inserção de formas.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L251-L374 — `StartGraphic`, `StartCustomGraphic`.
- `verovio/src/svgdevicecontext.cpp` L427-L482 — `ResumeGraphic`, `EndGraphic`,
  `EndCustomGraphic`, `SetCustomGraphicColor`, `EndResumedGraphic`, `RotateGraphic`.
- `verovio/src/svgdevicecontext.cpp` L484-L572 — `StartPage`/`EndPage`.
- `verovio/src/svgdevicecontext.cpp` L607-L617 — `AddChild` (ordem de inserção).
- `verovio/src/svgdevicecontext.cpp` L1251-L1279 — `AppendIdAndClass` (id só para `PRIMARY`).
- `verovio/include/vrv/devicecontext.h` L134-L140 (getters de tamanho e escala,
  `GetBaseSize`) e L264-L306 (graphics, custom, resume, text graphic).
- `verovio/include/vrv/vrvdef.h` L712 — `enum GraphicID { PRIMARY, SPANNING, SYMBOLREF }`.
- `verovio/src/view_page.cpp` L83-L96 — o que o `View` configura antes de `StartPage`.

## Arquivos

- Criar: `verovio/include/vrv/lottiegeometry.h`
- Modificar: `verovio/include/vrv/lottiedevicecontext.h`, `verovio/src/lottiedevicecontext.cpp`

## Estrutura sugerida (`lottiegeometry.h`)

Pode ajustar nomes; mantenha a semântica. Incluir `devicecontextbase.h` (para
`Point`, `COLOR_NONE`, `LineCapStyle`, `LineJoinStyle`).

```cpp
struct LottieVec { double x = 0.0; double y = 0.0; };

// Subpath no formato do Lottie: tangentes relativas ao vértice.
struct LottieBezier {
    std::vector<LottieVec> v; // vértices (coordenadas da página)
    std::vector<LottieVec> i; // tangente de entrada de cada vértice
    std::vector<LottieVec> o; // tangente de saída de cada vértice
    bool closed = false;
};

enum class LottieShapeKind { Path, Rect, Ellipse };

struct LottieShape {
    LottieShapeKind kind = LottieShapeKind::Path;
    std::vector<LottieBezier> paths; // Path: subpaths compartilham o preenchimento
    LottieVec center, size;          // Rect / Ellipse
    double radius = 0.0;             // Rect arredondado
    bool hasFill = false;
    int fillColor = COLOR_NONE;      // COLOR_NONE = herda a cor do grupo
    double fillOpacity = 1.0;
    bool hasStroke = false;
    double strokeWidth = 1.0;
    int strokeColor = COLOR_NONE;
    double strokeOpacity = 1.0;
    LineCapStyle lineCap = LINECAP_DEFAULT;
    LineJoinStyle lineJoin = LINEJOIN_DEFAULT;
    double dashLength = 0.0;
    double gapLength = 0.0;
};

struct LottieNode;

struct LottieChild {
    std::unique_ptr<LottieNode> group; // não nulo = subgrupo
    LottieShape shape;                 // usado quando group == nullptr
};

struct LottieNode {
    std::string id;        // xml:id (vazio se não for PRIMARY)
    std::string className; // Object::GetClassName() (+ classes extras) ou nome do custom graphic
    std::string colorCss;  // @color ou SetCustomGraphicColor; vazio = herda
    bool hidden = false;
    bool hasRotation = false;
    double rotation = 0.0;
    Point rotationOrigin;
    std::vector<LottieChild> children; // ordem de documento: o posterior é pintado por cima
};

struct LottiePage {
    std::unique_ptr<LottieNode> root;
    int width = 0, height = 0, contentHeight = 0;
    int baseWidth = 0, baseHeight = 0;
    double userScaleX = 1.0, userScaleY = 1.0;
    double viewBoxFactor = 10.0;
    int originX = 0, originY = 0;
};
```

## O que fazer no `LottieDeviceContext`

Membros: `std::vector<LottiePage> m_pages;`, `std::vector<LottieNode *> m_nodeStack;`,
`std::map<std::string, LottieNode *> m_idMap;`.

1. `StartPage()`: criar `LottiePage` com `root` novo e copiar `GetWidth()`,
   `GetHeight()`, `GetContentHeight()`, `GetBaseSize()`, `GetUserScaleX/Y()`,
   `GetViewBoxFactor()`, `m_originX/Y`. Pilha = `{root}`. Limpar `m_idMap`
   (no SVG a busca de `ResumeGraphic` só enxerga o documento da própria página).
2. `EndPage()`: esvaziar a pilha (espera-se que só reste a raiz).
3. `StartGraphic(object, gClass, gId, graphicID, prepend)`:
   - `id = (graphicID == PRIMARY) ? gId : ""`;
   - `className = object->GetClassName()`, mais `" " + gClass` se `gClass` não for vazio;
   - cor: mesma lógica de `svgdevicecontext.cpp` L313-L320 (`ATT_COLOR` → `colorCss`);
   - `hidden`: `ATT_VISIBILITY` com `GetVisible() == BOOLEAN_false` (L350-L361; ignore `showHidden`);
   - inserir no nó corrente: no início se `prepend`, senão no fim; empilhar;
   - se `id` não vazio, registrar em `m_idMap`.
4. `EndGraphic`, `EndResumedGraphic`: desempilhar. Não sobrescreva
   `StartTextGraphic`/`EndTextGraphic`: a implementação padrão da base já chama
   `StartGraphic`/`EndGraphic`.
5. Sobrescrever `StartCustomGraphic(name, gClass, gId)` (nó com
   `className = name` [+ gClass], `id = gId`; empilhar) e `EndCustomGraphic()`
   (desempilhar). Na base são no-op (`devicecontext.h` L274-L275).
6. `SetCustomGraphicColor(color)`: grava `colorCss` no nó corrente.
7. `ResumeGraphic(object, gId)`: se `gId` estiver em `m_idMap`, empilhar esse nó;
   senão, empilhar o nó corrente (mesmo comportamento do SVG em L431-L435).
8. `RotateGraphic(orig, angle)`: se o nó corrente ainda não tem rotação, gravar
   (o SVG ignora quando já existe transform, L477-L479).
9. Método privado `void AddShape(LottieShape &&shape)` replicando `AddChild`
   (L607-L617): se o nó corrente já tem algum filho que é grupo, inserir a forma
   **antes do primeiro filho grupo**; senão, se `m_pushBack`, no início; senão, no
   fim. Todos os `Draw*` dos passos seguintes usam só esse método.
10. Acessor público `const std::vector<LottiePage> &GetPages() const`.

## Fora de escopo

JSON, formas reais, interpretação de cores (só guardar a string), texto.

## Critérios de aceite

- Compila (`make -j4` em `verovio/tools`).
- A validação da estrutura acontece em A04, quando a árvore vira JSON.

## Armadilhas

- A regra "forma entra antes do primeiro subgrupo" muda a ordem de pintura
  (formas diretas de um grupo ficam abaixo dos subgrupos). Replicá-la é
  necessário para a paridade.
- `ResumeGraphic` é usado por ligaduras e elementos que atravessam sistemas
  (`src/view_slur.cpp` L47; `src/view_control.cpp` em várias linhas).
- Não guarde ponteiros para elementos de `children` (o vector realoca). Ponteiros
  para `LottieNode` são estáveis porque cada nó vive num `unique_ptr`.

## Notas de execução

- Estrutura de `lottiegeometry.h` seguida como sugerida, sem mudanças de nomes.
- `StartCustomGraphic`/`EndCustomGraphic` também registram `gId` em `m_idMap`
  quando não vazio (ex.: `keyAccid->GetID()` em `view_element.cpp:1164`),
  espelhando o SVG: lá `AppendIdAndClass` grava o atributo `id` tanto para
  `StartGraphic` quanto para `StartCustomGraphic`, e `ResumeGraphic` busca por
  esse atributo sem distinguir a origem. O passo não mencionava isso
  explicitamente, mas é necessário para paridade de comportamento.
- `AddShape` foi implementado conforme o passo, mas ainda não tem nenhum
  chamador (fica para A06+); não gera warning de função não usada por ser
  método de classe, não função estática.
- Build é C++20 (`CMAKE_CXX_STANDARD 20`), então `std::make_unique` está
  disponível e foi usado (já aparece uma vez em `src/object.cpp`).
- Build limpo (`cmake ../cmake && make -j4`, sem warnings novos) e a geração
  de SVG do corpus Grieg continua idêntica (mesmos warnings pré-existentes de
  `tie`/`tstamp`, sem relação com esta mudança). Validação estrutural da IR
  fica para A04, como previsto no critério de aceite.
