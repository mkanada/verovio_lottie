# A12 — Pacote `.lottie` com todas as páginas + CLI `dotlottie`

**Depende de:** A10, A11 · **Decisão:** D-CLI (nome `dotlottie`, confirmado em A04)

## Objetivo

Gerar um único `.lottie` por música, com todas as páginas como camadas, pelo
`Toolkit` e pela CLI.

## Ler antes (só isto)

- `verovio/src/toolkit.cpp` L1674-L1730 (`RenderToDeviceContext`), L1799-L1816 (`RenderToSVGFile`).
- `verovio/include/vrv/toolkit.h` L196 (`GetPageCount`), L355-L380 (estilo da documentação).
- `verovio/tools/main.cpp` L350-L371 (ramo do SVG) e o ramo `lottie` criado em A04.
- `docs/plano/README.md` — seção "Referência rápida: pacote dotLottie v2".
- `verovio/include/vrv/lottiewriter.h`, `verovio/include/vrv/filereader.h` (`ZipFileWriter`).

## Arquivos

- Modificar: `verovio/include/vrv/toolkit.h`, `verovio/src/toolkit.cpp`,
  `verovio/tools/main.cpp`, `compare/scripts/compare-page.sh`.

## O que fazer

1. `Toolkit`:
   - `std::string RenderToLottieAnimation();` — JSON com todas as páginas (útil
     para depuração e bindings).
   - `bool RenderToDotLottieFile(const std::string &filename);` (`@remark nojs`).
   - Ambos usam **um único** `LottieDeviceContext` com `SetResources` e um laço
     `p = 1..GetPageCount()` chamando `RenderToDeviceContext(p, &dc)`. Cada chamada
     vira uma página da IR, com as dimensões daquela página.
   - Serializar com `LottieWriter::WriteAnimation(todas as páginas, "score")`.
2. Empacotar com `ZipFileWriter`:
   - `manifest.json`:
     `{"version":"2","generator":"Verovio <GetVersion()> (verovio_lottie)","animations":[{"id":"score"}],"initial":{"animation":"score"}}`
     — ainda sem `stateMachines`;
   - `a/score.json`: a animação.
3. CLI: acrescentar `"dotlottie"` à lista e à mensagem de formatos; ramo que
   escreve `<outfile>.lottie` e ignora `--page`/`-a` (decisão: um arquivo por
   música). Saída padrão (`-o -`) não é suportada para esse formato binário:
   mensagem de erro e `exit(1)`.
4. `compare/scripts/compare-page.sh`: aceitar também `.lottie`, renderizando a
   página N com `--frame N-1` (timeline provisória de A03).

## Fora de escopo

State machine, markers, animações (fase C).

## Critérios de aceite

- `verovio/tools/verovio -t dotlottie corpus/mei/Chopin_Etude_Op10_No9.mei -o compare/out/chopin --resource-path verovio/data`
  gera `compare/out/chopin.lottie`.
- `unzip -t compare/out/chopin.lottie` passa; `unzip -l` lista `manifest.json` e `a/score.json`.
- Essa peça tem **4 páginas** no layout padrão:
  `compare/target/release/compare lottie-to-png compare/out/chopin.lottie /tmp/p4.png --width 2100 --height 2970 --frame 3`
  mostra a página 4, igual ao SVG da página 4 (via `compare-page.sh ... 4`).
- O frame 0 do `.lottie` e o PNG gerado por `-t lottie` da página 1 são idênticos
  (`compare diff` com 0 pixels diferentes).

## Notas de execução

_(preencher ao executar)_
