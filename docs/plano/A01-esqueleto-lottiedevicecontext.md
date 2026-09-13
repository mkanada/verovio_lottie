# A01 — Esqueleto do `LottieDeviceContext`

**Depende de:** nada · **Decisão:** nenhuma

## Objetivo

Criar a classe `LottieDeviceContext` (subclasse de `DeviceContext`) que compila e
implementa todos os métodos puros como no-op. Ainda não é usada por ninguém.

## Ler antes (só isto)

- `verovio/include/vrv/devicecontext.h` L99-L337 — métodos virtuais (os `= 0` são obrigatórios).
- `verovio/include/vrv/bboxdevicecontext.h` — exemplo enxuto de subclasse.
- `verovio/include/vrv/vrvdef.h` L285-L290 — `ClassId` dos device contexts.
- `verovio/src/svgdevicecontext.cpp` L596-L605 — `SetLogicalOrigin` / `GetLogicalOrigin`.
- `verovio/include/vrv/svgdevicecontext.h` L118 — `ApplyOffset()`.

## Arquivos

- Modificar: `verovio/include/vrv/vrvdef.h`
- Criar: `verovio/include/vrv/lottiedevicecontext.h`, `verovio/src/lottiedevicecontext.cpp`

## O que fazer

1. Em `vrvdef.h`, acrescentar `LOTTIE_DEVICE_CONTEXT,` logo depois de
   `SVG_DEVICE_CONTEXT,` (L288).
2. Header com `class LottieDeviceContext : public DeviceContext`:
   - Construtor `LottieDeviceContext()` chamando `DeviceContext(LOTTIE_DEVICE_CONTEXT)`;
     destrutor virtual.
   - `override` de **todos** os métodos `= 0`: `SetBackground`,
     `SetBackgroundImage`, `SetBackgroundMode`, `SetTextForeground`,
     `SetTextBackground`, `SetLogicalOrigin`, `GetLogicalOrigin`,
     `DrawQuadBezierPath`, `DrawCubicBezierPath`, `DrawCubicBezierPathFilled`,
     `DrawBentParallelogramFilled`, `DrawCircle`, `DrawEllipse`,
     `DrawEllipticArc`, `DrawLine`, `DrawPolyline`, `DrawPolygon`,
     `DrawRectangle`, `DrawRotatedText`, `DrawRoundedRectangle`, `DrawText`,
     `DrawMusicText`, `DrawSpline`, `DrawGraphicUri`, `DrawSvgShape`,
     `DrawBackgroundImage`, `StartText`, `EndText`, `MoveTextTo`,
     `MoveTextVerticallyTo`, `StartGraphic`, `EndGraphic`, `ResumeGraphic`,
     `EndResumedGraphic`, `RotateGraphic`, `StartPage`, `EndPage`.
   - Copie as assinaturas **exatamente** de `devicecontext.h`, inclusive os
     argumentos padrão: `DrawText(..., int x = VRV_UNSET, int y = VRV_UNSET, int width = VRV_UNSET, int height = VRV_UNSET)`,
     `DrawPolyline(int n, Point points[], bool close = false)`,
     `StartGraphic(..., GraphicID graphicID = PRIMARY, bool prepend = false)`
     (na base o parâmetro se chama `preprend`; o nome não importa).
   - `bool ApplyOffset() override { return true; }` — **obrigatório**: sem isso
     `View::CalcOffset` ignora os deslocamentos (`src/view.cpp` L188-L196) e as
     posições ficam diferentes das do SVG.
   - Membros `int m_originX = 0;` e `int m_originY = 0;`.
3. No `.cpp`, corpos vazios, exceto `SetLogicalOrigin` (`m_originX = -x; m_originY = -y;`)
   e `GetLogicalOrigin` (`return Point(m_originX, m_originY);`), iguais aos do SVG.
4. Rodar `cmake ../cmake` antes do `make` (arquivo `.cpp` novo).

## Fora de escopo

Estruturas de dados, JSON, Toolkit, CLI.

## Critérios de aceite

- `cd verovio/tools && cmake ../cmake && make -j4` compila sem erros nem warnings
  novos em `lottiedevicecontext.*`.
- `mkdir -p /tmp/vrv-a01 && verovio/tools/verovio -t svg corpus/mei/Grieg_Little_bird_Op43_No4.mei -o /tmp/vrv-a01/grieg --resource-path verovio/data`
  continua gerando o SVG.

## Armadilhas

- `vrvdef.h` é incluído em quase tudo: a recompilação vai demorar — é normal.
- Mantenha o novo id ao lado de `SVG_DEVICE_CONTEXT` (há outros ids depois,
  como `CUSTOM_DEVICE_CONTEXT`).

## Notas de execução

- Não havia `.clang-format` nem binário `clang-format` disponíveis neste
  ambiente (apesar do README do plano citar o primeiro); os arquivos novos
  foram formatados manualmente imitando `svgdevicecontext.h`/`.cpp`.
- `LottieDeviceContext()` chama `DeviceContext(LOTTIE_DEVICE_CONTEXT)`
  diretamente na lista de inicialização (sem corpo), igual ao padrão usado
  por `SvgDeviceContext`.
- `cmake ../cmake && make -j4` reconfigurou e recompilou o projeto inteiro
  nesta primeira execução (sem build incremental prévio nesta máquina); um
  segundo `make -j4` após tocar só em `lottiedevicecontext.cpp` recompila
  em incremental normalmente e não gerou warnings.
- Critérios de aceite confirmados: build limpo e
  `verovio -t svg corpus/mei/Grieg_Little_bird_Op43_No4.mei -o /tmp/vrv-a01/grieg --resource-path verovio/data`
  continua gerando `grieg.svg` normalmente (só os warnings pré-existentes de
  `tie`/`tstamp`, sem relação com esta mudança).
