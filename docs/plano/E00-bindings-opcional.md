# E00 — Bindings JS/Python (opcional)

**Status:** opcional — só executar se o usuário pedir (por exemplo, se o zywny
precisar gerar `.lottie` no navegador).

Detalhar em passos `E01…` antes de executar. Pontos de partida:

- **C**: `verovio/tools/c_wrapper.h` L65-L68 e `verovio/tools/c_wrapper.cpp`
  L335-L359 (padrão de `renderToSVG`/`renderToSVGFile`).
- **Emscripten**: `verovio/emscripten/exports.txt` L37-L38;
  `verovio/emscripten/npm/src/verovio-toolkit.js`.
- **Python**: `verovio/bindings/python/verovio.i` (SWIG) e
  `verovio/bindings/python/verovio.pyi` (tipagem).
- O `.lottie` é binário (zip): expor ao JS como bytes ou base64 exige decidir a
  API. `RenderToLottieAnimation()` (A12) já entrega a animação como string JSON,
  o que pode bastar para uso web.
- Métodos marcados com `@remark nojs` não são expostos ao JS.
