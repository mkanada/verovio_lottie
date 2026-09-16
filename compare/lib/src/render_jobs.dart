/// Jobs da ferramenta: lottie-to-png, diff e sm-render (svg-to-png é o
/// binário Rust separado `compare/svg_render/`, ver `compare/README.md`).
///
/// Mensagens de terminal mantidas no mesmo formato de sempre (os scripts
/// extraem `pct_diferente` etc. do stdout de `diff` com grep).
library;

import 'dart:io';
import 'dart:typed_data';

import 'package:image/image.dart' as img;

import 'diff.dart';
import 'lottie_native.dart';
import 'script.dart';

/// Compõe um buffer ARGB8888S (alpha reto) sobre branco opaco e devolve um
/// PNG sempre opaco — mesma convenção da versão anterior.
img.Image pixelsToWhitePng(Uint32List buffer, int width, int height) {
  int blend(int channel, int alpha) =>
      ((channel * alpha + 255 * (255 - alpha) + 127) ~/ 255);
  final out = img.Image(width: width, height: height);
  for (var i = 0; i < buffer.length; i++) {
    final px = buffer[i];
    final a = (px >> 24) & 0xFF;
    final r = (px >> 16) & 0xFF;
    final g = (px >> 8) & 0xFF;
    final b = px & 0xFF;
    out.setPixelRgba(i % width, i ~/ width, blend(r, a), blend(g, a),
        blend(b, a), 255);
  }
  return out;
}

void _loadInto(NativePlayer player, String input) {
  if (input.endsWith('.lottie')) {
    player.loadBytes(File(input).readAsBytesSync());
  } else if (input.endsWith('.json')) {
    player.loadJson(File(input).readAsStringSync());
  } else {
    throw ArgumentError(
        'extensão não suportada em $input; use .lottie ou .json');
  }
}

String _fmtSample(Uint32List buffer, int width, int x, int y) {
  final i = y * width + x;
  if (x < 0 || y < 0 || i < 0 || i >= buffer.length) {
    return 'pixel($x,$y)=fora-dos-limites';
  }
  final px = buffer[i];
  return 'pixel($x,$y)=rgba(${(px >> 16) & 0xFF},${(px >> 8) & 0xFF},${px & 0xFF},${(px >> 24) & 0xFF})';
}

Future<void> runLottieToPng({
  required String input,
  required String output,
  required int width,
  required int height,
  required double frame,
  required List<String> slots,
  required List<String> samples,
  List<String> preloadFonts = const [],
}) async {
  if (width <= 0 || height <= 0) {
    throw ArgumentError('--width e --height precisam ser maiores que zero');
  }
  final player = NativePlayer();
  try {
    // Fontes pré-carregadas (spike: fonte não embutida no pacote, provida
    // pelo host via `dotlottie_load_font` em vez de `fonts.list`/`origin:3`
    // - ver compare/README.md). Precisa acontecer ANTES do load: o load já
    // renderiza o frame inicial, que já precisaria da fonte resolvida.
    for (final spec in preloadFonts) {
      final colon = spec.indexOf(':');
      if (colon < 0) {
        throw FormatException(
            '--preload-font mal formado (esperado nome:caminho.ttf): $spec');
      }
      final name = spec.substring(0, colon);
      final path = spec.substring(colon + 1);
      NativePlayer.loadFont(name, await File(path).readAsBytes());
    }
    // O destino precisa existir ANTES do load: o load já deixa o buffer com
    // o frame inicial renderizado.
    player.setTarget(width, height);
    _loadInto(player, input);
    if (player.totalFrames() <= 0) {
      throw StateError(
          'nenhuma animação carregada — verifique se o arquivo é um Lottie/dotLottie válido');
    }
    // Slots de cor (theming v2, modo interativo M3): independem do frame.
    for (final s in slots) {
      final slot = parseColorSlot(s);
      player.setColorSlot(
          slot.id, slot.rgb[0], slot.rgb[1], slot.rgb[2]);
    }
    player.seekFrame(frame);
    player.flush();
    final buffer = player.snapshot();
    final image = pixelsToWhitePng(buffer, width, height);
    await File(output).writeAsBytes(img.encodePng(image));
    for (final s in samples) {
      final comma = s.indexOf(',');
      final x = int.parse(s.substring(0, comma).trim());
      final y = int.parse(s.substring(comma + 1).trim());
      stdout.writeln(_fmtSample(buffer, width, x, y));
    }
    stdout.writeln(
        'PNG salvo em $output (${width}x$height, frame $frame)');
  } finally {
    player.dispose();
  }
}

Future<void> runDiff({
  required String a,
  required String b,
  required String output,
  required int tolerance,
}) async {
  final result = diffPngFiles(a, b, output, tolerance: tolerance);
  stdout.writeln('Pixels comparados: ${result.total}');
  stdout.writeln(
      'Pixels diferentes (tolerância $tolerance): ${result.diffPixels} (${result.pct.toStringAsFixed(4)}%)');
  stdout.writeln('Maior diferença de canal observada: ${result.maxDiff}');
  stdout.writeln('Imagem de diferença salva em $output');
}

Future<void> runSmRender({
  required String input,
  required String outDir,
  String? sm,
  String? smFile,
  required int width,
  required int height,
  required String script,
  required String snap,
  required String prefix,
  required List<String> samples,
  required bool measureLoad,
}) async {
  if (width <= 0 || height <= 0) {
    throw ArgumentError('--width e --height precisam ser maiores que zero');
  }
  if ((sm == null) == (smFile == null)) {
    throw ArgumentError('informe --sm <id-no-pacote> ou --sm-file <json-avulso>');
  }
  await Directory(outDir).create(recursive: true);
  if (!input.endsWith('.lottie')) {
    throw ArgumentError('sm-render exige um pacote .lottie, não $input');
  }
  final player = NativePlayer();
  NativeStateMachine? engine;
  try {
    player.setTarget(width, height);
    player.loadBytes(File(input).readAsBytesSync());
    final loadStart = DateTime.now();
    if (sm != null) {
      engine = player.loadStateMachineById(sm);
    } else {
      engine =
          player.loadStateMachineData(File(smFile!).readAsStringSync());
    }
    final loadElapsed = DateTime.now().difference(loadStart);
    final startStart = DateTime.now();
    engine.start();
    final startElapsed = DateTime.now().difference(startStart);
    stdout.writeln(
        'state_machine_load: ${(loadElapsed.inMicroseconds / 1000).toStringAsFixed(3)}ms — '
        'start(): ${(startElapsed.inMicroseconds / 1000).toStringAsFixed(3)}ms');
    if (measureLoad) return;
    final actions = parseScript(script);
    final snaps = parseSnap(snap);
    final samplePoints = parseSamples(samples);
    var maxT = 0;
    for (final a in actions) {
      if (a.ms > maxT) maxT = a.ms;
    }
    for (final s in snaps) {
      if (s > maxT) maxT = s;
    }
    var t = 0;
    while (true) {
      // Ações marcadas para o instante `t` (inclusive t=0) rodam antes do
      // snapshot desse instante.
      for (final action in actions.where((a) => a.ms == t)) {
        final op = action.op;
        try {
          if (op is FireOp) {
            engine.fire(op.event);
          } else if (op is SlotOp) {
            player.setColorSlot(op.id, op.rgb[0], op.rgb[1], op.rgb[2]);
          } else if (op is ClearSlotOp) {
            player.clearSlot(op.id);
          } else if (op is ClearSlotsOp) {
            player.clearSlots();
          }
        } on NativeError catch (e) {
          stderr.writeln('aviso: ação em t=${t}ms falhou ($e)');
        }
      }
      // fire() não renderiza sozinho: força o flush antes do snapshot
      // (inofensivo quando nada mudou).
      player.flush();
      if (snaps.contains(t)) {
        final buffer = player.snapshot();
        final image = pixelsToWhitePng(buffer, width, height);
        final outPath = '$outDir/$prefix-t$t.png';
        await File(outPath).writeAsBytes(img.encodePng(image));
        var line =
            't=${t}ms state=${engine.currentState()} png=$outPath';
        for (final (x, y) in samplePoints) {
          line += ' ${_fmtSample(buffer, width, x, y)}';
        }
        stdout.writeln(line);
      }
      if (t >= maxT) break;
      engine.tick(1.0);
      t++;
    }
  } finally {
    engine?.dispose();
    player.dispose();
  }
}
