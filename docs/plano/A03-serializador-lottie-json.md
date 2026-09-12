# A03 — Serializador `LottieWriter` (IR → JSON Lottie)

**Depende de:** A02 · **Decisão:** nenhuma

## Objetivo

Converter as páginas da IR numa animação Lottie válida. O writer já deve suportar
todos os tipos de forma da IR (Path, Rect, Ellipse, fill, stroke, dash), para que
os passos seguintes só precisem preencher a IR.

## Ler antes (só isto)

- `verovio/include/vrv/lottiegeometry.h` (criado em A02).
- `verovio/src/svgdevicecontext.cpp` L150-L184 — `Commit` (tamanho em px).
- `verovio/src/svgdevicecontext.cpp` L523-L550 — `StartPage` (viewBox interno e
  translate das margens).
- `verovio/src/svgdevicecontext.cpp` L1295-L1308 — `GetColor` (cores inteiras).

## Arquivos

- Criar: `verovio/include/vrv/lottiewriter.h`, `verovio/src/lottiewriter.cpp`
  (rodar `cmake ../cmake` de novo).

## API sugerida

```cpp
class LottieWriter {
public:
    // Uma animação com uma camada por página.
    static std::string WriteAnimation(const std::vector<const LottiePage *> &pages, const std::string &name);
};
```

## O que fazer

1. **Timeline provisória**: `fr` 30, `ip` 0, `op` = número de páginas. A camada
   da página k (0-based) tem `ip` k e `op` k+1, então o frame k mostra a página
   k+1. Isso permite renderizar qualquer página com
   `compare lottie-to-png --frame k`. É provisório até D-LAYOUT-PAGINAS (fase C).
2. **Tamanho** (por página; a composição usa o máximo): como o ramo não-mm de
   `Commit` (L157-L175) — se `baseWidth` e `baseHeight` ≠ 0, usar a base; senão
   `Wpx = ceil(width * userScaleX)`, `Hpx = ceil(height * userScaleY)`.
3. **Transformação da camada** (reproduz o SVG):
   - viewBox interno: `VW = int(width * viewBoxFactor)`,
     `VH = int(contentHeight * viewBoxFactor)` (L534-L536, com o truncamento `int`);
   - o `<svg>` interno ocupa `Wpx × Hpx` com `preserveAspectRatio` padrão
     (xMidYMid meet): `S = min(Wpx/VW, Hpx/VH)`, `tx = (Wpx − VW·S)/2`,
     `ty = (Hpx − VH·S)/2`;
   - em seguida `translate(originX, originY)` (L549-L550);
   - logo, um ponto p da IR vai para `(tx + S·(p.x + originX), ty + S·(p.y + originY))`;
   - na camada: `"a": [0,0,0]`, `"s": [S·100, S·100, 100]`,
     `"p": [tx + S·originX, ty + S·originY, 0]`.
4. **Esqueleto do JSON**:

   ```json
   {"v":"5.7.0","fr":30,"ip":0,"op":N,"w":W,"h":H,"nm":"verovio","ddd":0,"assets":[],"markers":[],
    "layers":[{"ddd":0,"ind":1,"ty":4,"nm":"page-1","sr":1,
      "ks":{"o":{"a":0,"k":100},"r":{"a":0,"k":0},"p":{"a":0,"k":[px,py,0]},
            "a":{"a":0,"k":[0,0,0]},"s":{"a":0,"k":[s,s,100]}},
      "ao":0,"shapes":[],"ip":0,"op":1,"st":0,"bm":0}]}
   ```

5. **Nó → grupo**: `{"ty":"gr","nm":<id, ou className se não houver id>,"mn":<className>,"it":[<filhos>, <tr>]}`.
   - **Ordem invertida**: no SVG o que vem depois é pintado por cima; no Lottie o
     primeiro item de `it` (e a primeira camada de `layers`) fica por cima.
     Escreva os filhos do último para o primeiro.
   - `tr` sempre por último:
     `{"ty":"tr","p":{"a":0,"k":[0,0]},"a":{"a":0,"k":[0,0]},"s":{"a":0,"k":[100,100]},"r":{"a":0,"k":0},"o":{"a":0,"k":100},"sk":{"a":0,"k":0},"sa":{"a":0,"k":0}}`.
     Com rotação: `a` e `p` = `rotationOrigin`, `r` = `rotation` (sinal a
     confirmar na fase D; só 2 chamadas no `View`).
   - Grupo `hidden`: omitir (o SVG usa `visibility="hidden"`, que não pinta).
6. **Forma → grupo próprio**: `{"ty":"gr","it":[<geometria...>, <st?>, <fl?>, <tr>]}`.
   - Path: um item por subpath:
     `{"ty":"sh","ks":{"a":0,"k":{"i":[[x,y],...],"o":[...],"v":[...],"c":true}}}`.
     Os subpaths do mesmo grupo compartilham o fill (é o que faz os buracos das
     cabeças de mínima/semibreve).
   - Rect: `{"ty":"rc","p":{"a":0,"k":[cx,cy]},"s":{"a":0,"k":[w,h]},"r":{"a":0,"k":radius}}`.
   - Ellipse: `{"ty":"el","p":{"a":0,"k":[cx,cy]},"s":{"a":0,"k":[w,h]}}`.
   - Stroke **antes** do fill (para ficar por cima, como no SVG — confirmar em A06):
     `{"ty":"st","c":{"a":0,"k":[r,g,b,1]},"o":{"a":0,"k":op*100},"w":{"a":0,"k":w},"lc":lc,"lj":lj,"ml":4}`;
     se `dashLength > 0`, acrescentar
     `"d":[{"n":"d","nm":"dash","v":{"a":0,"k":dash}},{"n":"g","nm":"gap","v":{"a":0,"k":gap}}]`.
     Mapear `LINECAP_DEFAULT/BUTT → 1`, `ROUND → 2`, `SQUARE → 3`;
     `LINEJOIN_DEFAULT/MITER/MITER_CLIP/ARCS → 1`, `ROUND → 2`, `BEVEL → 3`
     (padrões do SVG: butt e miter).
   - Fill: `{"ty":"fl","c":{"a":0,"k":[r,g,b,1]},"o":{"a":0,"k":op*100},"r":1}`
     (`r: 1` = nonzero, a regra padrão do SVG).
7. **Cores**: resolver a herança durante a escrita, passando a cor corrente (RGB
   int) na recursão; começa em preto (o SVG põe `color="black"` na raiz, L527).
   `colorCss` não vazio: nesta etapa aceitar só `#RRGGBB` e `#RGB` (A06 completa o
   parser). Cor int da forma: `COLOR_NONE` → herdada; senão, mesma tabela de
   `GetColor` (L1297-L1306), convertida para componentes 0..1.
8. **Números**: `std::ostringstream` com `imbue(std::locale::classic())` (evita
   vírgula decimal), no máximo 3 casas decimais, sem zeros à direita. Não usar
   `jsonxx` para montar o documento (copia objetos grandes a cada `<<`).
9. Escapar `"` e `\` nas strings (`nm`, `mn`).

## Fora de escopo

Toolkit e CLI (A04), parser de cores CSS completo (A06), markers e state machine (fase C).

## Critérios de aceite

- Compila. A validação do JSON acontece em A04.

## Notas de execução

_(preencher ao executar)_
