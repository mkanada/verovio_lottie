# D04 — `RotateGraphic` (confirmar sinal e pivô)

**Depende de:** — · **Decisão necessária:** nenhuma — passo de verificação/
correção pontual, não de arquitetura nova.

## Objetivo

`LottieDeviceContext::RotateGraphic` **já está implementado**
(`verovio/src/lottiedevicecontext.cpp:705-716`: guarda `rotation`/
`rotationOrigin` no `LottieNode` corrente) e `LottieWriter` já emite a
transformação correspondente (`WriteTransformWithRotation`,
`verovio/src/lottiewriter.cpp:247-255`, usada em L508). Isto **não é um
passo de implementação do zero** — é confirmar empiricamente (a) o sinal do
ângulo e (b) o pivô, porque **nenhuma peça do corpus atual exercita
`RotateGraphic`** (confirmado: `grep -l "<arpeg\|<gliss" corpus/mei/*.mei`
não bate nada), então essa parte do exportador nunca foi validada
visualmente, só escrita por leitura de código.

## Ler antes (só isto)

- `verovio/src/svgdevicecontext.cpp` L475-482 (`SvgDeviceContext::RotateGraphic`) —
  referência: `transform="rotate(%f %d,%d)"` (SVG puro).
- `verovio/src/lottiedevicecontext.cpp` L705-716 (`LottieDeviceContext::RotateGraphic`,
  já implementado — só guarda o valor, sem inverter sinal).
- `verovio/src/lottiewriter.cpp` L247-255 (`WriteTransformWithRotation`) e
  L508 (onde é usada por grupo, condicionado a `node.hasRotation`).
- Os dois únicos chamadores:
  - `verovio/src/view_control.cpp` L1519-1597 (`DrawArpeg`), ângulo fixo
    `const int angle = -90` em L1586, pivô = `Point(ToDeviceContextX(x),
    ToDeviceContextY(y))` em L1587 — comentário no código diz "rotate
    counter clockwise" (comentário do Verovio original, não verificado
    contra a convenção do Lottie).
  - `verovio/src/view_control.cpp` L2149-2290 (`DrawGliss`), ângulo
    calculado em L2252: `RadToDeg(atan2(y1 - y2, x2 - x1))`, pivô =
    `Point(ToDeviceContextX(x1), ToDeviceContextY(y1))` em L2254.
- `docs/plano/README.md`, seção "Convenções" — "eixo y já apontando para
  baixo" quando os dados chegam no `DeviceContext` (`src/view.cpp` L85-92
  já inverteu). Ou seja, tanto `SvgDeviceContext` quanto
  `LottieDeviceContext` recebem o **mesmo** `angle`, no **mesmo** sistema de
  coordenadas (y para baixo) — é esse fato que sustenta a hipótese abaixo.

## Hipótese (a confirmar, não pressupor)

SVG `rotate(angle, cx, cy)` e a propriedade `r` de uma camada/grupo Lottie
usam a **mesma convenção**: ângulo positivo = sentido horário, em um
sistema de coordenadas com y para baixo (mesma convenção do After Effects,
que o Lottie segue). Como os dois `DeviceContext`s recebem o mesmo `angle`
já calculado no mesmo espaço de coordenadas (ver "Ler antes"), a hipótese é
que **nenhuma inversão de sinal é necessária** — o `RotateGraphic` de hoje
já passa `angle` direto pra `WriteTransformWithRotation` sem tocar o sinal.
O pivô também já parece correto por construção: `WriteTransformWithRotation`
define `p = a = origin`, e como todo vértice de shape neste exportador já
chega em coordenadas absolutas de página (sem deslocamento de grupo — ver
"Ler antes" de D01/D02), a transformação Lottie
`renderPonto = R(r)·(localPonto − a) + p` com `a = p = origin` rotaciona
exatamente em torno de `origin`, que é o mesmo pivô do SVG. **Isto não foi
testado visualmente** — é dedução a partir do código, o mesmo tipo de
verificação que este passo existe para fazer de verdade.

## O que fazer

1. Criar um MEI mínimo em `compare/out/d04-rotacao-teste.mei` com pelo
   menos um `<arpeg>` (cobre o caso de ângulo fixo `-90`) sobre um acorde de
   2+ notas, e idealmente também um `<gliss>` entre duas notas em alturas
   diferentes (cobre o ângulo calculado por `atan2`, incluindo um caso
   assimétrico onde erro de sinal ficaria visualmente óbvio — ex. glissando
   claramente ascendente ou descendente, não horizontal).
2. `compare/scripts/compare-page.sh compare/out/d04-rotacao-teste.mei 1` e
   inspecionar visualmente o PNG do Lottie lado a lado com o do SVG (crop
   na região do arpeggio/glissando).
3. **Se a rotação já bater** (hipótese confirmada): nenhuma mudança de
   código — só documentar nas notas de execução que foi verificado, com o
   MEI de teste preservado em `compare/out/` como regressão futura (fora do
   git, mas registrado).
4. **Se estiver espelhada/invertida**: corrigir com uma mudança local e
   isolada — inverter o sinal só em `LottieDeviceContext::RotateGraphic`
   (`rotation = -angle` em vez de `rotation = angle`, L714), não em
   `WriteTransformWithRotation` (que é genérico e não deveria carregar
   conhecimento de onde o ângulo veio) nem nos chamadores em
   `view_control.cpp` (que são compartilhados com o `SvgDeviceContext` e já
   validados lá).
5. **Se o pivô estiver errado** (forma rotaciona em torno do ponto errado,
   não só com sinal trocado): o bug mais provável é a ordem de
   `p`/`a`/`r` dentro do objeto de transform do Lottie (schema `"tr"` do
   After Effects: a ordem de aplicação é escala → rotação em torno de `a` →
   translada por `p - a`) — conferir contra a especificação informal do
   Lottie (`lottiefiles/lottie-docs` ou similar, fora deste repo) antes de
   mexer, já que `WriteTransformWithRotation` também é usada implicitamente
   como padrão de referência por qualquer rotação futura.

## Fora de escopo

Rotação animada (todo o mecanismo de animação já é state machine + fade de
cor/câmera, nunca rotação); qualquer `RotateGraphic` chamado fora dos dois
sites já mapeados (não há outros no `View` hoje).

## Critérios de aceite

- MEI de teste com `<arpeg>`: o símbolo de arpeggio (linha ondulada
  vertical, glifo `wiggleArpeggiatoUp`/`Down`) aparece na mesma orientação e
  posição no Lottie e no SVG.
- MEI de teste com `<gliss>` (se incluído): a linha ondulada de glissando
  segue a mesma diagonal (mesmo sentido ascendente/descendente) nos dois.
- `compare diff` com tolerância 32 (mesma do resto do projeto) não mostra
  divergência estrutural na região do símbolo rotacionado (só ruído de
  antialiasing, categoria 2 de `relatorio-paridade.md`, é aceitável).
- Corpus real (`compare-corpus.sh`) continua passando sem regressão — nenhum
  arpeggio/glissando real nele, então isso só confirma que a mudança (se
  houver) não quebrou nada mais.

## Notas de execução

**Hipótese confirmada — nenhuma mudança de código.**

MEI de teste criado em `compare/out/d04-rotacao-teste.mei` (fora do git, como
todo `compare/out/`, mas registrado aqui para reprodução futura): compasso 1
com um acorde de 4 notas + `<arpeg>` (ângulo fixo `-90`, caminho de
`DrawArpeg`); compasso 2 com `<gliss>` claramente ascendente (dó4→dó5);
compasso 3 com `<gliss>` claramente descendente (dó5→dó4), cobrindo os dois
sentidos de `atan2` em `DrawGliss`.

`compare/scripts/compare-page.sh compare/out/d04-rotacao-teste.mei 1`:

- Símbolo de arpeggio (linha ondulada vertical à esquerda do acorde):
  posição e orientação **idênticas** em recorte lado a lado do SVG e do
  Lottie (mesma coluna de pixels, mesma forma).
- As duas linhas de glissando (ascendente e descendente) aparecem no
  **mesmo sentido diagonal** nas duas renderizações — nenhuma espelhada.
- `compare diff --tolerance 32`: 13593/6237000 pixels (0,2179%), dentro da
  faixa de ruído de antialiasing já observada no projeto (categoria 2 de
  `relatorio-paridade.md`; comparável à média 0,2290% medida em D02 pós-
  rodapé). Inspeção do PNG de diff mostra só pontos isolados espalhados,
  sem mancha estrutural na região do arpeggio/gliss.

Conclusão: `angle` chega ao `LottieDeviceContext::RotateGraphic` no mesmo
sistema de coordenadas (y para baixo) usado pelo `SvgDeviceContext`, e o
pivô de `WriteTransformWithRotation` (`p = a = origin`) já rotaciona em
torno do ponto certo. Nenhuma inversão de sinal nem correção de pivô foi
necessária. Não rodei `compare-corpus.sh`: como nenhum arquivo de código
foi alterado, não há risco de regressão a verificar.
