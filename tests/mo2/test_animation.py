"""Checks the animation the preview plays: the values its keys give at a moment, and its named animations."""

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

# A mesh without animation.
BARREL = "meshes/clutter/barrel01.nif"


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


def play(devbench, preview: int, animation: str):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    picker = next(c for c in controls if c.get("options") == SHIELD_ANIMATIONS)
    devbench.call(
        "dialogs",
        {
            "action": "select",
            "id": picker["id"],
            "index": SHIELD_ANIMATIONS.index(animation),
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
def test_named_animations_open_on_the_looping_one(devbench):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, shown = open_pane(devbench, SHIELD)
    try:
        assert shown["sequences"] == SHIELD_ANIMATIONS
        assert shown["sequence"] == "mLoop"
        play(devbench, preview, "mBegin")
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview})


@pytest.mark.mo2
def test_animation_that_plays_once_starts_over(devbench):
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Uses a Fallout 4 mesh")
    preview, _ = open_pane(devbench, SHIELD)
    try:
        play(devbench, preview, "mBegin")
        early = animated_values(devbench, preview, 0.1)
        late = animated_values(devbench, preview, 0.25)
        again = animated_values(devbench, preview, 0.1 + SHIELD_BEGIN_LENGTH)
        assert early != late
        for values, expected in zip(again, early, strict=True):
            assert values == pytest.approx(expected, abs=1e-4)
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
