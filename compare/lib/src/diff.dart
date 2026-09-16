/// Comparação pixel a pixel entre dois PNGs (port da função `diff` em Rust).
///
/// Gera a imagem de diferença (fundo em tons de cinza esmaecidos a partir da
/// imagem A + pixels divergentes em vermelho) e as estatísticas impressas no
/// terminal. Usa o pacote `image` em vez de manipulação manual de bytes.
library;

import 'dart:io';

import 'package:image/image.dart' as img;

/// Resultado de [diffImages].
class DiffResult {
  final int total;
  final int diffPixels;
  final int maxDiff;
  final double pct;
  const DiffResult({
    required this.total,
    required this.diffPixels,
    required this.maxDiff,
    required this.pct,
  });

  @override
  String toString() => 'Pixels comparados: $total\n'
      'Pixels diferentes: $diffPixels (${pct.toStringAsFixed(4)}%)\n'
      'Maior diferença de canal observada: $maxDiff';
}

/// Maior diferença absoluta entre os 4 canais (RGBA) de dois pixels.
int channelDiff(img.Pixel a, img.Pixel b) {
  var d = (a.r - b.r).abs().toInt();
  final g = (a.g - b.g).abs().toInt();
  if (g > d) d = g;
  final bl = (a.b - b.b).abs().toInt();
  if (bl > d) d = bl;
  final al = (a.a - b.a).abs().toInt();
  if (al > d) d = al;
  return d;
}

/// Compara [a] e [b] (mesmas dimensões) com [tolerance] por canal e devolve
/// o resultado mais a imagem de diferença.
({DiffResult result, img.Image diff}) diffImages(
  img.Image a,
  img.Image b, {
  int tolerance = 0,
}) {
  if (a.width != b.width || a.height != b.height) {
    throw ArgumentError(
        'dimensões diferentes: ${a.width}x${a.height} vs ${b.width}x${b.height} '
        '— renderize os dois no mesmo tamanho antes de comparar');
  }
  final out = img.Image(width: a.width, height: a.height);
  var diffPixels = 0;
  var maxDiff = 0;
  for (var y = 0; y < a.height; y++) {
    for (var x = 0; x < a.width; x++) {
      final pa = a.getPixel(x, y);
      final pb = b.getPixel(x, y);
      final d = channelDiff(pa, pb);
      if (d > maxDiff) maxDiff = d;
      if (d > tolerance) {
        diffPixels++;
        out.setPixelRgba(x, y, 255, 0, 0, 255);
      } else {
        // Fundo em tons de cinza esmaecido (a partir da imagem A) para dar
        // contexto visual a quem for inspecionar a imagem de diferença.
        final l = (pa.r.toInt() + pa.g.toInt() + pa.b.toInt()) ~/ 3;
        final dimmed = 255 - ((255 - l) ~/ 3);
        out.setPixelRgba(x, y, dimmed, dimmed, dimmed, 255);
      }
    }
  }
  final total = a.width * a.height;
  return (
    result: DiffResult(
      total: total,
      diffPixels: diffPixels,
      maxDiff: maxDiff,
      pct: 100.0 * diffPixels / total,
    ),
    diff: out,
  );
}

/// Lê dois arquivos PNG, compara e grava a imagem de diferença em [output].
DiffResult diffPngFiles(String aPath, String bPath, String output,
    {int tolerance = 0}) {
  final a = img.decodeImage(File(aPath).readAsBytesSync());
  final b = img.decodeImage(File(bPath).readAsBytesSync());
  if (a == null) throw StateError('não foi possível decodificar $aPath');
  if (b == null) throw StateError('não foi possível decodificar $bPath');
  final r = diffImages(a, b, tolerance: tolerance);
  File(output).writeAsBytesSync(img.encodePng(r.diff));
  return r.result;
}
