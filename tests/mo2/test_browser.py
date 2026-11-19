"""Checks that the NIF Browser lists the game's own meshes and previews one from its archive."""

import pytest
from mopk.testing import wait_for

# A mesh that only the game's archives supply.
VANILLA_MESHES = {
    "Skyrim Special Edition": "meshes/clutter/barrel01.nif",
    "Fallout 4": "meshes/clothes/vault111suit/vault111suit.nif",
}


@pytest.fixture
def browser(devbench):
    """Opens the NIF Browser and waits for its mesh list."""
    devbench.call("preview_nif", {"action": "browse", "filter": ""})
    try:
        yield wait_for(
            lambda: devbench.call("preview_nif", {"action": "browse"}),
            lambda state: state["ready"],
            "The NIF Browser did not finish listing meshes",
            timeout=120,
        )
    finally:
        devbench.call("preview_nif", {"action": "closeBrowser"})


@pytest.mark.mo2
def test_browser_previews_a_vanilla_mesh(devbench, browser):
    game = devbench.health()["game"]
    if game not in VANILLA_MESHES:
        pytest.skip(f"No vanilla mesh is listed for {game}")
    mesh = VANILLA_MESHES[game]
    assert browser["meshes"] > 0

    # The filter keeps the mesh listed, so selecting it also checks the filtered list.
    shown = devbench.call(
        "preview_nif",
        {"action": "browse", "filter": mesh.rsplit("/", 1)[-1], "path": mesh},
    )
    # Archives spell their paths their own way, and data paths ignore case.
    assert shown["shown"].lower() == mesh

    pane = wait_for(
        lambda: devbench.call(
            "preview_nif", {"action": "inspect", "id": shown["preview"]}
        )["panes"][0],
        lambda p: p["rendered"],
        "The NIF Browser did not render the mesh",
    )
    assert pane["status"] == "loaded"
    assert pane["providers"][pane["currentProvider"]]["kind"] == "archive"
    assert pane["camera"]["framed"]
