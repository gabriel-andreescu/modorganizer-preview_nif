"""Checks the animation the preview plays: the values its keys give at a moment, and its named animations."""

from pathlib import Path

import pytest
from mopk.testing import wait_for

# A fire effect whose three controllers run by themselves: two curved keys that loop from a late start, and a
# straight ramp. The values are the file's keys worked out outside the plugin for this moment.
FIRE = "meshes/effects/bfxfirevol01.nif"
FIRE_TIME = 0.4
FIRE_VALUES = [
    {"vOffset": 0.04465147, "uOffset": 0.00777892},
    {"vOffset": -0.11111111},
]

# Its controllers follow one of three named animations. The looping one is not the first.
SHIELD = "meshes/effects/fireshieldhiteffect.nif"
SHIELD_ANIMATIONS = ["mBegin", "mLoop", "mEnd"]
# mBegin plays once, over this many seconds.
SHIELD_BEGIN_LENGTH = 0.30001020431518555
# An animation that plays once keeps its last pose this many seconds before it starts over.
HOLD = 1.0

# A piece of falling roof whose rotation keys are the smooth kind, at a moment where the curve between two keys runs
# far from the straight blend between them. The rows are the file's keys worked out outside the plugin.
RUBBLE = "meshes/dlc01/architecture/snowelfruins/dlc01sebf_holedrop06.nif"
RUBBLE_ANIMATIONS = ["stage1", "stage2", "stage3"]
RUBBLE_TIME = 3.85
RUBBLE_PIECE = "Plane06_frag_70:0"
RUBBLE_ROTATION = [
    [0.95170262, 0.02423521, -0.30606335],
    [-0.13601397, 0.92699936, -0.34953166],
    [0.27524956, 0.37427909, 0.88552405],
]

# Headwear's face bones file. Its controllers place the bones in a skeleton the file doesn't have.
HELMET_FACE_BONES = "meshes/armor/dcguard/mhelmetheavy_facebones.nif"

# A mesh without animation.
BARREL = "meshes/clutter/barrel01.nif"

# Its parts sit deep under turned and moved nodes, so a pose that is worked out again lands a hair off the stored one.
CATAPULT = "meshes/clutter/civilwar/catapult/civilwarcatapult01.nif"
# A skinned mesh, whose vertices a pose replaces.
BOOK = "meshes/clutter/books/book02/character assets/book02large01.nif"


def open_pane(devbench, path: str, **inspect) -> tuple[int, dict]:
    opened = devbench.call("preview_nif", {"action": "open", "path": path})
    details = devbench.call(
        "preview_nif", {"action": "inspect", "id": opened["id"], **inspect}
    )
    return opened["id"], details["panes"][0]


def pane(devbench, preview: int, **inspect) -> dict:
    details = devbench.call(
        "preview_nif", {"action": "inspect", "id": preview, **inspect}
    )
    return details["panes"][0]


def animated_values(devbench, preview: int, time: float) -> list[dict]:
    shapes = pane(devbench, preview, time=time)["shapes"]
    return [shape["animated"] for shape in shapes if "animated" in shape]


def numbers(values: dict) -> dict:
    """A shape's animated values with each color channel as its own number, which approx can compare."""
    return {
        f"{name}[{channel}]": number
        for name, value in values.items()
        for channel, number in enumerate(value if isinstance(value, list) else [value])
    }


def play(devbench, preview: int, animations: list[str], animation: str):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    picker = next(c for c in controls if c.get("options") == animations)
    devbench.call(
        "dialogs",
        {
            "action": "select",
            "id": picker["id"],
            "index": animations.index(animation),
        },
    )
    wait_for(
        lambda: pane(devbench, preview)["sequence"],
        lambda playing: playing == animation,
        f"Choosing {animation} did not play it",
    )


def shows_play_animation(devbench) -> bool:
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    return any(c.get("text") == "Play Animation" for c in controls)


def set_play_animation(devbench, plays: bool):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    box = next(c for c in controls if c.get("text") == "Play Animation")
    devbench.call(
        "dialogs", {"action": "setChecked", "id": box["id"], "checked": plays}
    )


def rest_capture(devbench, preview: int, checkpoint: str) -> bytes:
    result = devbench.capture(
        {
            "kind": "preview_nif",
            "checkpointId": checkpoint,
            "recording": "animation",
            "id": preview,
            "width": 512,
            "height": 512,
        }
    )
    return Path(result["path"]).read_bytes()


@pytest.mark.mo2
def test_animated_values_follow_the_keys(devbench):
    if devbench.health()["game"] != "Skyrim Special Edition":
        pytest.skip("Uses a Skyrim mesh")
    preview, shown = open_pane(devbench, FIRE, time=FIRE_TIME)
    try:
        assert shown["animated"]
        animated = [s["animated"] for s in shown["shapes"] if "animated" in s]
        assert len(animated) == len(FIRE_VALUES)
        for values, expected in zip(animated, FIRE_VALUES, strict=True):
            assert values == pytest.approx(expected, abs=1e-6)
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_smooth_rotation_keys_turn_a_part_the_way_the_game_does(devbench):
    if devbench.health()["game"] != "Skyrim Special Edition":
        pytest.skip("Uses a Skyrim mesh")
    preview, _ = open_pane(devbench, RUBBLE)
    try:
        play(devbench, preview, RUBBLE_ANIMATIONS, "stage2")
        shapes = pane(devbench, preview, time=RUBBLE_TIME)["shapes"]
        (piece,) = [shape for shape in shapes if shape["name"] == RUBBLE_PIECE]
        for row, expected in zip(
            piece["placed"]["rotation"], RUBBLE_ROTATION, strict=True
        ):
            assert row == pytest.approx(expected, abs=1e-5)
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_face_bone_poses_are_not_played(devbench):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, shown = open_pane(devbench, HELMET_FACE_BONES)
    try:
        assert not shown["animated"]
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_named_animations_open_on_the_looping_one(devbench):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, shown = open_pane(devbench, SHIELD)
    try:
        assert shown["sequences"] == SHIELD_ANIMATIONS
        assert shown["sequence"] == "mLoop"
        play(devbench, preview, SHIELD_ANIMATIONS, "mBegin")
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_animation_that_plays_once_holds_its_end_then_starts_over(devbench):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, _ = open_pane(devbench, SHIELD)
    try:
        play(devbench, preview, SHIELD_ANIMATIONS, "mBegin")
        early = animated_values(devbench, preview, 0.1)
        ended = animated_values(devbench, preview, SHIELD_BEGIN_LENGTH)
        held = animated_values(devbench, preview, SHIELD_BEGIN_LENGTH + HOLD / 2)
        again = animated_values(devbench, preview, 0.1 + SHIELD_BEGIN_LENGTH + HOLD)
        assert early != ended
        assert held == ended
        for values, expected in zip(again, early, strict=True):
            assert numbers(values) == pytest.approx(numbers(expected), abs=1e-4)
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_capture_without_a_time_shows_the_mesh_at_rest(devbench, visual):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, _ = open_pane(devbench, SHIELD)
    try:
        wait_for(
            lambda: pane(devbench, preview),
            lambda shown: "animationTime" in shown,
            "The preview did not start playing",
        )
        visual.check(
            "preview_nif",
            "fire-shield-hit",
            recording="fallout4",
            arguments={"id": preview, "width": 1024, "height": 1024},
        )
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
@pytest.mark.parametrize("mesh", [CATAPULT, BOOK])
def test_play_animation_off_shows_the_mesh_as_it_never_played(devbench, mesh):
    if devbench.health()["game"] != "Skyrim Special Edition":
        pytest.skip("Uses a Skyrim mesh")
    # The choice is kept for the profile, so the first preview switches it off for the one that is compared.
    setup, _ = open_pane(devbench, mesh)
    try:
        set_play_animation(devbench, False)
    finally:
        devbench.call("preview_nif", {"action": "close", "id": setup})

    preview, _ = open_pane(devbench, mesh)
    try:
        never_played = rest_capture(devbench, preview, "never-played")
        set_play_animation(devbench, True)
        wait_for(
            lambda: pane(devbench, preview),
            lambda shown: shown.get("animationTime", 0) > 0.2,
            "The preview did not play",
        )
        set_play_animation(devbench, False)
        assert rest_capture(devbench, preview, "played-then-off") == never_played
    finally:
        set_play_animation(devbench, True)
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_play_animation_shows_only_for_animated_meshes(devbench):
    if devbench.health()["game"] != "Skyrim Special Edition":
        pytest.skip("Uses Skyrim meshes")
    for path, animated in ((BARREL, False), (FIRE, True)):
        preview, shown = open_pane(devbench, path)
        try:
            assert shown["animated"] == animated
            assert shows_play_animation(devbench) == animated
        finally:
            devbench.call("preview_nif", {"action": "close", "id": preview})
