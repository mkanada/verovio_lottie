/// Parsing do roteiro de `sm-render` e dos argumentos repetíveis de cor/amostra.
///
/// Sintaxe idêntica à da versão Rust da ferramenta:
/// - slot: `id:r,g,b` (0-1 cada)
/// - roteiro: `ms:ação;ms:ação;...`, ações `fire <evento>`, `slot <id:r,g,b>`,
///   `clearslot <id>`, `clearslots`
/// - snap: `ms,ms,...`
/// - sample: `x,y`
library;

/// Um único `id:r,g,b` (0-1 cada) de `--slot`/da ação `slot` do roteiro.
({String id, List<double> rgb}) parseColorSlot(String s) {
  final colon = s.indexOf(':');
  if (colon < 0) {
    throw FormatException('slot mal formado (esperado id:r,g,b): $s');
  }
  final id = s.substring(0, colon).trim();
  final parts = s.substring(colon + 1).split(',').map((p) => p.trim()).toList();
  if (parts.length != 3) {
    throw FormatException('slot precisa de 3 componentes r,g,b (0-1): $s');
  }
  return (
    id: id,
    rgb: parts.map((p) {
      final v = double.tryParse(p);
      if (v == null) throw FormatException('componente inválido em $s: $p');
      return v;
    }).toList(),
  );
}

/// Ação do roteiro de `sm-render` num instante [ms].
///
/// `Slot`/`ClearSlot`/`ClearSlots` existem para testar o handoff M2 (state
/// machine, `Fire`) ↔ M3 (slot de cor) num único roteiro.
sealed class ScriptOp {
  const ScriptOp();
}

class FireOp extends ScriptOp {
  final String event;
  const FireOp(this.event);
}

class SlotOp extends ScriptOp {
  final String id;
  final List<double> rgb;
  const SlotOp(this.id, this.rgb);
}

class ClearSlotOp extends ScriptOp {
  final String id;
  const ClearSlotOp(this.id);
}

class ClearSlotsOp extends ScriptOp {
  const ClearSlotsOp();
}

class ScriptAction {
  final int ms;
  final ScriptOp op;
  const ScriptAction(this.ms, this.op);
}

List<ScriptAction> parseScript(String script) {
  final actions = <ScriptAction>[];
  for (final entry in script.split(';')) {
    final e = entry.trim();
    if (e.isEmpty) continue;
    final colon = e.indexOf(':');
    if (colon < 0) {
      throw FormatException('ação mal formada (esperado ms:ação): $e');
    }
    final ms = int.tryParse(e.substring(0, colon).trim());
    if (ms == null) {
      throw FormatException(
          'instante inválido: ${e.substring(0, colon).trim()}');
    }
    final rest = e.substring(colon + 1).trim();
    final ScriptOp op;
    if (rest.startsWith('fire ')) {
      op = FireOp(rest.substring('fire '.length).trim());
    } else if (rest.startsWith('slot ')) {
      final slot = parseColorSlot(rest.substring('slot '.length).trim());
      op = SlotOp(slot.id, slot.rgb);
    } else if (rest.startsWith('clearslot ')) {
      op = ClearSlotOp(rest.substring('clearslot '.length).trim());
    } else if (rest == 'clearslots') {
      op = const ClearSlotsOp();
    } else {
      throw FormatException(
          'ação não suportada (esperado "fire <nome>", "slot <id:r,g,b>", '
          '"clearslot <id>" ou "clearslots"): $rest');
    }
    actions.add(ScriptAction(ms, op));
  }
  return actions;
}

List<int> parseSnap(String snap) {
  return snap
      .split(',')
      .map((s) => s.trim())
      .where((s) => s.isNotEmpty)
      .map((s) {
        final v = int.tryParse(s);
        if (v == null) throw FormatException('snap inválido: $s');
        return v;
      })
      .toList();
}

List<(int, int)> parseSamples(List<String> samples) {
  return samples.map((s) {
    final comma = s.indexOf(',');
    if (comma < 0) {
      throw FormatException('--sample mal formado (esperado x,y): $s');
    }
    final x = int.tryParse(s.substring(0, comma).trim());
    final y = int.tryParse(s.substring(comma + 1).trim());
    if (x == null || y == null) {
      throw FormatException('coordenada inválida: $s');
    }
    return (x, y);
  }).toList();
}
