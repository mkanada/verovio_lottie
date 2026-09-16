import 'dart:io';

/// Minimal localhost HTTP server that serves one `.lottie` file.
///
/// `dotlottie_flutter`'s [DotLottieView] only accepts `url` / `asset` /
/// `json` sources — there is no `file` source type (see
/// `dotlottie_flutter.dart`'s `_DotLottieDesktopWidgetState._loadAnimation`:
/// desktop handles only `json` / `url` / `asset`). Pointing the view at this
/// server's URL is how a freshly rendered local file gets displayed without
/// forking the plugin.
class LottieFileServer {
  HttpServer? _server;
  String? _filePath;

  bool get isServing => _server != null;

  /// Serves [filePath] at `http://127.0.0.1:<port>/score.lottie` and returns
  /// that URL. Any previously served file is closed first.
  Future<Uri> serveFile(String filePath) async {
    await close();
    _filePath = filePath;
    _server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
    _server!.listen(_handleRequest);
    return Uri.parse('http://127.0.0.1:${_server!.port}/score.lottie');
  }

  Future<void> _handleRequest(HttpRequest request) async {
    try {
      if (request.uri.path != '/score.lottie') {
        request.response.statusCode = HttpStatus.notFound;
        await request.response.close();
        return;
      }
      final file = File(_filePath!);
      if (!await file.exists()) {
        request.response.statusCode = HttpStatus.notFound;
        await request.response.close();
        return;
      }
      // `.lottie` is a zip package; octet-stream makes every native player
      // treat it as binary dotLottie data (the desktop player's loadUrl
      // branches on `.json` suffix, everything else goes to loadBytes).
      request.response.headers.contentType =
          ContentType('application', 'octet-stream');
      request.response.headers.add('Access-Control-Allow-Origin', '*');
      request.response.headers.add('Cache-Control', 'no-store');
      await request.response.addStream(file.openRead());
      await request.response.close();
    } catch (_) {
      try {
        request.response.statusCode = HttpStatus.internalServerError;
        await request.response.close();
      } catch (_) {
        // Response already gone; nothing left to do.
      }
    }
  }

  Future<void> close() async {
    await _server?.close(force: true);
    _server = null;
    _filePath = null;
  }
}
