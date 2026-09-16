import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

import 'verovio_bindings.dart';

/// Idiomatic Dart wrapper around the native Verovio `Toolkit`, loaded via
/// `dart:ffi` from a `libverovio` built with `-DBUILD_AS_LIBRARY=ON` (see
/// ../../cmake/CMakeLists.txt and ../README.md).
///
/// Mirrors ../swift-toolkit/VerovioToolkit.swift method-for-method so the
/// two bindings stay easy to cross-check against c_wrapper.h.
class VerovioToolkit {
  VerovioToolkit._(this._bindings, this._ptr);

  /// Opens [libraryPath] (or, if omitted, the platform default shared
  /// library name on the loader search path) and constructs a Toolkit that
  /// looks up the music font resources at their compiled-in default
  /// location.
  factory VerovioToolkit({String? libraryPath}) {
    final bindings = VerovioBindings(_openLibrary(libraryPath));
    final ptr = bindings.constructor();
    return VerovioToolkit._checked(bindings, ptr);
  }

  /// Same as the default constructor, but with an explicit resource path
  /// (the directory holding the Bravura font etc., e.g. `verovio/data`).
  factory VerovioToolkit.withResourcePath(String resourcePath,
      {String? libraryPath}) {
    final bindings = VerovioBindings(_openLibrary(libraryPath));
    final ptr = using((arena) => bindings
        .constructorResourcePath(resourcePath.toNativeUtf8(allocator: arena)));
    return VerovioToolkit._checked(bindings, ptr);
  }

  /// Same as the default constructor, but skips loading font resources
  /// entirely (only useful for calls that do not render, e.g. Humdrum
  /// conversion).
  factory VerovioToolkit.withNoResource({String? libraryPath}) {
    final bindings = VerovioBindings(_openLibrary(libraryPath));
    final ptr = bindings.constructorNoResource();
    return VerovioToolkit._checked(bindings, ptr);
  }

  static VerovioToolkit _checked(VerovioBindings bindings, Pointer<Void> ptr) {
    if (ptr == nullptr) {
      throw StateError('Verovio Toolkit construction returned a null pointer');
    }
    return VerovioToolkit._(bindings, ptr);
  }

  static DynamicLibrary _openLibrary(String? path) {
    if (path != null) return DynamicLibrary.open(path);
    // Honour an explicit override for CI / custom installs.
    final envPath = Platform.environment['VEROVIO_LIBRARY_PATH'];
    if (envPath != null && envPath.isNotEmpty) {
      return DynamicLibrary.open(envPath);
    }
    if (Platform.isLinux || Platform.isAndroid) {
      // `DynamicLibrary.open('libverovio.so')` only searches the system
      // loader path (LD_LIBRARY_PATH etc.), not the working directory, so
      // resolve the .so shipped next to this package first.
      for (final candidate in _candidateLibraryPaths('libverovio.so')) {
        if (File(candidate).existsSync()) return DynamicLibrary.open(candidate);
      }
      return DynamicLibrary.open('libverovio.so');
    }
    if (Platform.isMacOS) return DynamicLibrary.open('libverovio.dylib');
    if (Platform.isWindows) return DynamicLibrary.open('verovio.dll');
    // iOS links VerovioCore statically into the host app/framework.
    return DynamicLibrary.process();
  }

  /// Absolute paths that may hold the packaged native library, in priority
  /// order: cwd (covers `dart test` from the package dir) then the directory
  /// containing this library's package (covers running from elsewhere).
  static Iterable<String> _candidateLibraryPaths(String fileName) sync* {
    yield '${Directory.current.path}/$fileName';
    try {
      final scriptPath =
          Platform.script.toFilePath();
      var dir = File(scriptPath).parent;
      // Walk up from e.g. .dart_tool/pub/... or lib/src/ to the package root.
      for (var i = 0; i < 6; i++) {
        yield '${dir.path}/$fileName';
        final parent = dir.parent;
        if (parent.path == dir.path) break;
        dir = parent;
      }
    } catch (_) {
      // Platform.script may be non-file (e.g. data: URI); ignore and fall back
      // to the bare library name below.
    }
  }

  final VerovioBindings _bindings;
  Pointer<Void>? _ptr;

  Pointer<Void> get _tk {
    final ptr = _ptr;
    if (ptr == null) {
      throw StateError('VerovioToolkit used after dispose()');
    }
    return ptr;
  }

  /// Releases the native Toolkit. Safe to call more than once; the instance
  /// must not be used afterwards.
  void dispose() {
    final ptr = _ptr;
    if (ptr != null) {
      _bindings.destructor(ptr);
      _ptr = null;
    }
  }

  static String _fromUtf8(Pointer<Utf8> cStr) =>
      cStr == nullptr ? '' : cStr.toDartString();

  void enableLog(bool value) => _bindings.enableLog(value);
  void enableLogToBuffer(bool value) => _bindings.enableLogToBuffer(value);

  bool edit(String action) => using(
      (arena) => _bindings.edit(_tk, action.toNativeUtf8(allocator: arena)));

  String editResponse() => _fromUtf8(_bindings.editResponse(_tk));
  String editStatus() => _fromUtf8(_bindings.editStatus(_tk));
  String getAvailableOptions() => _fromUtf8(_bindings.getAvailableOptions(_tk));
  String getDefaultOptions() => _fromUtf8(_bindings.getDefaultOptions(_tk));

  String getDescriptiveFeatures(String options) =>
      using((arena) => _fromUtf8(_bindings.getDescriptiveFeatures(
          _tk, options.toNativeUtf8(allocator: arena))));

  String getElementAttr(String element, String attribute) =>
      using((arena) => _fromUtf8(_bindings.getElementAttr(
          _tk,
          element.toNativeUtf8(allocator: arena),
          attribute.toNativeUtf8(allocator: arena))));

  String getElementsAtTime(int millisec) =>
      _fromUtf8(_bindings.getElementsAtTime(_tk, millisec));

  String getExpansionIdsForElement(String xmlId) =>
      using((arena) => _fromUtf8(_bindings.getExpansionIdsForElement(
          _tk, xmlId.toNativeUtf8(allocator: arena))));

  String getHumdrum() => _fromUtf8(_bindings.getHumdrum(_tk));

  bool getHumdrumFile(String filename) => using((arena) =>
      _bindings.getHumdrumFile(_tk, filename.toNativeUtf8(allocator: arena)));

  String getID() => _fromUtf8(_bindings.getID(_tk));

  String convertHumdrumToHumdrum(String humdrumData) =>
      using((arena) => _fromUtf8(_bindings.convertHumdrumToHumdrum(
          _tk, humdrumData.toNativeUtf8(allocator: arena))));

  String convertHumdrumToMIDI(String humdrumData) =>
      using((arena) => _fromUtf8(_bindings.convertHumdrumToMIDI(
          _tk, humdrumData.toNativeUtf8(allocator: arena))));

  String convertMEIToHumdrum(String meiData) =>
      using((arena) => _fromUtf8(_bindings.convertMEIToHumdrum(
          _tk, meiData.toNativeUtf8(allocator: arena))));

  String getLog() => _fromUtf8(_bindings.getLog(_tk));

  String getMEI({String options = ''}) => using((arena) =>
      _fromUtf8(_bindings.getMEI(_tk, options.toNativeUtf8(allocator: arena))));

  String getMIDIValuesForElement(String xmlId) =>
      using((arena) => _fromUtf8(_bindings.getMIDIValuesForElement(
          _tk, xmlId.toNativeUtf8(allocator: arena))));

  String getNotatedIdForElement(String xmlId) =>
      using((arena) => _fromUtf8(_bindings.getNotatedIdForElement(
          _tk, xmlId.toNativeUtf8(allocator: arena))));

  String getOptions() => _fromUtf8(_bindings.getOptions(_tk));
  String getOptionUsageString() =>
      _fromUtf8(_bindings.getOptionUsageString(_tk));
  int getPageCount() => _bindings.getPageCount(_tk);

  int getPageWithElement(String xmlId) => using((arena) =>
      _bindings.getPageWithElement(_tk, xmlId.toNativeUtf8(allocator: arena)));

  String getResourcePath() => _fromUtf8(_bindings.getResourcePath(_tk));
  int getScale() => _bindings.getScale(_tk);

  double getTimeForElement(String xmlId) => using((arena) =>
      _bindings.getTimeForElement(_tk, xmlId.toNativeUtf8(allocator: arena)));

  String getTimesForElement(String xmlId) => using((arena) => _fromUtf8(
      _bindings.getTimesForElement(_tk, xmlId.toNativeUtf8(allocator: arena))));

  String getVersion() => _fromUtf8(_bindings.getVersion(_tk));

  bool loadData(String data) => using(
      (arena) => _bindings.loadData(_tk, data.toNativeUtf8(allocator: arena)));

  bool loadFile(String filename) => using((arena) =>
      _bindings.loadFile(_tk, filename.toNativeUtf8(allocator: arena)));

  bool loadZipDataBase64(String base64) => using((arena) =>
      _bindings.loadZipDataBase64(_tk, base64.toNativeUtf8(allocator: arena)));

  bool loadZipDataBuffer(Uint8List data) => using((arena) {
        final buf = arena<Uint8>(data.length);
        buf.asTypedList(data.length).setAll(0, data);
        return _bindings.loadZipDataBuffer(_tk, buf, data.length);
      });

  void redoLayout({String options = ''}) => using((arena) =>
      _bindings.redoLayout(_tk, options.toNativeUtf8(allocator: arena)));

  void redoPagePitchPosLayout() => _bindings.redoPagePitchPosLayout(_tk);

  String renderData(String data, String options) =>
      using((arena) => _fromUtf8(_bindings.renderData(
          _tk,
          data.toNativeUtf8(allocator: arena),
          options.toNativeUtf8(allocator: arena))));

  /// Renders the whole score as a single dotLottie (`.lottie`) package: one
  /// `score` composition, one layer per page, automatic note highlight
  /// (`sm_highlight`) and, for multi-page scores, page-turn (`sm_page`).
  /// This is the production format (`-t dotlottie` on the CLI).
  bool renderToDotLottieFile(String filename) => using((arena) => _bindings
      .renderToDotLottieFile(_tk, filename.toNativeUtf8(allocator: arena)));

  /// Renders a single page as a dotLottie package with a working
  /// note-highlight state machine, no page-turn (`-t dotlottie-highlight`).
  bool renderToDotLottieHighlightFile(String filename, {int pageNo = 1}) =>
      using((arena) => _bindings.renderToDotLottieHighlightFile(
          _tk, filename.toNativeUtf8(allocator: arena), pageNo));

  String renderToExpansionMap() =>
      _fromUtf8(_bindings.renderToExpansionMap(_tk));

  bool renderToExpansionMapFile(String filename) => using((arena) => _bindings
      .renderToExpansionMapFile(_tk, filename.toNativeUtf8(allocator: arena)));

  /// Debug-only classic Lottie JSON for a single page (no dotLottie
  /// package, no state machine, no embedded fonts). See `-t lottie`.
  String renderToLottie({int pageNo = 1}) =>
      _fromUtf8(_bindings.renderToLottie(_tk, pageNo));

  /// Debug-only: all pages as one classic Lottie animation JSON (one layer
  /// per page), still without a dotLottie package or state machine.
  String renderToLottieAnimation() =>
      _fromUtf8(_bindings.renderToLottieAnimation(_tk));

  bool renderToLottieFile(String filename, {int pageNo = 1}) =>
      using((arena) => _bindings.renderToLottieFile(
          _tk, filename.toNativeUtf8(allocator: arena), pageNo));

  String renderToMIDI() => _fromUtf8(_bindings.renderToMIDI(_tk));

  bool renderToMIDIFile(String filename) => using((arena) =>
      _bindings.renderToMIDIFile(_tk, filename.toNativeUtf8(allocator: arena)));

  String renderToPAE() => _fromUtf8(_bindings.renderToPAE(_tk));

  bool renderToPAEFile(String filename) => using((arena) =>
      _bindings.renderToPAEFile(_tk, filename.toNativeUtf8(allocator: arena)));

  String renderToSVG(int pageNo, {bool xmlDeclaration = false}) =>
      _fromUtf8(_bindings.renderToSVG(_tk, pageNo, xmlDeclaration));

  bool renderToSVGFile(String filename, {int pageNo = 1}) =>
      using((arena) => _bindings.renderToSVGFile(
          _tk, filename.toNativeUtf8(allocator: arena), pageNo));

  String renderToTimemap({String options = ''}) => using((arena) => _fromUtf8(
      _bindings.renderToTimemap(_tk, options.toNativeUtf8(allocator: arena))));

  bool renderToTimemapFile(String filename, {String options = ''}) =>
      using((arena) => _bindings.renderToTimemapFile(
          _tk,
          filename.toNativeUtf8(allocator: arena),
          options.toNativeUtf8(allocator: arena)));

  void resetOptions() => _bindings.resetOptions(_tk);
  void resetXmlIdSeed(int seed) => _bindings.resetXmlIdSeed(_tk, seed);

  bool saveFile(String filename, {String options = ''}) =>
      using((arena) => _bindings.saveFile(
          _tk,
          filename.toNativeUtf8(allocator: arena),
          options.toNativeUtf8(allocator: arena)));

  bool select(String selection) => using((arena) =>
      _bindings.select(_tk, selection.toNativeUtf8(allocator: arena)));

  bool setInputFrom(String input) => using((arena) =>
      _bindings.setInputFrom(_tk, input.toNativeUtf8(allocator: arena)));

  bool setOptions(String jsonOptions) => using((arena) =>
      _bindings.setOptions(_tk, jsonOptions.toNativeUtf8(allocator: arena)));

  /// Selects the output format, e.g. `svg`, `midi`, `timemap`, `lottie`,
  /// `dotlottie` or `dotlottie-highlight`.
  bool setOutputTo(String output) => using((arena) =>
      _bindings.setOutputTo(_tk, output.toNativeUtf8(allocator: arena)));

  bool setResourcePath(String path) => using((arena) =>
      _bindings.setResourcePath(_tk, path.toNativeUtf8(allocator: arena)));

  bool setScale(int scale) => _bindings.setScale(_tk, scale);

  String validatePAE(String data) => using((arena) => _fromUtf8(
      _bindings.validatePAE(_tk, data.toNativeUtf8(allocator: arena))));

  String validatePAEFile(String filename) => using((arena) => _fromUtf8(
      _bindings.validatePAEFile(_tk, filename.toNativeUtf8(allocator: arena))));
}
