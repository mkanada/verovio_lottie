/// Widget compartilhado para exibir partituras animadas (dotLottie) geradas
/// pelo verovio_lottie — usado pelo `compare` e pelo zywny (ver
/// `docs/descricao-do-projeto.md` do verovio_lottie para o contexto do
/// projeto).
///
/// Embute:
/// - `dotlottie_flutter` (via [ScoreViewer]/[LottieFileServer]).
/// - As fontes usadas na renderização Verovio das partituras (Liberation +
///   SMuFL: Bravura, Leipzig, Gootville, Petaluma, Leland) — ver
///   [verovioResourcePath].
library;

export 'src/lottie_file_server.dart';
export 'src/resources.dart';
export 'src/score_pages.dart';
export 'src/score_viewer.dart';
