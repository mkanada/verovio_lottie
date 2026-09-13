#!/usr/bin/env python3
"""Gera as fixtures do spike B01 (docs/plano/B01-spike-state-machine.md).

Sem dependências externas (só stdlib: json, zipfile).

Saídas (neste mesmo diretório):
  - notes.lottie   pacote dotLottie v2: animação "score" (3 círculos n1/n2/n3
                   com marker+cor keyframada cada) + duas state machines
                   ("sm_instant" e "sm_tweened", ver E4) + manifest.
  - sm_scale.json  state machine sintética avulsa (3000 notas), para E5.
"""

import json
import zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent

FR = 30
# markers: n1 [0,15] n2 [16,31] n3 [32,47] idle [48,49) — ver nota abaixo.
NOTE_LEN = 15
# `Player::end_frame()` é o fim do SEGMENT do marker ATIVO (inclusive: o
# playhead pode ficar parado exatamente em `tm+dr`, ver
# `handle_forward_mode`/`start_frame`/`end_frame` em player.rs). Sem gap, o
# frame final de um marker (ex.: n2 em tm+dr=30) coincide com o `tm` do
# próximo (n3 em 30) — como todas as camadas compartilham UM playhead, se a
# nota atual (n2) ficar "presa" nesse frame de fronteira sem próximo evento,
# a cor keyframada da PRÓXIMA nota (n3, vermelha em seu próprio tm) also
# passa a valer, mesmo sem ninguém ter disparado n3 (achado do E2, ver
# docs/plano/spikes/B01-resultado.md). Por isso cada nota reserva
# NOTE_LEN+1 frames (1 de folga) e só usa os NOTE_LEN primeiros.
NOTE_SLOT = NOTE_LEN + 1
NOTES = ["n1", "n2", "n3"]
# "page2" não tem círculo próprio — é um marker de 1 frame parado (nenhuma cor
# keyframada nele), representando o *destino* de uma virada de página no mesmo
# state machine/engine das notas, pra testar (e) — se os dois convivem quando
# compartilham um único "current_state"/playhead (ver E-page em
# docs/plano/spikes/B01-resultado.md).
PAGE_EVENT = "page2"
STAR_TARGETS = NOTES + [PAGE_EVENT]
MARKERS = {name: {"tm": i * NOTE_SLOT, "dr": NOTE_LEN} for i, name in enumerate(NOTES)}
MARKERS["idle"] = {"tm": len(NOTES) * NOTE_SLOT, "dr": 1}
MARKERS[PAGE_EVENT] = {"tm": (len(NOTES) + 1) * NOTE_SLOT, "dr": 1}
OP = (len(NOTES) + 1) * NOTE_SLOT + 1

WIDTH, HEIGHT = 300, 100
DIAMETER = 60
CENTERS = {"n1": (50, 50), "n2": (150, 50), "n3": (250, 50)}

BLACK = [0, 0, 0, 1]
RED = [1, 0, 0, 1]
LINEAR_OUT = {"x": 0, "y": 0}
LINEAR_IN = {"x": 1, "y": 1}


def color_property(note: str) -> dict:
    """Propriedade "c" (cor de preenchimento) do círculo `note`: preta, pula pra
    vermelha exatamente no início do marker (keyframe de "hold" — h:1 — logo
    antes), e esmaece de volta pra preta até o fim do marker."""
    start = MARKERS[note]["tm"]
    end = start + MARKERS[note]["dr"]
    keyframes = []
    if start > 0:
        keyframes.append({"t": 0, "s": BLACK, "h": 1, "i": LINEAR_IN, "o": LINEAR_OUT})
    keyframes.append({"t": start, "s": RED, "i": LINEAR_IN, "o": LINEAR_OUT})
    keyframes.append({"t": end, "s": BLACK})
    return {"a": 1, "sid": f"{note}Color", "k": keyframes}


def circle_layer(note: str, index: int) -> dict:
    cx, cy = CENTERS[note]
    return {
        "ddd": 0,
        "ind": index,
        "ty": 4,
        "nm": note,
        "sr": 1,
        "ks": {
            "o": {"a": 0, "k": 100},
            "r": {"a": 0, "k": 0},
            "p": {"a": 0, "k": [cx, cy, 0]},
            "a": {"a": 0, "k": [0, 0, 0]},
            "s": {"a": 0, "k": [100, 100, 100]},
        },
        "ao": 0,
        "shapes": [
            {
                "ty": "gr",
                "it": [
                    {
                        "ty": "el",
                        "p": {"a": 0, "k": [0, 0]},
                        "s": {"a": 0, "k": [DIAMETER, DIAMETER]},
                    },
                    {"ty": "fl", "c": color_property(note), "o": {"a": 0, "k": 100}},
                    {
                        "ty": "tr",
                        "p": {"a": 0, "k": [0, 0]},
                        "a": {"a": 0, "k": [0, 0]},
                        "s": {"a": 0, "k": [100, 100]},
                        "r": {"a": 0, "k": 0},
                        "o": {"a": 0, "k": 100},
                    },
                ],
            }
        ],
        "ip": 0,
        "op": OP,
        "st": 0,
    }


def build_animation() -> dict:
    return {
        "v": "5.5.2",
        "fr": FR,
        "ip": 0,
        "op": OP,
        "w": WIDTH,
        "h": HEIGHT,
        "nm": "score",
        "ddd": 0,
        "assets": [],
        "markers": [
            {"cm": name, "tm": m["tm"], "dr": m["dr"]} for name, m in MARKERS.items()
        ],
        "layers": [circle_layer(note, i + 1) for i, note in enumerate(NOTES)],
    }


def playback_state(name: str, *, loop: bool = False, autoplay: bool = True) -> dict:
    return {
        "name": name,
        "type": "PlaybackState",
        "animation": "score",
        "segment": name,
        "autoplay": autoplay,
        "loop": loop,
        "transitions": [],
    }


def global_state(transition_type: str, *, duration: float = 0.0, easing=None) -> dict:
    transitions = []
    for target in STAR_TARGETS:
        t = {
            "type": transition_type,
            "toState": target,
            "guards": [{"type": "Event", "inputName": target}],
        }
        if transition_type == "Tweened":
            t["duration"] = duration
            t["easing"] = easing or [0, 0, 0.58, 1]
        transitions.append(t)
    return {
        "name": "GLOBAL",
        "type": "GlobalState",
        "transitions": transitions,
    }


def build_state_machine(transition_type: str, **kwargs) -> dict:
    states = [playback_state("idle", loop=False, autoplay=False)]
    states += [playback_state(note) for note in NOTES]
    states.append(playback_state(PAGE_EVENT, loop=False, autoplay=False))
    states.append(global_state(transition_type, **kwargs))
    return {
        "initial": "idle",
        "states": states,
        "inputs": [{"type": "Event", "name": target} for target in STAR_TARGETS],
        "interactions": [],
    }


def build_manifest() -> dict:
    return {
        "version": "2",
        "generator": "verovio_lottie B01 spike (compare/fixtures/b01/gen.py)",
        "animations": [{"id": "score"}],
        "stateMachines": [{"id": "sm_instant"}, {"id": "sm_tweened"}],
        "initial": {"animation": "score", "stateMachine": "sm_instant"},
    }


def build_scale_state_machine(count: int) -> dict:
    """E5: state machine sintética com `count` notas — todas com segment="idle"
    (não vamos de fato tocar cada uma; só medir tamanho e tempo de load)."""
    names = [f"note{i:04d}" for i in range(count)]
    states = [playback_state("idle", loop=False, autoplay=False)]
    states += [
        {
            "name": name,
            "type": "PlaybackState",
            "animation": "score",
            "segment": "idle",
            "autoplay": False,
            "loop": False,
            "transitions": [],
        }
        for name in names
    ]
    states.append(
        {
            "name": "GLOBAL",
            "type": "GlobalState",
            "transitions": [
                {
                    "type": "Transition",
                    "toState": name,
                    "guards": [{"type": "Event", "inputName": name}],
                }
                for name in names
            ],
        }
    )
    return {
        "initial": "idle",
        "states": states,
        "inputs": [{"type": "Event", "name": name} for name in names],
        "interactions": [],
    }


def main() -> None:
    animation = build_animation()
    manifest = build_manifest()
    sm_instant = build_state_machine("Transition")
    sm_tweened = build_state_machine("Tweened", duration=0.15, easing=[0, 0, 0.58, 1])

    lottie_path = HERE / "notes.lottie"
    with zipfile.ZipFile(lottie_path, "w", zipfile.ZIP_DEFLATED) as zf:
        zf.writestr("manifest.json", json.dumps(manifest))
        zf.writestr("a/score.json", json.dumps(animation))
        zf.writestr("s/sm_instant.json", json.dumps(sm_instant))
        zf.writestr("s/sm_tweened.json", json.dumps(sm_tweened))
    print(f"escrito {lottie_path} ({lottie_path.stat().st_size} bytes)")

    scale_path = HERE / "sm_scale.json"
    scale_json = json.dumps(build_scale_state_machine(3000))
    scale_path.write_text(scale_json)
    print(f"escrito {scale_path} ({len(scale_json)} bytes)")


if __name__ == "__main__":
    main()
