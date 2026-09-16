import 'package:compare/src/diff.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:image/image.dart' as img;

img.Image solid(int w, int h, int r, int g, int b, [int a = 255]) {
  final im = img.Image(width: w, height: h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      im.setPixelRgba(x, y, r, g, b, a);
    }
  }
  return im;
}

void main() {
  test('imagens iguais: zero divergência', () {
    final r = diffImages(solid(4, 4, 255, 255, 255), solid(4, 4, 255, 255, 255));
    expect(r.result.diffPixels, 0);
    expect(r.result.maxDiff, 0);
    expect(r.result.pct, 0.0);
  });

  test('um pixel diferente sai vermelho no diff', () {
    final a = solid(2, 2, 255, 255, 255);
    final b = solid(2, 2, 255, 255, 255);
    b.setPixelRgba(1, 0, 0, 0, 0, 255);
    final r = diffImages(a, b);
    expect(r.result.diffPixels, 1);
    expect(r.result.maxDiff, 255);
    final px = r.diff.getPixel(1, 0);
    expect(px.r.toInt(), 255);
    expect(px.g.toInt(), 0);
    // pixel igual vira cinza esmaecido, não vermelho
    final same = r.diff.getPixel(0, 0);
    expect(same.r.toInt(), greaterThan(200));
    expect(same.g.toInt(), same.r.toInt());
  });

  test('tolerância ignora antialiasing pequeno', () {
    final a = solid(2, 2, 255, 255, 255);
    final b = solid(2, 2, 250, 250, 250);
    expect(diffImages(a, b, tolerance: 32).result.diffPixels, 0);
    expect(diffImages(a, b).result.diffPixels, 4);
  });

  test('dimensões diferentes lançam', () {
    expect(
      () => diffImages(solid(2, 2, 0, 0, 0), solid(3, 2, 0, 0, 0)),
      throwsArgumentError,
    );
  });
}
