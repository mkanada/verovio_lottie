# E01 — Bindings Dart FFI (versão "library" do projeto)

**Depende de:** A12 (pacote `dotlottie` completo), A04 (`lottie` debug JSON) ·
**Decisão necessária:** nenhuma — extensão mecânica de infraestrutura já
existente (`tools/c_wrapper.*`, `cmake/CMakeLists.txt` `BUILD_AS_LIBRARY`),
não abre nenhuma questão de arquitetura do exportador em si.

## Objetivo

Pedido do usuário nesta sessão: gerar a versão "library" do projeto —
importável de outras linguagens via FFI, com o Toolkit completo disponível
— seguindo o padrão do [`csa8820/verovio_flutter`](https://github.com/csa8820/verovio_flutter)
(wrapper C `extern "C"` em cima do `Toolkit` C++ → biblioteca compartilhada
→ camada de bindings na linguagem alvo). Diferente do que E00 cogitava
(bindings JS/Python via emscripten/SWIG), o alvo concreto pedido foi
Dart/FFI (a mesma linguagem do projeto de referência), que já está
disponível neste ambiente (Flutter/Dart instalados) e é buildável e
testável de ponta a ponta em Linux sem depender de NDK/Xcode.

## O que já existia (achado, não construído do zero)

- `verovio/tools/c_wrapper.h`/`.cpp`: wrapper C `extern "C"` completo em
  cima de `Toolkit`, com um padrão consistente (`vrvToolkit_<Método>`,
  `void *tkPtr` como primeiro argumento, retorno de string via
  `Toolkit::SetCString`/`GetCString`). Já é a base dos bindings Go
  (`bindings/go`), Java/SWIG (`bindings/java`), Python/SWIG
  (`bindings/python`) e Swift (`bindings/swift-core` +
  `bindings/swift-toolkit/VerovioToolkit.swift`, este último a referência
  mais próxima do que um binding Dart deveria parecer).
- `verovio/cmake/CMakeLists.txt` L206-L219: opção `-DBUILD_AS_LIBRARY=ON`
  já produz `libverovio.so` (Linux) linkando `tools/c_wrapper.cpp` + todo o
  código-fonte do Verovio (inclusive o exportador dotLottie deste fork,
  que não é um módulo separado). `BUILD_AS_ANDROID_LIBRARY` existe também,
  mas nenhum projeto Android/Gradle deste binding foi criado aqui — não há
  NDK neste ambiente para buildar/validar, então ficou fora de escopo (ver
  "Fora de escopo").
- Convenção de build dir: `tools/build-library` (já no `.gitignore` do
  Verovio vendorizado).

## O que faltava e foi adicionado

`c_wrapper.h`/`.cpp` não exportava nenhum dos métodos de Lottie/dotLottie
do `Toolkit` (`include/vrv/toolkit.h` L384-L427) — só os formatos que já
existiam antes deste fork (SVG, MIDI, PAE, timemap, expansion map). Sem
isso, "o Toolkit tem que estar disponível" (pedido do usuário) ficaria
incompleto: o próprio exportador deste projeto não seria alcançável via
FFI. Adicionadas 5 funções, no mesmo padrão e na mesma posição alfabética
das vizinhas:

| Função C | Método `Toolkit` | Formato CLI equivalente |
| --- | --- | --- |
| `vrvToolkit_renderToDotLottieFile` | `RenderToDotLottieFile` | `-t dotlottie` |
| `vrvToolkit_renderToDotLottieHighlightFile` | `RenderToDotLottieHighlightFile` | `-t dotlottie-highlight` |
| `vrvToolkit_renderToLottie` | `RenderToLottie` | (interno, sem flag CLI própria) |
| `vrvToolkit_renderToLottieAnimation` | `RenderToLottieAnimation` | (interno, sem flag CLI própria) |
| `vrvToolkit_renderToLottieFile` | `RenderToLottieFile` | `-t lottie` |

`bindings/swift-core/swift_c_wrapper.h` só inclui `c_wrapper.h` — nenhuma
mudança necessária lá, as 5 funções já aparecem no módulo Swift de graça.

## Bindings Dart criados

`verovio/bindings/dart/` (novo, ao lado de `bindings/go`, `bindings/java`,
`bindings/python`, `bindings/swift-*`):

- `lib/src/verovio_bindings.dart` — `dart:ffi` puro, um typedef nativo/Dart
  por formato de assinatura, uma classe `VerovioBindings` que resolve as
  67 funções (65 `vrvToolkit_*` + `enableLog`/`enableLogToBuffer`) via
  `DynamicLibrary.lookupFunction`. Espelha `c_wrapper.h` 1:1 e na mesma
  ordem, para diff fácil quando o `c_wrapper.h` mudar.
- `lib/src/verovio_toolkit.dart` — classe `VerovioToolkit` idiomática
  (gerência do ponteiro nativo, conversão `String`↔`Utf8` via
  `package:ffi`/`using(Arena)`), método a método análoga a
  `VerovioToolkit.swift`, cobrindo também os métodos que o binding Swift
  não cobre (`enableLog`, `loadZipDataBuffer`, `renderData`) e os 5 novos
  de Lottie/dotLottie.
- `build_linux_so.sh` — chama o mesmo `cmake -DBUILD_AS_LIBRARY=ON` acima
  e copia `libverovio.so` para dentro do pacote Dart.
- `test/verovio_toolkit_test.dart` — teste de ponta a ponta real (não
  mockado): abre `libverovio.so`, carrega
  `corpus/mei/Grieg_Little_bird_Op43_No4.mei` (que já tem
  `corpus/lottie/Grieg_Little_bird_Op43_No4.lottie` validado), renderiza
  SVG e os dois pacotes `.lottie` (completo e `-highlight`), confere
  assinatura zip (`PK\x03\x04`) nos dois.
- `example/main.dart`, `README.md`, `pubspec.yaml`, `.gitignore`.

## Fora de escopo

- **Android/iOS de verdade** (`.so` por ABI via NDK, `.xcframework` via
  Xcode): nem NDK nem toolchain Apple estão disponíveis neste ambiente
  Linux. `BUILD_AS_ANDROID_LIBRARY` e `bindings/iOS/` já existem no
  Verovio vendorizado (reutilizáveis), mas nenhum script novo foi escrito
  aqui porque não haveria como buildar nem validar — teria sido código não
  testado apresentado como funcionando. Ver `bindings/dart/README.md` para
  o apontamento de como isso seguiria.
- **Empacotamento Flutter (plugin de verdade, isolate dedicado, gestão de
  assets)**: o pedido foi "a versão library... via FFI", não "um plugin
  Flutter"; `VerovioAsyncService`/`VerovioResourceManager` do
  `verovio_flutter` são otimizações de UI (não travar a thread principal,
  empacotar assets no APK) que fazem sentido dentro de um app Flutter, não
  numa biblioteca de baixo nível. Documentado como próximo passo natural
  caso o zywny vá ser um app Flutter (ainda não decidido — ver
  `docs/descricao-do-projeto.md`).
- **Cobertura de bindings Web/JS**: continua em aberto e opcional, é
  exatamente o escopo original do E00 (emscripten), não tocado aqui.

## Critérios de aceite

- `libverovio.so` builda sem erro com `-DBUILD_AS_LIBRARY=ON` e exporta as
  5 novas funções (`nm -D libverovio.so | grep Lottie`).
- `dart analyze` limpo em `bindings/dart/`.
- `dart test` verde: carrega um MEI real, `getPageCount() > 0`, SVG contém
  `<svg`, e os dois `RenderToDotLottie*File` produzem um arquivo com magic
  bytes de zip (`PK\x03\x04`) e tamanho não trivial.

## Notas de execução

Tudo executado e verificado nesta sessão:

- Build (`tools/build-library`, `-DBUILD_AS_LIBRARY=ON -DCMAKE_BUILD_TYPE=Release`):
  sucesso, `libverovio.so` ~20 MB, as 5 símbolos novos presentes em
  `nm -D`.
- `dart pub get` + `dart analyze bindings/dart`: sem erros/avisos.
- `dart test` (4 casos, rodando contra o `.so` recém-buildado): todos
  passaram, incluindo os dois testes que geram `.lottie` de verdade via
  FFI (whole-score e highlight de uma página) a partir do
  `Grieg_Little_bird_Op43_No4.mei` do corpus.
- `dart run example/main.dart`: roda de ponta a ponta contra o corpus,
  produz um `.lottie` de ~830 KB para essa peça (2 páginas).

Nenhuma mudança foi necessária em `svg`/`midi`/`timemap`/outros formatos
já expostos — só adição, no fim do arquivo (posição alfabética) das 5
funções novas.
