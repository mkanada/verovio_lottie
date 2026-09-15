# verovio (Dart FFI bindings)

Dart bindings for the Verovio `Toolkit` — including this fork's native
`.lottie`/dotLottie exporter — built the same way as
[`csa8820/verovio_flutter`](https://github.com/csa8820/verovio_flutter):
a plain C API (`extern "C"`) wraps the C++ `Toolkit` class, compiled into a
shared library, loaded from Dart with `dart:ffi`. See
`../../../docs/plano/E01-bindings-dart-ffi.md` for how this was built and
what it reuses from the rest of `bindings/`.

## Layout

```
lib/
  verovio.dart              public export (VerovioToolkit, VerovioBindings)
  src/verovio_bindings.dart low-level dart:ffi signatures, 1:1 with ../../tools/c_wrapper.h
  src/verovio_toolkit.dart  idiomatic VerovioToolkit class (mirrors ../swift-toolkit/VerovioToolkit.swift)
build_linux_so.sh           builds libverovio.so and copies it here
example/main.dart           CLI-style usage example
test/verovio_toolkit_test.dart  end-to-end test against a real corpus file
```

## Build the native library (Linux)

```sh
./build_linux_so.sh
```

This runs `cmake -DBUILD_AS_LIBRARY=ON` against `../../cmake` (the same
CMake project used by every other binding here), builds into
`../../tools/build-library/`, and copies `libverovio.so` next to this
README so `DynamicLibrary.open('libverovio.so')` finds it when the working
directory is `bindings/dart/`. Re-run it whenever `verovio/src` or
`tools/c_wrapper.*` changes.

## Use it

```sh
dart pub get
dart run example/main.dart ../../../corpus/mei/Grieg_Little_bird_Op43_No4.mei out.lottie
```

```dart
import 'package:verovio/verovio.dart';

final toolkit = VerovioToolkit.withResourcePath('../../data'); // Bravura/Leipzig fonts
toolkit.loadFile('score.mei');
toolkit.renderToDotLottieFile('score.lottie');       // whole score, -t dotlottie
toolkit.renderToDotLottieHighlightFile('page1.lottie', pageNo: 1); // -t dotlottie-highlight
toolkit.dispose();
```

`VerovioToolkit` exposes the full `c_wrapper.h` surface (loading, SVG,
MIDI, PAE, Humdrum conversion, timemap, expansion map, editor actions,
options, and the Lottie/dotLottie renderers), not just the methods shown
above.

## Run the tests

```sh
./build_linux_so.sh   # if you haven't already
dart test
```

The suite loads a real corpus MEI file and exercises the FFI path against
the freshly built `.so`: page count, SVG rendering, and both dotLottie
renderers (checked via the zip `PK` magic bytes on the output file).

## Platform coverage

| Platform | Status |
| --- | --- |
| Linux (x86_64) | Built and tested here (`build_linux_so.sh`, `dart test` — see E01 execution notes) |
| macOS / Windows desktop | Same CMake target (`-DBUILD_AS_LIBRARY=ON`) builds `libverovio.dylib`/`verovio.dll` there; `VerovioToolkit._openLibrary` already picks the right default name. Not built/tested in this environment (no macOS/Windows toolchain here). |
| Android | `../../cmake/CMakeLists.txt` already has `-DBUILD_AS_ANDROID_LIBRARY=ON` (links `liblog`, sets the 16 KB page-size linker flag). No Gradle/JNI project was written here — no Android NDK in this environment to build or verify one. `verovio_flutter`'s `android/` + `build_android_so.sh` is the reference shape to follow (one `.so` per ABI: armeabi-v7a, arm64-v8a, x86_64). |
| iOS | `../iOS/create_ios_framework_headers.sh` and `../swift-core`/`../swift-toolkit` already give a working Swift/XCFramework path reusing the same `c_wrapper.h`. Not exercised here (no Xcode/iOS SDK in this environment). |
| Web (WASM) | Out of scope for this step — see `../../../docs/plano/E00-bindings-opcional.md` (emscripten, still optional/unstarted). |

## Notes

- String returns from the toolkit (`vrvToolkit_get*`/`render*`) point into
  a buffer owned by the C++ `Toolkit` instance (`SetCString`/`GetCString`)
  — `VerovioToolkit` copies them into a Dart `String` and never frees that
  pointer, same as the Swift binding.
- `dotlottie`/`dotlottie-highlight` output is a binary zip; write it to a
  file (`renderToDotLottieFile`/`renderToDotLottieHighlightFile`), the same
  restriction the CLI has (`-o -` to stdout is not supported for these
  formats either).
- One `VerovioToolkit` wraps one native `Toolkit` instance. It is not
  thread-safe — same rule as the C++ class itself.
