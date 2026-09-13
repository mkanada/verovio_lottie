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

- `Toolkit::RenderToLottieAnimation()`/`RenderToDotLottieFile()` implementados em
  `toolkit.h`/`toolkit.cpp` como planejado: um único `LottieDeviceContext`
  reaproveitado no laço `p = 1..GetPageCount()`, `LottieWriter::WriteAnimation`
  já suportava múltiplas páginas desde A03 (um layer por página, `ip`/`op` de
  1 frame cada — por isso `--frame N-1` mostra a página N). `manifest.json` e
  `a/score.json` empacotados com `ZipFileWriter` (A11), sem `stateMachines`.
- **Bug encontrado e corrigido**: `View::DrawCurrentPage` (não alterado,
  `view_page.cpp` L91-92) lê a origem lógica atual e subtrai as margens da
  página (`origin = dc->GetLogicalOrigin(); dc->SetLogicalOrigin(origin.x -
  marginLeft, ...)`). Isso é inofensivo quando cada página usa um
  `DeviceContext` novo (origem sempre começa em 0,0 — é o que `RenderToSVG`/
  `RenderToLottie` fazem), mas ao reaproveitar **um único**
  `LottieDeviceContext` para todas as páginas (arquitetura pedida por este
  passo), a origem acumulava a margem de cada página anterior, deslocando o
  conteúdo das páginas 2+ (constatado comparando o layer da página 4 dentro
  do pacote multipágina — `p:[0,0,0]` — contra o JSON de página única
  equivalente — `p:[50,50,0]`; motivo do `Command::LottieToPng` do `compare`
  não bater com o SVG para páginas != 1). Corrigido com um
  `deviceContext->SetLogicalOrigin(0, 0)` no início de
  `Toolkit::RenderToDeviceContext` (antes de `m_view.DrawCurrentPage`), que
  reseta a origem a cada página e é um no-op para os usos de página única já
  existentes (origem já nasce em 0,0 num DC novo). Não mexe em
  `SvgDeviceContext`/`View`.
- `compare/scripts/compare-page.sh`: aceita `.lottie` como `<arquivo>`. Como
  um pacote dotLottie não carrega a partitura de origem, não dá pra
  (re)gerar o SVG a partir dele — o modo `.lottie` espera que
  `<nome>-p<página>-svg.png` já exista em `compare/out/` (de uma execução
  anterior do script com o arquivo de partitura original) para ler
  largura/altura e servir de referência no `diff`; se não existir, o script
  avisa e sai com erro em vez de falhar tentando extrair dimensões de um
  arquivo inexistente. Só faz a etapa "Lottie -> PNG" (com
  `--frame $((PAGE-1))`) e o `diff`; não regenera o SVG nem chama o
  `verovio`.
- Critérios de aceite validados com
  `corpus/mei/Chopin_Etude_Op10_No9.mei` (4 páginas, 2100x2970):
  `unzip -t`/`unzip -l` passam; página 4 do pacote (`--frame 3`) bate com o
  SVG da página 4 (0,1483% de diferença — igual ao `-t lottie -p 4` isolado,
  resíduo conhecido do D-TEXTO/texto comum ainda não desenhado); frame 0 do
  pacote e o PNG de `-t lottie` da página 1 são **pixel-idênticos** (0
  diferenças, tolerância 0).
- `-o -` com `-t dotlottie` corretamente rejeitado (`exit(1)` com mensagem);
  `-p`/`-a` são ignorados nesse formato (não há laço de páginas no ramo da
  CLI, um arquivo por música).
- README (`docs/plano/README.md`): corrigida a tabela de passos — A09, A10 e
  A11 já estavam implementados no código (commits `64ef3c6`, `27066a2`) mas
  ainda constavam como "pendente"; marcados como "concluído" junto com A12.
