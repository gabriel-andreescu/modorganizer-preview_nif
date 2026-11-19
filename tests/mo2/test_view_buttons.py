"""Checks that the viewport's view buttons turn the camera to each axis view and, clicked again, to its opposite."""

import pytest
from mopk.testing import wait_for

# Upright barrels, which open from the front.
MESHES = {
    "Skyrim Special Edition": "meshes/clutter/barrel01.nif",
    "Fallout 4": "meshes/setdressing/metalbarrel/metalbarrel01.nif",
}

# The tooltip each button starts with, and the yaw and pitch after its first and second click.
BUTTONS = [
    ("Side view", (90, 0), (270, 0)),
    ("Top view", (0, 90), (0, 270)),
    ("Front view", (0, 0), (180, 0)),
]


@pytest.fixture
def preview(devbench):
    game = devbench.health()["game"]
    if game not in MESHES:
        pytest.skip(f"No mesh is listed for {game}")
    opened = devbench.call("preview_nif", {"action": "open", "path": MESHES[game]})
    try:
        wait_for(
            lambda: camera(devbench, opened["id"]),
            lambda state: state["framed"],
            "The preview did not frame the mesh",
        )
        yield opened["id"]
    finally:
        devbench.call("preview_nif", {"action": "close", "id": opened["id"]})


def camera(devbench, preview: int) -> dict:
    details = devbench.call("preview_nif", {"action": "inspect", "id": preview})
    return details["panes"][0]["camera"]


def angles(devbench, preview: int) -> tuple[int, int]:
    state = camera(devbench, preview)
    return round(state["yaw"]), round(state["pitch"])


@pytest.mark.mo2
def test_view_buttons_turn_the_camera(devbench, preview):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    for tooltip, first, second in BUTTONS:
        button = next(c for c in controls if c["tooltip"].startswith(tooltip))
        for expected in (first, second):
            devbench.call("dialogs", {"action": "click", "id": button["id"]})
            wait_for(
                lambda: angles(devbench, preview),
                lambda shown, expected=expected: shown == expected,
                f"{tooltip} did not turn the camera to {expected}",
            )
    assert camera(devbench, preview)["framed"]
