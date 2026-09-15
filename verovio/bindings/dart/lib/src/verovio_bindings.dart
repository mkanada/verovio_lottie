// Low-level dart:ffi signatures for verovio/tools/c_wrapper.h.
//
// One-to-one mirror of the C API (same functions, same order) so that a
// diff against c_wrapper.h is enough to keep this in sync. Callers should
// use VerovioToolkit (verovio_toolkit.dart) instead of this class directly.
import 'dart:ffi';

import 'package:ffi/ffi.dart';

typedef _VoidFromBoolNative = Void Function(Bool);
typedef _VoidFromBoolDart = void Function(bool);

typedef _PtrFromVoidNative = Pointer<Void> Function();
typedef _PtrFromVoidDart = Pointer<Void> Function();

typedef _PtrFromPtrStrNative = Pointer<Void> Function(Pointer<Utf8>);
typedef _PtrFromPtrStrDart = Pointer<Void> Function(Pointer<Utf8>);

typedef _VoidFromPtrNative = Void Function(Pointer<Void>);
typedef _VoidFromPtrDart = void Function(Pointer<Void>);

typedef _Utf8FromPtrNative = Pointer<Utf8> Function(Pointer<Void>);
typedef _Utf8FromPtrDart = Pointer<Utf8> Function(Pointer<Void>);

typedef _Utf8FromPtrStrNative = Pointer<Utf8> Function(
    Pointer<Void>, Pointer<Utf8>);
typedef _Utf8FromPtrStrDart = Pointer<Utf8> Function(
    Pointer<Void>, Pointer<Utf8>);

typedef _Utf8FromPtrStrStrNative = Pointer<Utf8> Function(
    Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _Utf8FromPtrStrStrDart = Pointer<Utf8> Function(
    Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _Utf8FromPtrIntNative = Pointer<Utf8> Function(Pointer<Void>, Int32);
typedef _Utf8FromPtrIntDart = Pointer<Utf8> Function(Pointer<Void>, int);

typedef _Utf8FromPtrIntBoolNative = Pointer<Utf8> Function(
    Pointer<Void>, Int32, Bool);
typedef _Utf8FromPtrIntBoolDart = Pointer<Utf8> Function(
    Pointer<Void>, int, bool);

typedef _BoolFromPtrStrNative = Bool Function(Pointer<Void>, Pointer<Utf8>);
typedef _BoolFromPtrStrDart = bool Function(Pointer<Void>, Pointer<Utf8>);

typedef _BoolFromPtrStrIntNative = Bool Function(
    Pointer<Void>, Pointer<Utf8>, Int32);
typedef _BoolFromPtrStrIntDart = bool Function(
    Pointer<Void>, Pointer<Utf8>, int);

typedef _BoolFromPtrStrStrNative = Bool Function(
    Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);
typedef _BoolFromPtrStrStrDart = bool Function(
    Pointer<Void>, Pointer<Utf8>, Pointer<Utf8>);

typedef _BoolFromPtrIntNative = Bool Function(Pointer<Void>, Int32);
typedef _BoolFromPtrIntDart = bool Function(Pointer<Void>, int);

typedef _BoolFromPtrBufIntNative = Bool Function(
    Pointer<Void>, Pointer<Uint8>, Int32);
typedef _BoolFromPtrBufIntDart = bool Function(
    Pointer<Void>, Pointer<Uint8>, int);

typedef _IntFromPtrNative = Int32 Function(Pointer<Void>);
typedef _IntFromPtrDart = int Function(Pointer<Void>);

typedef _IntFromPtrStrNative = Int32 Function(Pointer<Void>, Pointer<Utf8>);
typedef _IntFromPtrStrDart = int Function(Pointer<Void>, Pointer<Utf8>);

typedef _DoubleFromPtrStrNative = Double Function(Pointer<Void>, Pointer<Utf8>);
typedef _DoubleFromPtrStrDart = double Function(Pointer<Void>, Pointer<Utf8>);

typedef _VoidFromPtrStrNative = Void Function(Pointer<Void>, Pointer<Utf8>);
typedef _VoidFromPtrStrDart = void Function(Pointer<Void>, Pointer<Utf8>);

typedef _VoidFromPtrIntNative = Void Function(Pointer<Void>, Int32);
typedef _VoidFromPtrIntDart = void Function(Pointer<Void>, int);

/// Direct 1:1 bindings to the `vrvToolkit_*` C functions, resolved once from
/// the opened [DynamicLibrary].
class VerovioBindings {
  VerovioBindings(DynamicLibrary lib)
      : enableLog = lib.lookupFunction<_VoidFromBoolNative, _VoidFromBoolDart>(
            'enableLog'),
        enableLogToBuffer =
            lib.lookupFunction<_VoidFromBoolNative, _VoidFromBoolDart>(
                'enableLogToBuffer'),
        constructor = lib.lookupFunction<_PtrFromVoidNative, _PtrFromVoidDart>(
            'vrvToolkit_constructor'),
        constructorResourcePath =
            lib.lookupFunction<_PtrFromPtrStrNative, _PtrFromPtrStrDart>(
                'vrvToolkit_constructorResourcePath'),
        constructorNoResource =
            lib.lookupFunction<_PtrFromVoidNative, _PtrFromVoidDart>(
                'vrvToolkit_constructorNoResource'),
        destructor = lib.lookupFunction<_VoidFromPtrNative, _VoidFromPtrDart>(
            'vrvToolkit_destructor'),
        edit = lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
            'vrvToolkit_edit'),
        editResponse = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_editResponse'),
        editStatus = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_editStatus'),
        getAvailableOptions =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_getAvailableOptions'),
        getDefaultOptions =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_getDefaultOptions'),
        getDescriptiveFeatures =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_getDescriptiveFeatures'),
        getElementAttr = lib.lookupFunction<_Utf8FromPtrStrStrNative,
            _Utf8FromPtrStrStrDart>('vrvToolkit_getElementAttr'),
        getElementsAtTime =
            lib.lookupFunction<_Utf8FromPtrIntNative, _Utf8FromPtrIntDart>(
                'vrvToolkit_getElementsAtTime'),
        getExpansionIdsForElement =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_getExpansionIdsForElement'),
        getHumdrum = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_getHumdrum'),
        getHumdrumFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_getHumdrumFile'),
        getID = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_getID'),
        convertHumdrumToHumdrum =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_convertHumdrumToHumdrum'),
        convertHumdrumToMIDI =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_convertHumdrumToMIDI'),
        convertMEIToHumdrum =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_convertMEIToHumdrum'),
        getLog = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_getLog'),
        getMEI = lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
            'vrvToolkit_getMEI'),
        getMIDIValuesForElement =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_getMIDIValuesForElement'),
        getNotatedIdForElement =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_getNotatedIdForElement'),
        getOptions = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_getOptions'),
        getOptionUsageString =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_getOptionUsageString'),
        getPageCount = lib.lookupFunction<_IntFromPtrNative, _IntFromPtrDart>(
            'vrvToolkit_getPageCount'),
        getPageWithElement =
            lib.lookupFunction<_IntFromPtrStrNative, _IntFromPtrStrDart>(
                'vrvToolkit_getPageWithElement'),
        getResourcePath =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_getResourcePath'),
        getScale = lib.lookupFunction<_IntFromPtrNative, _IntFromPtrDart>(
            'vrvToolkit_getScale'),
        getTimeForElement =
            lib.lookupFunction<_DoubleFromPtrStrNative, _DoubleFromPtrStrDart>(
                'vrvToolkit_getTimeForElement'),
        getTimesForElement =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_getTimesForElement'),
        getVersion = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_getVersion'),
        loadData =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_loadData'),
        loadFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_loadFile'),
        loadZipDataBase64 =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_loadZipDataBase64'),
        loadZipDataBuffer = lib.lookupFunction<_BoolFromPtrBufIntNative,
            _BoolFromPtrBufIntDart>('vrvToolkit_loadZipDataBuffer'),
        redoLayout =
            lib.lookupFunction<_VoidFromPtrStrNative, _VoidFromPtrStrDart>(
                'vrvToolkit_redoLayout'),
        redoPagePitchPosLayout =
            lib.lookupFunction<_VoidFromPtrNative, _VoidFromPtrDart>(
                'vrvToolkit_redoPagePitchPosLayout'),
        renderData = lib.lookupFunction<_Utf8FromPtrStrStrNative,
            _Utf8FromPtrStrStrDart>('vrvToolkit_renderData'),
        renderToDotLottieFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_renderToDotLottieFile'),
        renderToDotLottieHighlightFile = lib
            .lookupFunction<_BoolFromPtrStrIntNative, _BoolFromPtrStrIntDart>(
                'vrvToolkit_renderToDotLottieHighlightFile'),
        renderToExpansionMap =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_renderToExpansionMap'),
        renderToExpansionMapFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_renderToExpansionMapFile'),
        renderToLottie =
            lib.lookupFunction<_Utf8FromPtrIntNative, _Utf8FromPtrIntDart>(
                'vrvToolkit_renderToLottie'),
        renderToLottieAnimation =
            lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
                'vrvToolkit_renderToLottieAnimation'),
        renderToLottieFile = lib.lookupFunction<_BoolFromPtrStrIntNative,
            _BoolFromPtrStrIntDart>('vrvToolkit_renderToLottieFile'),
        renderToMIDI = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_renderToMIDI'),
        renderToMIDIFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_renderToMIDIFile'),
        renderToPAE = lib.lookupFunction<_Utf8FromPtrNative, _Utf8FromPtrDart>(
            'vrvToolkit_renderToPAE'),
        renderToPAEFile =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_renderToPAEFile'),
        renderToSVG = lib.lookupFunction<_Utf8FromPtrIntBoolNative,
            _Utf8FromPtrIntBoolDart>('vrvToolkit_renderToSVG'),
        renderToSVGFile = lib.lookupFunction<_BoolFromPtrStrIntNative,
            _BoolFromPtrStrIntDart>('vrvToolkit_renderToSVGFile'),
        renderToTimemap =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_renderToTimemap'),
        renderToTimemapFile = lib.lookupFunction<_BoolFromPtrStrStrNative,
            _BoolFromPtrStrStrDart>('vrvToolkit_renderToTimemapFile'),
        resetOptions = lib.lookupFunction<_VoidFromPtrNative, _VoidFromPtrDart>(
            'vrvToolkit_resetOptions'),
        resetXmlIdSeed =
            lib.lookupFunction<_VoidFromPtrIntNative, _VoidFromPtrIntDart>(
                'vrvToolkit_resetXmlIdSeed'),
        saveFile = lib.lookupFunction<_BoolFromPtrStrStrNative,
            _BoolFromPtrStrStrDart>('vrvToolkit_saveFile'),
        select = lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
            'vrvToolkit_select'),
        setInputFrom =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_setInputFrom'),
        setOptions =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_setOptions'),
        setOutputTo =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_setOutputTo'),
        setResourcePath =
            lib.lookupFunction<_BoolFromPtrStrNative, _BoolFromPtrStrDart>(
                'vrvToolkit_setResourcePath'),
        setScale =
            lib.lookupFunction<_BoolFromPtrIntNative, _BoolFromPtrIntDart>(
                'vrvToolkit_setScale'),
        validatePAE =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_validatePAE'),
        validatePAEFile =
            lib.lookupFunction<_Utf8FromPtrStrNative, _Utf8FromPtrStrDart>(
                'vrvToolkit_validatePAEFile');

  final _VoidFromBoolDart enableLog;
  final _VoidFromBoolDart enableLogToBuffer;

  final _PtrFromVoidDart constructor;
  final _PtrFromPtrStrDart constructorResourcePath;
  final _PtrFromVoidDart constructorNoResource;

  final _VoidFromPtrDart destructor;
  final _BoolFromPtrStrDart edit;
  final _Utf8FromPtrDart editResponse;
  final _Utf8FromPtrDart editStatus;
  final _Utf8FromPtrDart getAvailableOptions;
  final _Utf8FromPtrDart getDefaultOptions;
  final _Utf8FromPtrStrDart getDescriptiveFeatures;
  final _Utf8FromPtrStrStrDart getElementAttr;
  final _Utf8FromPtrIntDart getElementsAtTime;
  final _Utf8FromPtrStrDart getExpansionIdsForElement;
  final _Utf8FromPtrDart getHumdrum;
  final _BoolFromPtrStrDart getHumdrumFile;
  final _Utf8FromPtrDart getID;
  final _Utf8FromPtrStrDart convertHumdrumToHumdrum;
  final _Utf8FromPtrStrDart convertHumdrumToMIDI;
  final _Utf8FromPtrStrDart convertMEIToHumdrum;
  final _Utf8FromPtrDart getLog;
  final _Utf8FromPtrStrDart getMEI;
  final _Utf8FromPtrStrDart getMIDIValuesForElement;
  final _Utf8FromPtrStrDart getNotatedIdForElement;
  final _Utf8FromPtrDart getOptions;
  final _Utf8FromPtrDart getOptionUsageString;
  final _IntFromPtrDart getPageCount;
  final _IntFromPtrStrDart getPageWithElement;
  final _Utf8FromPtrDart getResourcePath;
  final _IntFromPtrDart getScale;
  final _DoubleFromPtrStrDart getTimeForElement;
  final _Utf8FromPtrStrDart getTimesForElement;
  final _Utf8FromPtrDart getVersion;
  final _BoolFromPtrStrDart loadData;
  final _BoolFromPtrStrDart loadFile;
  final _BoolFromPtrStrDart loadZipDataBase64;
  final _BoolFromPtrBufIntDart loadZipDataBuffer;
  final _VoidFromPtrStrDart redoLayout;
  final _VoidFromPtrDart redoPagePitchPosLayout;
  final _Utf8FromPtrStrStrDart renderData;
  final _BoolFromPtrStrDart renderToDotLottieFile;
  final _BoolFromPtrStrIntDart renderToDotLottieHighlightFile;
  final _Utf8FromPtrDart renderToExpansionMap;
  final _BoolFromPtrStrDart renderToExpansionMapFile;
  final _Utf8FromPtrIntDart renderToLottie;
  final _Utf8FromPtrDart renderToLottieAnimation;
  final _BoolFromPtrStrIntDart renderToLottieFile;
  final _Utf8FromPtrDart renderToMIDI;
  final _BoolFromPtrStrDart renderToMIDIFile;
  final _Utf8FromPtrDart renderToPAE;
  final _BoolFromPtrStrDart renderToPAEFile;
  final _Utf8FromPtrIntBoolDart renderToSVG;
  final _BoolFromPtrStrIntDart renderToSVGFile;
  final _Utf8FromPtrStrDart renderToTimemap;
  final _BoolFromPtrStrStrDart renderToTimemapFile;
  final _VoidFromPtrDart resetOptions;
  final _VoidFromPtrIntDart resetXmlIdSeed;
  final _BoolFromPtrStrStrDart saveFile;
  final _BoolFromPtrStrDart select;
  final _BoolFromPtrStrDart setInputFrom;
  final _BoolFromPtrStrDart setOptions;
  final _BoolFromPtrStrDart setOutputTo;
  final _BoolFromPtrStrDart setResourcePath;
  final _BoolFromPtrIntDart setScale;
  final _Utf8FromPtrStrDart validatePAE;
  final _Utf8FromPtrStrDart validatePAEFile;
}
