/// Lottie/dotLottie → PNG através do widget real `ScoreViewer`
/// (`package:verovio_viewer`), capturado com o mecanismo padrão do próprio
/// Flutter para tirar um "print" de uma árvore de widgets
/// (`RenderRepaintBoundary.toImage`) — não o FFI direto de
/// `lottie_native.dart`.
///
/// Existe pra validar visualmente o caminho de código que uma UI de verdade
/// (zywny, ou uma futura inspeção visual do `compare`) realmente percorre —
/// `LottieFileServer` servindo o arquivo, `pageRestFrames` lendo os markers
/// de página — em vez de só o motor nativo por trás dele. Cobre unicamente
/// comparação de página (sem highlight/slot: ver `compare/README.md`), então
/// só aceita pacotes `.lottie` de verdade (não o `.json` de página avulsa
/// gerado por `verovio -t lottie`, que não tem os markers `pageN` que
/// `pageRestFrames` procura).
///
/// Não precisa de nenhuma janela "útil": `runApp` aqui só existe pra ter uma
/// árvore de widgets de verdade pra tirar satisfação com `toImage` — a
/// composição alvo (`width`x`height`) é imposta via `OverflowBox` com
/// constraints exatas, independente do tamanho real da janela nativa (GLFW)
/// por trás — por isso segue funcionando sob `xvfb-run` como as outras
/// operações desta ferramenta.
library;

import 'dart:async';
import 'dart:io';
import 'dart:ui' as ui;

import 'package:flutter/rendering.dart';
import 'package:flutter/scheduler.dart';
import 'package:flutter/widgets.dart';
import 'package:verovio_viewer/verovio_viewer.dart';

Future<void> renderScoreViewerToPng({
  required String lottiePath,
  required int page,
  required int width,
  required int height,
  required String output,
}) async {
  if (!lottiePath.endsWith('.lottie')) {
    throw ArgumentError(
      '--engine widget exige um pacote .lottie (com markers de página) — '
      'não $lottiePath',
    );
  }

  final rests = await pageRestFrames(lottiePath);
  if (page < 0 || page >= rests.length) {
    throw ArgumentError(
      '--page $page fora do intervalo (pacote tem ${rests.length} página(s))',
    );
  }
  final targetFrame = rests[page];

  final boundaryKey = GlobalKey();
  final scoreViewerKey = GlobalKey<ScoreViewerState>();
  final readyCompleter = Completer<void>();
  final renderCompleter = Completer<void>();

  runApp(
    Directionality(
      textDirection: TextDirection.ltr,
      child: OverflowBox(
        minWidth: width.toDouble(),
        maxWidth: width.toDouble(),
        minHeight: height.toDouble(),
        maxHeight: height.toDouble(),
        alignment: Alignment.topLeft,
        child: RepaintBoundary(
          key: boundaryKey,
          child: SizedBox(
            width: width.toDouble(),
            height: height.toDouble(),
            child: ColoredBox(
              color: const Color(0xFFFFFFFF),
              child: ScoreViewer(
                key: scoreViewerKey,
                lottiePath: lottiePath,
                onReady: () {
                  if (!readyCompleter.isCompleted) readyCompleter.complete();
                },
                onError: (e) {
                  if (!readyCompleter.isCompleted) {
                    readyCompleter.completeError(e);
                  }
                },
                onRender: (frameNo) {
                  if (!renderCompleter.isCompleted &&
                      frameNo.round() == targetFrame) {
                    renderCompleter.complete();
                  }
                },
              ),
            ),
          ),
        ),
      ),
    ),
  );

  await readyCompleter.future.timeout(
    const Duration(seconds: 20),
    onTimeout: () =>
        throw StateError('ScoreViewer não sinalizou onReady (timeout)'),
  );

  // goToPage é um no-op visual quando page==0 (onLoad já buscou o mesmo
  // frame), mas simplifica o fluxo: sempre passa pelo mesmo caminho.
  await scoreViewerKey.currentState!.goToPage(page);

  // onRender confirma que o motor nativo já processou o frame pedido, não
  // que o Flutter já decodificou/pintou a imagem resultante (isso é
  // assíncrono, via `ui.ImageDescriptor`, e roda em paralelo ao evento) —
  // por isso ainda assenta mais alguns frames de verdade antes de capturar.
  await renderCompleter.future.timeout(
    const Duration(seconds: 20),
    onTimeout: () =>
        throw StateError('onRender não confirmou o frame $targetFrame a tempo'),
  );
  await Future<void>.delayed(const Duration(milliseconds: 200));
  for (var i = 0; i < 3; i++) {
    await SchedulerBinding.instance.endOfFrame;
  }

  final boundary =
      boundaryKey.currentContext!.findRenderObject() as RenderRepaintBoundary;
  final image = await boundary.toImage(pixelRatio: 1.0);
  final byteData = await image.toByteData(format: ui.ImageByteFormat.png);
  await File(output).writeAsBytes(byteData!.buffer.asUint8List());
  stdout.writeln(
    'PNG salvo em $output (${width}x$height, página $page, frame $targetFrame, engine widget)',
  );
}
