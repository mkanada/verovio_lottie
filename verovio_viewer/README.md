# verovio_viewer

Pacote Flutter compartilhado para exibir partituras animadas (`.lottie`)
geradas pelo exportador dotLottie do [verovio_lottie](../README.md) — usado
pelo `compare/` (deste repositório, quando precisar de um modo de inspeção
visual interativo, além do diff automático por FFI) e pelo
[zywny](https://github.com/mkanada/zywny).

Embute:

- **`dotlottie_flutter`** (o runtime oficial da LottieFiles) — via
  [`ScoreViewer`], um widget que serve o `.lottie` localmente (contornando a
  limitação de `DotLottieView` no desktop, que só carrega `url`/`asset`/
  `json`, nunca um caminho de arquivo — ver `lib/src/lottie_file_server.dart`)
  e posiciona a câmera na página correta.
- **As fontes usadas pelo Verovio para renderizar a partitura** — Liberation
  (texto comum) e os conjuntos SMuFL (Bravura, Leipzig, Gootville, Petaluma,
  Leland), o mesmo `resourcePath` que o motor nativo do Verovio espera. Ver
  [`verovioResourcePath()`].

## Por que um zip, não arquivos crus

`assets/verovio_data.zip` é gerado por `tool/build_assets.sh` a partir de
`../verovio/data` deste repositório e **não é versionado** (regenerável,
derivado de conteúdo já vendorizado). Empacotado como um único zip porque o
bundler de assets do Flutter lista um diretório sem recursão: uma pasta
`assets/verovio_data/` declarada direto no `pubspec.yaml` faria as subpastas
por glifo (`Bravura/`, `Leipzig/`, ...) desaparecerem silenciosamente — bug
real, já contornado no zywny via `install(DIRECTORY ...)` no CMake antes
deste pacote existir. `verovioResourcePath()` descompacta o zip em runtime
(uma vez por processo, cacheado em `getApplicationSupportDirectory()`) e
devolve o diretório resultante.

Rode `tool/build_assets.sh` sempre que `../verovio/data` mudar (nova versão
de fonte, novo glifo, etc.) e antes do primeiro `flutter pub get`/build de
qualquer app que dependa deste pacote.

## Uso

```dart
import 'package:verovio_viewer/verovio_viewer.dart';

// 1. Resolver o resourcePath ANTES de chamar o Verovio nativo para gerar o
//    .lottie (este pacote não embrulha o pipeline de render em si — cada
//    app chama o Verovio com seu próprio binding/isolate, como já faz o
//    zywny em lib/verovio_render.dart).
final resourcePath = await verovioResourcePath();

// 2. Exibir o .lottie já gerado.
ScoreViewer(
  lottiePath: outputPath,
  onControllerReady: (controller) => _controller = controller,
  onReady: () => setState(() => _status = 'pronto ✓'),
  onError: (e) => setState(() => _status = 'erro: $e'),
)

// Navegação de página e acesso ao DotLottieViewController (play/pause/
// speed/slots/eventos de destaque) via a GlobalKey<ScoreViewerState>, ou o
// callback onControllerReady acima:
await scoreViewerKey.currentState?.goToPage(1);
```

## O que fica de fora (de propósito)

O pipeline "partitura → `.lottie`" (bindings do Verovio, localização de
`libverovio.so`, isolate de render) **não** faz parte deste pacote — cada
app que gera o `.lottie` continua com seu próprio código para isso (ver
`lib/verovio_render.dart` + `lib/native_paths.dart` no zywny). `verovio_viewer`
cobre só o lado de exibição (`dotlottie_flutter` + fontes), que é o que se
repete entre `compare` e o zywny.
