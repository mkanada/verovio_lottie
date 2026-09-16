/// Renderização de Lottie/dotLottie para pixels via FFI direto no
/// `libdotlottie_rs.so` que o pacote `dotlottie_flutter` empacota no build
/// Linux (o mesmo motor que o widget `DotLottieView` usa nessa plataforma).
///
/// Por que FFI direto em vez do widget `DotLottieView`?
/// - O widget no desktop só carrega de `url`/`asset`/`json`: um `.lottie`
///   arbitrário do disco (com fontes embutidas e state machines) não é
///   carregável por ele; aqui usamos `loadBytes`, preservando o pacote todo.
/// - O widget não expõe slots de cor (modo interativo M3) nem avanço manual
///   de relógio (`tick` de 1ms), ambos necessários para replicar
///   `lottie-to-png --slot` e `sm-render` da versão anterior.
/// - Sem widget não há janela/textura: o binário roda headless sob `xvfb-run`
///   (só porque o embedder Linux exige um display) sem depender de captura
///   de tela — os pixels saem do software renderer (ThorVG) como antes.
///
/// Assinaturas C verificadas contra o `dotlottie-rs` vendorizado em
/// `dotlottie-rs/src/c_api/mod.rs` e contra os bindings do próprio
/// `dotlottie_flutter` (`src/ffi/dotlottie_bindings.dart`, v0.1.7).
/// `ColorSpace::ARGB8888S` (alpha reto, valor 3 — ver `renderer/backend.rs`)
/// é usado de propósito: ver a nota D05 no README sobre dessaturação com
/// `ARGB8888` premultiplicado.
library;

import 'dart:ffi';
import 'dart:io';
import 'dart:typed_data';

import 'package:ffi/ffi.dart';

final class _Player extends Opaque {}

final class _StateMachine extends Opaque {}

/// dotlottieDotLottieResult (só o Success importa na prática).
const int _kResultSuccess = 0;

/// dotlottieColorSpace::ARGB8888S — alpha reto (straight), não premultiplicado.
const int _kColorSpaceArgb8888S = 3;

final DynamicLibrary _lib = _openLibrary();

DynamicLibrary _openLibrary() {
  if (!Platform.isLinux) {
    throw UnsupportedError(
        'compare só suporta Linux (libdotlottie_rs.so do dotlottie_flutter)');
  }
  // 1) override explícito (útil em `flutter test`).
  final env = Platform.environment['COMPARE_DOTLOTTIE_SO'];
  if (env != null && env.isNotEmpty) return DynamicLibrary.open(env);
  // 2) ao lado do executável: `flutter build linux` coloca as .so do plugin
  //    em <bundle>/lib/.
  final exeDir = File(Platform.resolvedExecutable).parent;
  for (final candidate in [
    '${exeDir.path}/lib/libdotlottie_rs.so',
    '${exeDir.path}/libdotlottie_rs.so',
  ]) {
    if (File(candidate).existsSync()) return DynamicLibrary.open(candidate);
  }
  // 3) caminho de busca do sistema (LD_LIBRARY_PATH).
  return DynamicLibrary.open('libdotlottie_rs.so');
}

final _newPlayer = _lib.lookupFunction<
    Pointer<_Player> Function(Uint32),
    Pointer<_Player> Function(int)>('dotlottie_new_player');

final _destroy = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>),
    int Function(Pointer<_Player>)>('dotlottie_destroy');

final _loadAnimationData = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Pointer<Utf8>),
    int Function(
        Pointer<_Player>, Pointer<Utf8>)>('dotlottie_load_animation_data');

final _loadDotlottieData = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Pointer<Uint8>, UintPtr),
    int Function(Pointer<_Player>, Pointer<Uint8>,
        int)>('dotlottie_load_dotlottie_data');

final _setSwTarget = _lib.lookupFunction<
    Int32 Function(
        Pointer<_Player>, Pointer<Uint32>, Uint32, Uint32, Int32),
    int Function(Pointer<_Player>, Pointer<Uint32>, int, int,
        int)>('dotlottie_set_sw_target');

final _setViewport = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Int32, Int32, Int32, Int32),
    int Function(
        Pointer<_Player>, int, int, int, int)>('dotlottie_set_viewport');

final _setFrame = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Float),
    int Function(Pointer<_Player>, double)>('dotlottie_set_frame');

final _render = _lib.lookupFunction<Int32 Function(Pointer<_Player>),
    int Function(Pointer<_Player>)>('dotlottie_render');

final _getTotalFrames = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Pointer<Float>),
    int Function(
        Pointer<_Player>, Pointer<Float>)>('dotlottie_get_total_frames');

final _smLoad = _lib.lookupFunction<
    Pointer<_StateMachine> Function(Pointer<_Player>, Pointer<Utf8>),
    Pointer<_StateMachine> Function(Pointer<_Player>,
        Pointer<Utf8>)>('dotlottie_state_machine_load');

final _smLoadData = _lib.lookupFunction<
    Pointer<_StateMachine> Function(Pointer<_Player>, Pointer<Utf8>),
    Pointer<_StateMachine> Function(Pointer<_Player>,
        Pointer<Utf8>)>('dotlottie_state_machine_load_data');

final _smStart = _lib.lookupFunction<
    Int32 Function(Pointer<_StateMachine>, Pointer<Utf8>, Bool),
    int Function(Pointer<_StateMachine>, Pointer<Utf8>,
        bool)>('dotlottie_state_machine_start');

final _smRelease = _lib.lookupFunction<
    Void Function(Pointer<_StateMachine>),
    void Function(
        Pointer<_StateMachine>)>('dotlottie_state_machine_release');

final _smTick = _lib.lookupFunction<
    Int32 Function(Pointer<_StateMachine>, Float, Pointer<Bool>),
    int Function(Pointer<_StateMachine>, double,
        Pointer<Bool>)>('dotlottie_state_machine_tick');

final _smFire = _lib.lookupFunction<
    Int32 Function(Pointer<_StateMachine>, Pointer<Utf8>),
    int Function(Pointer<_StateMachine>,
        Pointer<Utf8>)>('dotlottie_state_machine_fire_event');

final _smCurrentState = _lib.lookupFunction<
    Int32 Function(
        Pointer<_StateMachine>, Pointer<Uint8>, Pointer<UintPtr>),
    int Function(Pointer<_StateMachine>, Pointer<Uint8>,
        Pointer<UintPtr>)>('dotlottie_state_machine_get_current_state');

final _setColorSlot = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Pointer<Utf8>, Float, Float, Float),
    int Function(Pointer<_Player>, Pointer<Utf8>, double, double,
        double)>('dotlottie_set_color_slot');

final _clearSlot = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>, Pointer<Utf8>),
    int Function(
        Pointer<_Player>, Pointer<Utf8>)>('dotlottie_clear_slot');

final _clearSlots = _lib.lookupFunction<
    Int32 Function(Pointer<_Player>),
    int Function(Pointer<_Player>)>('dotlottie_clear_slots');

// dotlottie_load_font/unload_font não recebem Pointer<_Player>: é um
// registro de fontes global do motor (ThorVG `tvg_font_load_data`), não
// por instância - por isso NativePlayer.loadFont/unloadFont abaixo são
// `static`, não métodos de instância.
final _loadFont = _lib.lookupFunction<
    Int32 Function(Pointer<Utf8>, Pointer<Uint8>, IntPtr),
    int Function(
        Pointer<Utf8>, Pointer<Uint8>, int)>('dotlottie_load_font');

final _unloadFont = _lib.lookupFunction<Int32 Function(Pointer<Utf8>),
    int Function(Pointer<Utf8>)>('dotlottie_unload_font');

/// Exceção para falhas do motor nativo com contexto da operação.
class NativeError extends StateError {
  NativeError(super.message);
}

/// Player de software (headless) sobre o dotlottie-rs empacotado.
///
/// O buffer é ARGB8888S (alpha reto): cada u32 é
/// `(a << 24) | (r << 16) | (g << 8) | b`, com RGB **não** premultiplicado.
class NativePlayer {
  final Pointer<_Player> _ptr;
  Pointer<Uint32>? _pixels;
  int width = 0;
  int height = 0;
  bool _disposed = false;

  NativePlayer() : _ptr = _newPlayer(1) {
    if (_ptr == nullptr) {
      throw NativeError('dotlottie_new_player retornou nulo');
    }
  }

  /// Aloca o destino de software. Precisa existir ANTES do load (o load já
  /// renderiza o frame inicial contra o alvo configurado).
  void setTarget(int w, int h) {
    _check(w > 0 && h > 0, 'dimensões precisam ser maiores que zero');
    if (_pixels != null) malloc.free(_pixels!);
    _pixels = malloc<Uint32>(w * h);
    width = w;
    height = h;
    _check(_setSwTarget(_ptr, _pixels!, w, h, _kColorSpaceArgb8888S) ==
        _kResultSuccess, 'set_sw_target falhou');
    _setViewport(_ptr, 0, 0, w, h);
  }

  void loadBytes(Uint8List bytes) {
    final ptr = malloc<Uint8>(bytes.length);
    try {
      ptr.asTypedList(bytes.length).setAll(0, bytes);
      _check(_loadDotlottieData(_ptr, ptr, bytes.length) == _kResultSuccess,
          'load_dotlottie_data falhou — arquivo é um .lottie válido?');
    } finally {
      malloc.free(ptr);
    }
  }

  void loadJson(String json) {
    final ptr = json.toNativeUtf8();
    try {
      _check(_loadAnimationData(_ptr, ptr) == _kResultSuccess,
          'load_animation_data falhou — JSON é um Lottie válido?');
    } finally {
      malloc.free(ptr);
    }
  }

  double totalFrames() {
    final out = malloc<Float>();
    try {
      _check(_getTotalFrames(_ptr, out) == _kResultSuccess,
          'get_total_frames falhou');
      return out.value;
    } finally {
      malloc.free(out);
    }
  }

  /// Posiciona em [frame] e renderiza. O no-op do ThorVG quando o frame já é
  /// o corrente (ver README) é inofensivo: o buffer já está correto.
  void seekFrame(double frame) {
    _setFrame(_ptr, frame);
    _render(_ptr);
  }

  /// Força o flush do estado atual para o buffer (inofensivo sem mudanças).
  void flush() => _render(_ptr);

  /// Cópia atual do buffer ARGB (alpha reto) — válida após load/seek/tick.
  Uint32List snapshot() {
    final p = _pixels;
    if (p == null) throw NativeError('setTarget() ainda não foi chamado');
    return Uint32List.fromList(p.asTypedList(width * height));
  }

  void setColorSlot(String id, double r, double g, double b) {
    final idPtr = id.toNativeUtf8();
    try {
      _check(_setColorSlot(_ptr, idPtr, r, g, b) == _kResultSuccess,
          'set_color_slot($id) falhou');
    } finally {
      malloc.free(idPtr);
    }
  }

  void clearSlot(String id) {
    final idPtr = id.toNativeUtf8();
    try {
      _check(_clearSlot(_ptr, idPtr) == _kResultSuccess,
          'clear_slot($id) falhou');
    } finally {
      malloc.free(idPtr);
    }
  }

  void clearSlots() {
    _check(
        _clearSlots(_ptr) == _kResultSuccess, 'clear_slots() falhou');
  }

  /// Registra [bytes] (uma fonte TTF) no motor sob [name], globalmente para
  /// todo `NativePlayer` do processo (ver a nota acima de [_loadFont]) - o
  /// carregador de Lottie do ThorVG resolve `fName`/`fFamily` de um `ty:5`
  /// contra esse registro mesmo quando a fonte não está embutida no pacote
  /// (`fonts.list` sem `fPath`). Precisa acontecer antes de renderizar
  /// qualquer frame que use essa fonte.
  static void loadFont(String name, Uint8List bytes) {
    final namePtr = name.toNativeUtf8();
    final dataPtr = malloc<Uint8>(bytes.length);
    try {
      dataPtr.asTypedList(bytes.length).setAll(0, bytes);
      if (_loadFont(namePtr, dataPtr, bytes.length) != _kResultSuccess) {
        throw NativeError('load_font($name) falhou');
      }
    } finally {
      malloc.free(namePtr);
      malloc.free(dataPtr);
    }
  }

  static void unloadFont(String name) {
    final namePtr = name.toNativeUtf8();
    try {
      if (_unloadFont(namePtr) != _kResultSuccess) {
        throw NativeError('unload_font($name) falhou');
      }
    } finally {
      malloc.free(namePtr);
    }
  }

  NativeStateMachine loadStateMachineById(String id) {
    final idPtr = id.toNativeUtf8();
    try {
      final sm = _smLoad(_ptr, idPtr);
      if (sm == nullptr) {
        throw NativeError(
            'state_machine_load($id) falhou — id existe no manifest (s/<id>.json)?');
      }
      return NativeStateMachine(sm);
    } finally {
      malloc.free(idPtr);
    }
  }

  NativeStateMachine loadStateMachineData(String json) {
    final jsonPtr = json.toNativeUtf8();
    try {
      final sm = _smLoadData(_ptr, jsonPtr);
      if (sm == nullptr) {
        throw NativeError('state_machine_load_data() falhou');
      }
      return NativeStateMachine(sm);
    } finally {
      malloc.free(jsonPtr);
    }
  }

  void dispose() {
    if (_disposed) return;
    _disposed = true;
    if (_pixels != null) {
      malloc.free(_pixels!);
      _pixels = null;
    }
    _destroy(_ptr);
  }

  void _check(bool ok, String message) {
    if (!ok) throw NativeError(message);
  }
}

/// State machine carregada — avança por [tick] manual (ms), como a versão
/// anterior (`engine.tick(1.0)` por milissegundo de roteiro).
class NativeStateMachine {
  final Pointer<_StateMachine> _ptr;
  bool _disposed = false;

  NativeStateMachine(this._ptr);

  void start() {
    final rc = _smStart(_ptr, nullptr.cast(), false);
    if (rc != _kResultSuccess) {
      throw NativeError('state_machine_start() falhou (rc=$rc)');
    }
  }

  /// Avança [dtMs] milissegundos. Devolve se um frame novo foi renderizado.
  bool tick(double dtMs) {
    final rendered = malloc<Bool>();
    try {
      final rc = _smTick(_ptr, dtMs, rendered);
      if (rc != _kResultSuccess) {
        throw NativeError('state_machine_tick() falhou (rc=$rc)');
      }
      return rendered.value;
    } finally {
      malloc.free(rendered);
    }
  }

  void fire(String event) {
    final e = event.toNativeUtf8();
    try {
      final rc = _smFire(_ptr, e);
      if (rc != _kResultSuccess) {
        throw NativeError(
            'fire($event) falhou (rc=$rc) — nome declarado como Event input?');
      }
    } finally {
      malloc.free(e);
    }
  }

  String currentState() {
    final sizeOut = malloc<UintPtr>();
    try {
      if (_smCurrentState(_ptr, nullptr.cast<Uint8>(), sizeOut) !=
          _kResultSuccess) {
        return '?';
      }
      final size = sizeOut.value;
      if (size == 0) return '?';
      final buf = malloc<Uint8>(size);
      try {
        if (_smCurrentState(_ptr, buf, sizeOut) != _kResultSuccess) return '?';
        return buf.cast<Utf8>().toDartString();
      } finally {
        malloc.free(buf);
      }
    } finally {
      malloc.free(sizeOut);
    }
  }

  void dispose() {
    if (_disposed) return;
    _disposed = true;
    _smRelease(_ptr);
  }
}
