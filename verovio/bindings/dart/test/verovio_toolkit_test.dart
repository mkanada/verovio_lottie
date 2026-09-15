@TestOn('linux')
library;

import 'dart:io';

import 'package:test/test.dart';
import 'package:verovio/verovio.dart';

// corpus/mei/Grieg_Little_bird_Op43_No4.mei already has a validated
// corpus/lottie/Grieg_Little_bird_Op43_No4.lottie counterpart (see
// corpus/README.md), so a working export from it is a meaningful smoke
// test rather than a synthetic fixture.
String _repoRoot() {
  var dir = Directory.current;
  while (!File('${dir.path}/CLAUDE.md').existsSync()) {
    final parent = dir.parent;
    if (parent.path == dir.path) {
      throw StateError('Could not locate repo root from ${Directory.current}');
    }
    dir = parent;
  }
  return dir.path;
}

void main() {
  final repoRoot = _repoRoot();
  final resourcePath = '$repoRoot/verovio/data';
  final sampleMei = '$repoRoot/corpus/mei/Grieg_Little_bird_Op43_No4.mei';

  test('loads a MEI file and reports pages', () {
    final toolkit = VerovioToolkit.withResourcePath(resourcePath);
    addTearDown(toolkit.dispose);

    expect(toolkit.getVersion(), isNotEmpty);
    expect(toolkit.loadFile(sampleMei), isTrue);
    expect(toolkit.getPageCount(), greaterThan(0));
  });

  test('renders SVG for page 1', () {
    final toolkit = VerovioToolkit.withResourcePath(resourcePath);
    addTearDown(toolkit.dispose);

    expect(toolkit.loadFile(sampleMei), isTrue);
    final svg = toolkit.renderToSVG(1);
    expect(svg, contains('<svg'));
  });

  test('renders a whole-score dotLottie package', () {
    final toolkit = VerovioToolkit.withResourcePath(resourcePath);
    addTearDown(toolkit.dispose);

    expect(toolkit.loadFile(sampleMei), isTrue);

    final outFile =
        File('${Directory.systemTemp.path}/verovio_dart_ffi_test.lottie');
    addTearDown(() {
      if (outFile.existsSync()) outFile.deleteSync();
    });

    expect(toolkit.renderToDotLottieFile(outFile.path), isTrue);
    expect(outFile.existsSync(), isTrue);

    final bytes = outFile.readAsBytesSync();
    // dotLottie packages are zip archives: "PK\x03\x04" local-file-header
    // magic is the cheapest correctness check without a zip dependency.
    expect(bytes.length, greaterThan(1000));
    expect(bytes.sublist(0, 4), equals([0x50, 0x4b, 0x03, 0x04]));
  });

  test('renders a single-page dotLottie-highlight package', () {
    final toolkit = VerovioToolkit.withResourcePath(resourcePath);
    addTearDown(toolkit.dispose);

    expect(toolkit.loadFile(sampleMei), isTrue);

    final outFile = File(
        '${Directory.systemTemp.path}/verovio_dart_ffi_test_highlight.lottie');
    addTearDown(() {
      if (outFile.existsSync()) outFile.deleteSync();
    });

    expect(toolkit.renderToDotLottieHighlightFile(outFile.path, pageNo: 1),
        isTrue);
    expect(outFile.readAsBytesSync().sublist(0, 2), equals([0x50, 0x4b]));
  });
}
