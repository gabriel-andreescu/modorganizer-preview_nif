"""Checks that the viewport's view buttons turn the camera to each axis view and, clicked again, to its opposite.

With the cameras of Split Preview synced, a view button puts both panes on its view.
"""

import pytest
from mopk.testing import wait_for

# Upright barrels, which open from the front.
MESHES = {
    "Skyrim Special Edition": "meshes/clutter/barrel01.nif",
    "Fallout 4": "meshes/setdressing/metalbarrel/metalbarrel01.nif",
}

# Fallout 4's archives hold two copies of this mesh, which Split Preview needs.
SPLIT_MESH = "meshes/actors/character/characterassets/femalebody.nif"

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


def pane_angles(devbench, preview: int) -> list[tuple[int, int]]:
    details = devbench.call("preview_nif", {"action": "inspect", "id": preview})
    return [
        (round(pane["camera"]["yaw"]), round(pane["camera"]["pitch"]))
        for pane in details["panes"]
        if pane.get("rendered")
    ]


def set_checked(devbench, label: str, checked: bool):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    box = next(c for c in controls if c.get("text") == label)
    devbench.call(
        "dialogs", {"action": "setChecked", "id": box["id"], "checked": checked}
    )


def click_first(devbench, tooltip: str):
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    button = next(c for c in controls if c["tooltip"].startswith(tooltip))
    devbench.call("dialogs", {"action": "click", "id": button["id"]})


@pytest.mark.mo2
def test_view_button_matches_synced_panes(devbench):
    """Panes that were turned apart both end on a view button's view while their cameras are synced."""
    if devbench.health()["game"] != "Fallout 4":
        pytest.skip("Needs a mesh with two copies, which Fallout 4's archives have")
    opened = devbench.call("preview_nif", {"action": "open", "path": SPLIT_MESH})
    try:
        if not opened["split"]:
            set_checked(devbench, "Split Preview", True)
        wait_for(
            lambda: pane_angles(devbench, opened["id"]),
            lambda shown: len(shown) == 2,
            "Split Preview did not draw both panes",
        )

        set_checked(devbench, "Sync Cameras", False)
        click_first(devbench, "Side view")
        wait_for(
            lambda: pane_angles(devbench, opened["id"]),
            lambda shown: shown[0] != shown[1],
            "The side view did not turn one pane alone",
        )

        set_checked(devbench, "Sync Cameras", True)
        click_first(devbench, "Top view")
        wait_for(
            lambda: pane_angles(devbench, opened["id"]),
            lambda shown: shown[0] == shown[1] and shown[0][1] == 90,
            "The top view did not put both panes on the same view",
        )
    finally:
        if not opened["split"]:
            set_checked(devbench, "Split Preview", False)
        devbench.call("preview_nif", {"action": "close", "id": opened["id"]})
