import 'package:compare/src/svg_preprocess.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('stripTitleElements remove title aninhado em tspan (D01-3)', () {
    const svg = '<svg><text><tspan x="10" text-anchor="middle">'
        '<title class="labelAttr">rótulo bem longo</title>'
        '<tspan>texto de verdade</tspan></tspan></text></svg>';
    final out = stripTitleElements(svg);
    expect(out.contains('<title'), isFalse);
    expect(out.contains('rótulo bem longo'), isFalse);
    expect(out.contains('texto de verdade'), isTrue);
  });

  test('stripTitleElements remove self-closing e mantém resto', () {
    const svg = '<svg><title/><rect width="5"/></svg>';
    expect(stripTitleElements(svg), '<svg><rect width="5"/></svg>');
  });

  test('stripTitleElements sem title devolve igual', () {
    const svg = '<svg><rect width="5"/></svg>';
    expect(stripTitleElements(svg), svg);
  });

  test('pinSerifFamily troca valor inteiro pelo nome puro', () {
    const svg = '<text font-family="Times, serif">cresc.</text>';
    expect(
      pinSerifFamily(svg, 'Liberation Serif'),
      '<text font-family="Liberation Serif">cresc.</text>',
    );
  });

  test('pinSerifFamily não toca outras famílias', () {
    const svg = '<text font-family="Leipzig">x</text>';
    expect(pinSerifFamily(svg, 'Liberation Serif'), svg);
  });

  test('flattenNestedSvg sem aninhamento devolve igual', () {
    const svg = '<svg width="100px" height="50px"><rect/></svg>';
    expect(flattenNestedSvg(svg), svg);
  });

  test('flattenNestedSvg estilo Verovio vira g com scale(0.1)', () {
    const svg = '<svg width="2100px" height="2970px">'
        '<svg class="definition-scale" color="black" '
        'font-family="Times, serif" viewBox="0 0 21000 29700">'
        '<rect/></svg></svg>';
    final out = flattenNestedSvg(svg);
    expect(out.contains('<svg class='), isFalse);
    expect(out.contains('transform="scale(0.1,0.1)"'), isTrue);
    expect(out.contains('font-family="Times, serif"'), isTrue);
    expect(out.endsWith('</g></svg>'), isTrue);
  });

  test('flattenNestedSvg realoca textos para fora do g com pré-escala', () {
    const svg = '<svg width="200px" height="100px">'
        '<svg viewBox="0 0 2000 1000">'
        '<rect width="10" height="10"/>'
        '<text font-size="0px"><tspan x="1000" y="400" text-anchor="middle">'
        '<tspan font-size="300px">Hi</tspan></tspan></text>'
        '</svg></svg>';
    final out = flattenNestedSvg(svg);
    // O retângulo fica dentro do g com scale...
    expect(out.contains('<rect width="10" height="10"/></g>'), isTrue);
    // ...e o texto vai para depois dele, com x/y/font-size em px finais.
    expect(
      out.contains(
        '<text font-size="0px"><tspan x="100" y="40" text-anchor="middle">'
        '<tspan font-size="30px">Hi</tspan></tspan></text></svg>',
      ),
      isTrue,
    );
    expect(out.contains('transform="scale(0.1,0.1)"'), isTrue);
  });

  test('descaleTextBlock escala x/y/font-size e preserva o resto', () {
    expect(
      descaleTextBlock(
        '<text x="10000" y="417" text-anchor="middle" font-size="607px">a</text>',
        0.1,
        0.1,
      ),
      '<text x="1000" y="41.7" text-anchor="middle" font-size="60.7px">a</text>',
    );
    // Auto-fechado também é escalado.
    expect(
      descaleTextBlock('<text x="1510" y="2189" font-size="0px" />', 0.1, 0.1),
      '<text x="151" y="218.9" font-size="0px" />',
    );
  });
}
