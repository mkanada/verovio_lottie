/// Pré-processamento do SVG de referência antes de entregar ao `flutter_svg`.
///
/// Equivalente às duas correções documentadas na versão Rust da ferramenta:
/// - D01-3: remove todo elemento `<title>` (o medidor de `text-anchor` de
///   alguns renderizadores incluía o texto do `<title>` aninhado na largura).
///   `<title>` nunca é desenhado por nenhum renderizador, então a remoção não
///   muda nada visualmente.
/// - D01-2: `--pin-serif-family`: troca o genérico CSS `serif` (que o Verovio
///   emite como `font-family="Times, serif"`) por uma família empacotada no
///   app, independente das fontes instaladas no sistema operacional.
library;

/// Remove todo elemento `<title>...</title>` (e `<title .../>`) do [svg].
///
/// Implementado por regex sobre o texto original (equivalente à remoção por
/// range de bytes da versão anterior): `<title>` não aninha, então
/// `<title\b[^>]*>.*?</title\s*>` com dotAll captura cada ocorrência exata.
String stripTitleElements(String svg) {
  var out = svg.replaceAll(
    RegExp(r'<title\b[^>]*>.*?</title\s*>', dotAll: true),
    '',
  );
  out = out.replaceAll(RegExp(r'<title\b[^>]*/>'), '');
  return out;
}

/// Fixa o genérico CSS `serif` (que o Verovio emite como
/// `font-family="Times, serif"`) na família [family], independente das fontes
/// instaladas no sistema operacional.
///
/// Troca o valor INTEIRO do atributo pelo nome puro da família (sem manter
/// `, serif`): o `flutter_svg` repassa a string de `font-family` direto ao
/// `ParagraphBuilder` do motor, que só resolve um nome exato de família —
/// uma lista CSS (`"Liberation Serif, serif"`) não bate nem com a fonte
/// empacotada no app. Como o SVG do Verovio declara `font-family` num único
/// lugar (herdado pelo resto), isso equivale ao `--pin-serif-family` da
/// versão anterior.
String pinSerifFamily(String svg, String family) {
  return svg.replaceAllMapped(
    RegExp(r'font-family="Times\s*,\s*serif"'),
    (_) => 'font-family="$family"',
  );
}

double? _leadingNumber(String s) {
  final m = RegExp(r'^[+-]?(\d+(\.\d+)?)').firstMatch(s.trim());
  return m == null ? null : double.tryParse(m.group(1)!);
}

String? _attr(String tag, String name) {
  final m = RegExp('$name="([^"]*)"').firstMatch(tag);
  return m?.group(1);
}

/// Achata o `<svg>` aninhado do Verovio (`<svg class="definition-scale"
/// viewBox="0 0 W H">`, sem width/height próprios) num `<g
/// transform="scale(...)">` matematicamente equivalente, e realoca os
/// elementos `text` para fora desse `<g>` com coordenadas pré-escaladas.
///
/// Motivo da primeira parte: `flutter_svg`/`vector_graphics` não suporta
/// `<svg>` aninhado e dropa o conteúdo em silêncio (PNG 100% branco) — mesma
/// limitação que o loader de SVG do ThorVG tinha (ver
/// `thorvg-cli/src/render.cpp`, `FlattenNestedSvg`). Os demais atributos da
/// tag interna (ex. `font-family`, `color`) são preservados no `<g>`, pois
/// valem por herança. Içar o `viewBox` para a raiz (a alternativa "natural")
/// não serve: quebra os `<use>` com `transform` do Verovio no
/// `vector_graphics` (todos os glifos SMuFL somem).
///
/// Motivo da segunda parte (realocação): sob um ancestral com `transform` o
/// `vector_graphics` erra o texto de dois jeitos complementares, ambos
/// verificados empiricamente contra o binário release: texto plano com
/// `x`/`text-anchor` no próprio `text` sai no tamanho SEM escala (título
/// gigante), e texto com `tspan` aninhado sai na posição errada (título
/// colado à esquerda). Sem ancestral com `transform`, todos os padrões do
/// Verovio (`x`/`text-anchor` em `text` ou `tspan`, tamanhos por folha)
/// saem corretos — então cada bloco `text` é movido para depois do `<g>`
/// (sempre no topo, ordem relativa preservada) com `x`, `y` e `font-size`
/// já multiplicados pela escala. O layout interno de cada run (avanços,
/// kerning, âncora) continua por conta do motor, agora no tamanho final.
///
/// Assume o que o Verovio garante: no máximo um nível de aninhamento, raiz
/// com `width`/`height` (escala quadrada), e o `</svg>` que fecha o aninhado
/// é o primeiro `</svg>` após a tag interna. O texto do Verovio não usa
/// `dx`/`dy`/`rotate`/`transform` próprio (verificado no corpus) — só `x`,
/// `y` e `font-size`, os três pré-escalados aqui. Fora disso, devolve o
/// original inalterado.
String flattenNestedSvg(String svg) {
  final first = svg.indexOf('<svg');
  if (first < 0) return svg;
  final second = svg.indexOf('<svg', first + 4);
  if (second < 0) return svg; // sem aninhamento, nada a fazer

  final outerTagEnd = svg.indexOf('>', first);
  if (outerTagEnd < 0) return svg;
  final outerTag = svg.substring(first, outerTagEnd + 1);
  final outerW = _leadingNumber(_attr(outerTag, 'width') ?? '');
  final outerH = _leadingNumber(_attr(outerTag, 'height') ?? '');
  if (outerW == null || outerH == null || outerW <= 0 || outerH <= 0) {
    return svg;
  }

  final innerTagEnd = svg.indexOf('>', second);
  if (innerTagEnd < 0) return svg;
  final innerTag = svg.substring(second, innerTagEnd + 1);
  final vbParts = (_attr(innerTag, 'viewBox') ?? '').split(RegExp(r'\s+'));
  if (vbParts.length != 4) return svg;
  final vb = vbParts.map(double.tryParse).toList();
  if (vb.any((v) => v == null)) return svg;
  final vw = vb[2]!, vh = vb[3]!;
  if (vw <= 0 || vh <= 0) return svg;

  // "<svg ...>" -> "<g ...>", trocando só o nome da tag e o atributo viewBox
  // por transform (mantém todos os outros atributos por herança).
  final sx = outerW / vw;
  final sy = outerH / vh;
  var gTag = innerTag.replaceFirst('<svg', '<g');
  gTag = gTag.replaceFirst(
    RegExp(r'\s*viewBox="[^"]*"'),
    ' transform="scale($sx,$sy)"',
  );

  final innerClose = svg.indexOf('</svg>', innerTagEnd);
  if (innerClose < 0) return svg;
  final content = svg.substring(innerTagEnd + 1, innerClose);
  // Realoca os textos para fora do <g> com scale (ver doc acima).
  final relocated = StringBuffer();
  final rest = content.replaceAllMapped(
    RegExp(r'<text\b[^>]*(?:/>|>.*?</text>)', dotAll: true),
    (m) {
      relocated.write(descaleTextBlock(m.group(0)!, sx, sy));
      return '';
    },
  );
  return '${svg.substring(0, second)}$gTag'
      '$rest</g>'
      '$relocated'
      '${svg.substring(innerClose + 6)}';
}

/// Multiplica `x`, `y` e `font-size` de um bloco `text` (aberto ou
/// auto-fechado) por (`sx`, `sy`) — `font-size` por `sx` (a escala do
/// Verovio é sempre quadrada). Usado ao realocar textos para fora do `<g>`
/// com `scale` em [flattenNestedSvg].
String descaleTextBlock(String block, double sx, double sy) {
  String scaleValue(String value, double s) {
    final m = RegExp(r'^[+-]?(\d+(\.\d+)?)(.*)$').firstMatch(value.trim());
    if (m == null) return value;
    final scaled = double.parse(m.group(1)!) * s;
    var out = scaled.toStringAsFixed(4);
    out = out.replaceFirst(RegExp(r'\.?0+$'), '');
    return '$out${m.group(3)}';
  }
  var out = block.replaceAllMapped(
    RegExp(r'\sx="[^"]*"'),
    (m) => ' x="${scaleValue(m.group(0)!.substring(4, m.group(0)!.length - 1), sx)}"',
  );
  out = out.replaceAllMapped(
    RegExp(r'\sy="[^"]*"'),
    (m) => ' y="${scaleValue(m.group(0)!.substring(4, m.group(0)!.length - 1), sy)}"',
  );
  out = out.replaceAllMapped(
    RegExp(r'\sfont-size="[^"]*"'),
    (m) {
      final v = m.group(0)!;
      return ' font-size="${scaleValue(v.substring(12, v.length - 1), sx)}"';
    },
  );
  return out;
}
