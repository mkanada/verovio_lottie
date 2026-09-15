// Run from bindings/dart/ after ./build_linux_so.sh:
//   dart run example/main.dart <input.mei> <output.lottie> [resourcePath]
import 'dart:io';

import 'package:verovio/verovio.dart';

void main(List<String> args) {
  if (args.length < 2) {
    stderr.writeln(
        'usage: dart run example/main.dart <input.mei> <output.lottie> [resourcePath]');
    exit(64);
  }
  final input = args[0];
  final output = args[1];
  final resourcePath = args.length > 2 ? args[2] : '../../data';

  final toolkit = VerovioToolkit.withResourcePath(resourcePath);
  try {
    print('Verovio ${toolkit.getVersion()}');

    if (!toolkit.loadFile(input)) {
      stderr.writeln('Failed to load $input');
      exit(1);
    }
    print('Loaded $input (${toolkit.getPageCount()} page(s))');

    if (!toolkit.renderToDotLottieFile(output)) {
      stderr.writeln('Failed to render $output');
      exit(1);
    }
    print('Wrote $output');
  } finally {
    toolkit.dispose();
  }
}
