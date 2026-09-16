/// Leitura de pacotes `.lottie` (zip) para resolver dimensões e frames de
/// repouso por página — substitui o trecho `unzip -p ... a/score.json` +
/// Python que os scripts `.sh` usavam com a ferramenta Rust.
///
/// Convenção (docs/plano/C04-paginas-virada.md): pacotes multi-página têm um
/// marker `page<N-1>` em `a/score.json` indicando o frame em que a câmera
/// fica parada exatamente na página N. Pacotes sem essa trilha (1 página só)
/// usam o fallback `frame == página - 1`.
library;

import 'dart:convert';
import 'dart:io';

import 'package:archive/archive.dart';

/// Conteúdo relevante de `a/score.json` para a comparação.
class ScoreInfo {
  final int width;
  final int height;

  /// marker `cm` → frame `tm`.
  final Map<String, double> markers;
  const ScoreInfo({
    required this.width,
    required this.height,
    required this.markers,
  });

  /// Frame de repouso da câmera para a página [page] (1-based).
  double frameForPage(int page) {
    final marker = markers['page${page - 1}'];
    if (marker != null) return marker;
    return (page - 1).toDouble();
  }
}

ScoreInfo parseScoreJson(Map<String, dynamic> data) {
  final w = (data['w'] as num).toInt();
  final h = (data['h'] as num).toInt();
  final markers = <String, double>{};
  final raw = data['markers'];
  if (raw is List) {
    for (final m in raw) {
      if (m is Map<String, dynamic>) {
        final cm = m['cm'];
        final tm = m['tm'];
        if (cm is String && tm is num) markers[cm] = tm.toDouble();
      }
    }
  }
  return ScoreInfo(width: w, height: h, markers: markers);
}

/// Extrai e interpreta `a/score.json` de um pacote `.lottie`.
ScoreInfo readScoreInfoFromPackage(List<int> bytes) {
  final archive = ZipDecoder().decodeBytes(bytes);
  for (final file in archive.files) {
    if (file.name == 'a/score.json' || file.name.endsWith('/score.json')) {
      final data =
          jsonDecode(utf8.decode(file.content as List<int>)) as Map<String, dynamic>;
      return parseScoreJson(data);
    }
  }
  throw StateError('a/score.json não encontrado no pacote .lottie');
}

/// Carrega [path] (`.lottie` ou `.json`) e devolve o [ScoreInfo].
ScoreInfo loadScoreInfo(String path) {
  if (path.endsWith('.json')) {
    final data = jsonDecode(File(path).readAsStringSync()) as Map<String, dynamic>;
    return parseScoreJson(data);
  }
  if (path.endsWith('.lottie')) {
    return readScoreInfoFromPackage(File(path).readAsBytesSync());
  }
  throw ArgumentError(
      'extensão não suportada em $path; use .lottie ou .json');
}
