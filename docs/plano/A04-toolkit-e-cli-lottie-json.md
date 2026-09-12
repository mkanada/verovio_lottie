# A04 — `Toolkit::RenderToLottie` + formato `lottie` na CLI

**Depende de:** A03 · **Decisão:** D-CLI — antes de começar, confirme com o
usuário os nomes `lottie` (JSON de uma página, para depuração) e `dotlottie`
(pacote final, usado em A12).

## Objetivo

Expor a renderização de uma página como JSON Lottie pelo `Toolkit` e pela CLI,
para que cada passo seguinte possa ser verificado visualmente.

## Ler antes (só isto)

- `verovio/include/vrv/toolkit.h` L355-L380 — documentação de `RenderToSVG`/`RenderToSVGFile` (imitar).
- `verovio/src/toolkit.cpp` L45 (includes), L1674-L1730 (`RenderToDeviceContext`),
  L1740-L1816 (`RenderToSVG`, `RenderToSVGFile`).
- `verovio/include/vrv/toolkitdef.h` L13-L35 — `FileFormat`.
- `verovio/src/options.cpp` L1987-L2027 — `SetOutputTo`.
- `verovio/tools/main.cpp` L284-L292, L312-L315, L350-L371.

## Arquivos

- Modificar: `verovio/include/vrv/toolkitdef.h`, `verovio/src/options.cpp`,
  `verovio/include/vrv/toolkit.h`, `verovio/src/toolkit.cpp`, `verovio/tools/main.cpp`

## O que fazer

1. `toolkitdef.h`: acrescentar `LOTTIE` e `DOTLOTTIE` **no fim** do enum (depois
   de `SERIALIZATION`), para não mudar valores já expostos a bindings.
2. `Options::SetOutputTo`: `"lottie"` → `LOTTIE`, `"dotlottie"` → `DOTLOTTIE`.
3. `Toolkit` (comentários no estilo existente; `@remark nojs` no método de arquivo):
   - `std::string RenderToLottie(int pageNo = 1);` — `ResetLogBuffer()`; cria
     `LottieDeviceContext`; `SetResources(&m_doc.GetResources())` (como L1747);
     `RenderToDeviceContext(pageNo, &dc)`; se falhar, retorna `""`; senão
     `LottieWriter::WriteAnimation({ &dc.GetPages().front() }, "verovio")`.
   - `bool RenderToLottieFile(const std::string &filename, int pageNo = 1);` —
     igual a `RenderToSVGFile`.
   - Incluir `lottiedevicecontext.h` e `lottiewriter.h` em `toolkit.cpp` (perto de L45).
4. `tools/main.cpp`:
   - acrescentar `"lottie"` à lista de formatos (L284-L285) e à mensagem de erro (L288-L289);
   - novo ramo copiando o laço do SVG (L350-L371) com extensão `.json` e
     `RenderToLottie`/`RenderToLottieFile`;
   - **não** acrescentar `lottie` à condição de L313 (ela desliga a paginação).

## Fora de escopo

Pacote `.lottie`, várias páginas num mesmo arquivo (A12).

## Critérios de aceite

Da raiz do repositório, com `mkdir -p /tmp/vrv-a04`:

- `verovio/tools/verovio -t lottie corpus/mei/Grieg_Little_bird_Op43_No4.mei -o /tmp/vrv-a04/grieg --resource-path verovio/data`
  gera `/tmp/vrv-a04/grieg.json`.
- `python3 -m json.tool /tmp/vrv-a04/grieg.json > /dev/null` não dá erro.
- `w`/`h` do JSON iguais a `width`/`height` do `<svg>` gerado com `-t svg` para o
  mesmo arquivo (hoje 2100×2970).
- `compare/target/release/compare lottie-to-png /tmp/vrv-a04/grieg.json /tmp/vrv-a04/grieg.png --width 2100 --height 2970`
  gera um PNG (vazio nesta etapa — esperado). Se aparecer um aviso de
  `set_frame`/`render` "sem efeito", é o comportamento inofensivo documentado em
  `compare/README.md`.
- Com `-a`, a peça (2 páginas) gera `grieg_001.json` e `grieg_002.json`.
- A saída `-t svg` continua igual.

## Armadilhas

- `main.cpp` chama `toolkit.SetOutputTo(optarg)` (L209) ignorando o retorno: se
  faltar o item 2, só aparece `Output format ... is not supported` no log.

## Notas de execução

_(preencher ao executar)_
