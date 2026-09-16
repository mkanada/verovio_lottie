import 'package:compare/src/script.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('parseColorSlot id:r,g,b', () {
    final s = parseColorSlot('d1e134:1,0,0');
    expect(s.id, 'd1e134');
    expect(s.rgb, [1.0, 0.0, 0.0]);
  });

  test('parseColorSlot mal formado lança', () {
    expect(() => parseColorSlot('sem-dois-pontos'), throwsFormatException);
    expect(() => parseColorSlot('id:1,2'), throwsFormatException);
  });

  test('parseScript fire/slot/clearslot/clearslots', () {
    final actions = parseScript(
        '0:fire d1e134;2750:slot d1e252:0,1,0;10:clearslot d1e252;20:clearslots');
    expect(actions.length, 4);
    expect(actions[0].ms, 0);
    expect((actions[0].op as FireOp).event, 'd1e134');
    final slot = actions[1].op as SlotOp;
    expect(slot.id, 'd1e252');
    expect(slot.rgb, [0.0, 1.0, 0.0]);
    expect((actions[2].op as ClearSlotOp).id, 'd1e252');
    expect(actions[3].op, isA<ClearSlotsOp>());
  });

  test('parseScript vazio e ação inválida', () {
    expect(parseScript(''), isEmpty);
    expect(() => parseScript('10:dançar x'), throwsFormatException);
  });

  test('parseSnap e parseSamples', () {
    expect(parseSnap('0,2750,3417'), [0, 2750, 3417]);
    expect(parseSnap(''), isEmpty);
    expect(parseSamples(['10,20']), [(10, 20)]);
    expect(() => parseSamples(['x']), throwsFormatException);
  });
}
