import 'dart:io';

import 'package:archive/archive.dart';
import 'package:flutter/services.dart' show rootBundle;
import 'package:path_provider/path_provider.dart';

const String _assetPath = 'packages/verovio_viewer/assets/verovio_data.zip';

/// A file that only exists once extraction finished completely — an
/// interrupted/partial extraction must never be mistaken for a usable one,
/// since Verovio crashes on missing fonts instead of failing cleanly.
const String _markerRelativePath = 'text/LiberationSerif-Regular.ttf';

Future<String>? _resourcePathFuture;

/// Extracts (once per process) the Liberation + SMuFL font resources
/// bundled with this package and returns the directory holding them —
/// pass it straight as Verovio's `resourcePath`. Same tree as
/// `verovio/data/` in the verovio_lottie repo (Bravura, Leipzig, Gootville,
/// Petaluma, Leland, `text/` with the Liberation TTFs).
///
/// Bundled as a single zip asset rather than raw files: Flutter's asset
/// bundler lists a directory entry non-recursively, so a plain
/// `assets/verovio_data/` folder would silently drop every SMuFL subfolder.
Future<String> verovioResourcePath() {
  return _resourcePathFuture ??= _extractResources().catchError((Object e) {
    _resourcePathFuture = null;
    throw e;
  });
}

Future<String> _extractResources() async {
  final base = await getApplicationSupportDirectory();
  final dir = Directory('${base.path}/verovio_viewer/verovio_data');
  final marker = File('${dir.path}/$_markerRelativePath');
  if (await marker.exists()) return dir.path;

  if (await dir.exists()) await dir.delete(recursive: true);
  await dir.create(recursive: true);

  final data = await rootBundle.load(_assetPath);
  final archive = ZipDecoder().decodeBytes(
    data.buffer.asUint8List(data.offsetInBytes, data.lengthInBytes),
  );
  for (final file in archive.files) {
    if (!file.isFile) continue;
    final out = File('${dir.path}/${file.name}');
    await out.parent.create(recursive: true);
    await out.writeAsBytes(file.content as List<int>);
  }
  return dir.path;
}
