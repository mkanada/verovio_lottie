import 'dart:convert';

import 'package:archive/archive.dart';
import 'package:compare/src/lottie_package.dart';
import 'package:flutter_test/flutter_test.dart';

List<int> fakePackage(Map<String, dynamic> score) {
  final archive = Archive();
  final bytes = utf8.encode(jsonEncode(score));
  archive.addFile(ArchiveFile('a/score.json', bytes.length, bytes));
  archive.addFile(ArchiveFile(
      'manifest.json', 2, utf8.encode('{}')));
  return ZipEncoder().encode(archive);
}

void main() {
  test('frameForPage usa marker page<N-1> e cai em página-1', () {
    final info = readScoreInfoFromPackage(fakePackage({
      'w': 2100,
      'h': 2970,
      'markers': [
        {'cm': 'page0', 'tm': 0},
        {'cm': 'page1', 'tm': 42},
      ],
    }));
    expect(info.width, 2100);
    expect(info.height, 2970);
    expect(info.frameForPage(1), 0.0);
    expect(info.frameForPage(2), 42.0);
    expect(info.frameForPage(9), 8.0); // fallback
  });

  test('pacote sem score.json lança', () {
    final archive = Archive();
    final bytes = utf8.encode('{}');
    archive.addFile(ArchiveFile('manifest.json', bytes.length, bytes));
    expect(
      () => readScoreInfoFromPackage(ZipEncoder().encode(archive)),
      throwsStateError,
    );
  });
}
