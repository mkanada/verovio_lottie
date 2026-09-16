import 'package:compare/src/render_jobs.dart';
import 'package:compare/src/svg_preprocess.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:image/image.dart' as img;

int inkPixels(img.Image im) {
  var n = 0;
  for (var y = 0; y < im.height; y++) {
    for (var x = 0; x < im.width; x++) {
      final p = im.getPixel(x, y);
      if (p.r.toInt() < 128 || p.g.toInt() < 128 || p.b.toInt() < 128) n++;
    }
  }
  return n;
}

double inkCentroidX(img.Image im) {
  var sx = 0;
  var n = 0;
  for (var y = 0; y < im.height; y++) {
    for (var x = 0; x < im.width; x++) {
      final p = im.getPixel(x, y);
      if (p.r.toInt() < 128 && p.g.toInt() < 128 && p.b.toInt() < 128) {
        sx += x;
        n++;
      }
    }
  }
  return n == 0 ? -1 : sx / n;
}

void main() {
  test('retângulo simples renderiza tinta no tamanho natural', () async {
    const svg = '<svg width="100px" height="80px" '
        'xmlns="http://www.w3.org/2000/svg">'
        '<rect x="10" y="10" width="30" height="20" fill="black"/>'
        '</svg>';
    final im = await renderSvg(svg);
    expect(im.width, 100);
    expect(im.height, 80);
    expect(inkPixels(im), greaterThan(100));
  });

  test('svg aninhado com viewBox estilo Verovio', () async {
    const svg = '<svg width="200px" height="200px" '
        'xmlns="http://www.w3.org/2000/svg">'
        '<svg class="definition-scale" viewBox="0 0 2000 2000">'
        '<rect x="100" y="100" width="300" height="200" fill="black"/>'
        '</svg></svg>';
    // runSvgToPng achata o aninhado antes de renderizar.
    final im = await renderSvg(flattenNestedSvg(svg));
    expect(im.width, 200);
    expect(im.height, 200);
    expect(inkPixels(im), greaterThan(100));
  });

  test('texto realocado renderiza sem erro no tamanho declarado', () async {
    // O raster de texto no ambiente de teste (fonte Ahem) não é fiel ao
    // release; aqui só garantimos que o pipeline completo (flatten com
    // realocação) produz imagem válida nas dimensões declaradas. O
    // posicionamento real é verificado no binário release (ver README).
    const svg = '<svg width="400px" height="100px" '
        'xmlns="http://www.w3.org/2000/svg">'
        '<svg viewBox="0 0 4000 1000">'
        '<rect x="100" y="100" width="300" height="200" fill="black"/>'
        '<text font-size="0px">'
        '<tspan x="2000" y="400" text-anchor="middle">'
        '<tspan font-size="400px">Hi</tspan>'
        '</tspan></text>'
        '</svg></svg>';
    final im = await renderSvg(flattenNestedSvg(svg));
    expect(im.width, 400);
    expect(im.height, 100);
    expect(inkPixels(im), greaterThan(100));
  });

  test('defs/use com path', () async {
    const svg = '<svg width="100px" height="100px" '
        'xmlns="http://www.w3.org/2000/svg">'
        '<defs><path id="g" d="M10 10h20v20H10z"/></defs>'
        '<use href="#g" fill="black"/>'
        '</svg>';
    final im = await renderSvg(svg);
    expect(inkPixels(im), greaterThan(100));
  });
}
