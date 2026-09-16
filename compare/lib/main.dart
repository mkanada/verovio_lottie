/// Ponto de entrada da ferramenta `compare` (Flutter/Linux).
///
/// Mesma superfície de CLI da versão anterior em Rust:
/// `svg-to-png`, `lottie-to-png`, `diff`, `sm-render`.
///
/// O app não abre janela útil: inicializa o binding do Flutter (necessário
/// para o `flutter_svg` renderizar pelo motor), executa o job pedido,
/// imprime o resultado e sai com o código apropriado (0 ok, 1 erro de
/// execução, 2 erro de uso). Rode sob `xvfb-run` em máquinas sem display.
library;

import 'dart:io';

import 'package:args/args.dart';
import 'package:flutter/widgets.dart';

import 'src/render_jobs.dart';

ArgParser _baseCommands() {
  final parser = ArgParser();
  parser.addCommand('svg-to-png', ArgParser()
    ..addMultiOption('font', help: 'Arquivo de fonte adicional (repetível).')
    ..addOption('pin-serif-family',
        help: 'Família para a qual o genérico CSS "serif" deve resolver.'));
  parser.addCommand('lottie-to-png', ArgParser()
    ..addOption('width', mandatory: true)
    ..addOption('height', mandatory: true)
    ..addOption('frame', defaultsTo: '0')
    ..addMultiOption('slot',
        help: 'Slot de cor "id:r,g,b" (repetível).', splitCommas: false)
    ..addMultiOption('sample',
        help: 'Pixel "x,y" a amostrar (repetível).', splitCommas: false));
  parser.addCommand('diff', ArgParser()
    ..addOption('tolerance', defaultsTo: '0'));
  parser.addCommand('sm-render', ArgParser()
    ..addOption('sm')
    ..addOption('sm-file')
    ..addOption('width', mandatory: true)
    ..addOption('height', mandatory: true)
    ..addOption('script', defaultsTo: '')
    ..addOption('snap', defaultsTo: '')
    ..addOption('prefix', defaultsTo: 'frame')
    ..addMultiOption('sample', splitCommas: false)
    ..addFlag('measure-load', defaultsTo: false));
  return parser;
}

void _usage(ArgParser parser) {
  stderr.writeln('Uso: compare <comando> [opções]');
  stderr.writeln('');
  stderr.writeln('Comandos: svg-to-png, lottie-to-png, diff, sm-render.');
  stderr.writeln(parser.usage);
}

Future<void> main(List<String> args) async {
  WidgetsFlutterBinding.ensureInitialized();
  final parser = _baseCommands();
  ArgResults results;
  try {
    results = parser.parse(args);
  } on FormatException catch (e) {
    stderr.writeln('Erro: ${e.message}');
    _usage(parser);
    exit(2);
  }
  final command = results.command;
  if (command == null) {
    _usage(parser);
    exit(2);
  }
  try {
    switch (command.name) {
      case 'svg-to-png':
        if (command.rest.length != 2) {
          throw FormatException('svg-to-png <entrada.svg> <saída.png>');
        }
        await runSvgToPng(
          input: command.rest[0],
          output: command.rest[1],
          fonts: command['font'] as List<String>,
          pinSerif: command['pin-serif-family'] as String?,
        );
      case 'lottie-to-png':
        if (command.rest.length != 2) {
          throw FormatException(
              'lottie-to-png <entrada.lottie|.json> <saída.png> --width W --height H');
        }
        await runLottieToPng(
          input: command.rest[0],
          output: command.rest[1],
          width: int.parse(command['width'] as String),
          height: int.parse(command['height'] as String),
          frame: double.parse(command['frame'] as String),
          slots: command['slot'] as List<String>,
          samples: command['sample'] as List<String>,
        );
      case 'diff':
        if (command.rest.length != 3) {
          throw FormatException('diff <a.png> <b.png> <saída.png>');
        }
        await runDiff(
          a: command.rest[0],
          b: command.rest[1],
          output: command.rest[2],
          tolerance: int.parse(command['tolerance'] as String),
        );
      case 'sm-render':
        if (command.rest.length != 2) {
          throw FormatException('sm-render <pacote.lottie> <dir-saída>');
        }
        await runSmRender(
          input: command.rest[0],
          outDir: command.rest[1],
          sm: command['sm'] as String?,
          smFile: command['sm-file'] as String?,
          width: int.parse(command['width'] as String),
          height: int.parse(command['height'] as String),
          script: command['script'] as String,
          snap: command['snap'] as String,
          prefix: command['prefix'] as String,
          samples: command['sample'] as List<String>,
          measureLoad: command['measure-load'] as bool,
        );
      default:
        _usage(parser);
        exit(2);
    }
  } on FormatException catch (e) {
    stderr.writeln('Erro: ${e.message}');
    exit(2);
  } catch (e) {
    stderr.writeln('Erro: $e');
    exit(1);
  }
  exit(0);
}
