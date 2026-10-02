"""Checks that previews pick the copy of each file that the game loads, using fixture mods installed into MO2."""

import shutil
import time
from pathlib import Path, PurePosixPath

import pytest
from mopk.testing import wait_for

FIXTURES = Path(__file__).parent / "fixtures" / "mods"
SEPARATOR = "My Mods - Published - Dev_separator"
GAME = "Skyrim Special Edition"

# Highest priority first, since each move places the mod right after the separator.
MODS = [
    "preview_nif - fixture loose high",
    "preview_nif - fixture inactive archives",
    "preview_nif - fixture archive early",
    "preview_nif - fixture archive late",
    "preview_nif - fixture loose low",
]

# The early archive's mod outranks the late one's, so only plugin load order can make the late archive win.
PLUGINS = {
    "previewnif_early.esp": True,
    "previewnif_late.esp": True,
    "previewnif_off.esp": False,
}

NIF = "meshes/previewnif/cases.nif"

# Every expected copy is green in the fixtures, and every other copy has another color.
EXPECTED_TEXTURES = {
    "loose_vs_archive": (
        "loose",
        "preview_nif - fixture loose high/textures/previewnif/loose_vs_archive.dds",
    ),
    "archive_order": ("archive", "previewnif_late - Textures.bsa"),
    "ignored_archives": ("archive", "previewnif_early.bsa"),
    "vanilla_override": ("archive", "previewnif_late.bsa"),
}

# The texture source dropdown lists every mod that has one of the NIF's textures, in MO2's mod list order, highest
# priority first. This is not the order the game picks copies in, which test_texture_winner checks. The inactive
# archives' mod is missing because the game loads none of its archives.
EXPECTED_TEXTURE_SOURCES = [
    ("auto", ""),
    ("mod", "preview_nif - fixture loose high"),
    ("mod", "preview_nif - fixture archive early"),
    ("mod", "preview_nif - fixture archive late"),
    ("mod", "preview_nif - fixture loose low"),
    ("gameData", ""),
]

# The early archive's mod supplies two of the textures. Each of the others comes from the copy the game loads, which a
# different mod supplies.
FALLBACK_SOURCE = "preview_nif - fixture archive early"
EXPECTED_FALLBACKS = {
    "textures/previewnif/archive_order.dds": (FALLBACK_SOURCE, False),
    "textures/previewnif/ignored_archives.dds": (FALLBACK_SOURCE, False),
    "textures/previewnif/loose_vs_archive.dds": (
        "preview_nif - fixture loose high",
        True,
    ),
    "textures/previewnif/flat_n.dds": ("preview_nif - fixture loose low", True),
    "textures/clutter/alchemyworkbench01.dds": (
        "preview_nif - fixture archive late",
        True,
    ),
}

EXPECTED_VERSIONS = [
    ("loose", "preview_nif - fixture loose high"),
    ("loose", "preview_nif - fixture loose low"),
    ("archive", "previewnif_late.bsa"),
    ("archive", "previewnif_early.bsa"),
]


def install_mod(devbench, name: str, installed: set[str]) -> bool:
    """Copies the fixture into the mod and enables it. Returns whether the mod was disabled before."""
    if name not in installed:
        devbench.call("mods", {"action": "create", "name": name})
    mod = devbench.call("mods", {"action": "get", "name": name})
    path = Path(mod["path"])
    for child in path.iterdir():
        if child.name == "meta.ini":
            continue
        if child.is_dir():
            shutil.rmtree(child)
        else:
            child.unlink()
    shutil.copytree(FIXTURES / name, path, dirs_exist_ok=True)
    devbench.call("mods", {"action": "move", "name": name, "separator": SEPARATOR})
    if mod["enabled"]:
        return False
    devbench.call("mods", {"action": "setEnabled", "name": name, "enabled": True})
    return True


def plugin_names(devbench) -> list[str]:
    return [
        plugin["name"]
        for plugin in devbench.call("plugins", {"action": "list"})["plugins"]
    ]


def set_plugin_states(devbench, states: dict[str, bool]) -> None:
    """Sets the fixture plugins' states and puts the early plugin right before the late one, last in the load order."""
    for plugin, enabled in states.items():
        devbench.call(
            "plugins", {"action": "setEnabled", "name": plugin, "enabled": enabled}
        )
    last = len(plugin_names(devbench)) - 1
    for plugin in ("previewnif_early.esp", "previewnif_late.esp"):
        devbench.call(
            "plugins", {"action": "setPriority", "name": plugin, "priority": last}
        )


def event_head(devbench) -> int:
    return devbench.call("events", {"action": "poll"})["headSeq"]


def wait_for_refresh(devbench, since: int) -> None:
    """Waits until MO2 reports a finished refresh after `since` and then stays quiet.

    MO2 refreshes asynchronously and publishes several refreshed events per refresh, so lists read before the events
    stop can still be the old ones.
    """
    refreshed = False
    deadline = time.monotonic() + 60
    while time.monotonic() < deadline:
        result = devbench.call(
            "events", {"action": "wait", "since": since, "timeoutMs": 2000}
        )
        events = result["events"]
        if not events:
            if refreshed:
                return
            continue
        refreshed = refreshed or any(
            event["topic"] == "plugins.refreshed" for event in events
        )
        since = result["headSeq"]
    raise AssertionError(f"MO2 did not finish refreshing after event {since}")


def archive_parsing(devbench) -> bool:
    devbench.call("settings", {"action": "get", "section": "workarounds"})
    status = wait_for(
        lambda: devbench.call("settings", {"action": "operationStatus"}),
        lambda s: s["state"] == "finished",
    )
    devbench.call("settings", {"action": "cancel"})
    wait_for(
        lambda: devbench.call("settings", {"action": "operationStatus"}),
        lambda s: s.get("action") == "cancel" and s["state"] == "finished",
    )
    controls = status["result"]["dialog"]["controls"]
    return next(c for c in controls if c["objectName"] == "enableArchiveParsingBox")[
        "checked"
    ]


def set_archive_parsing(devbench, enabled: bool) -> None:
    since = event_head(devbench)
    devbench.call(
        "settings",
        {
            "action": "set",
            "section": "workarounds",
            "values": {"enableArchiveParsingBox": enabled},
        },
    )
    wait_for(
        lambda: devbench.call("settings", {"action": "operationStatus"}),
        lambda s: s["state"] == "finished",
    )
    devbench.call("settings", {"action": "apply"})
    wait_for(
        lambda: devbench.call("settings", {"action": "operationStatus"}),
        lambda s: s["state"] == "finished",
    )
    refresh(devbench, since)


def refresh(devbench, since: int | None = None) -> None:
    since = event_head(devbench) if since is None else since
    devbench.call("settings", {"action": "refresh"})
    wait_for_refresh(devbench, since)


@pytest.fixture(
    scope="module",
    params=[False, True],
    ids=["archive-parsing-off", "archive-parsing-on"],
)
def fixture_mods(request, devbench_session):
    """Installs the fixture mods and enables their plugins, then disables the plugins and restores archive parsing.

    Left enabled, the fixture plugins would load their archives in the game and replace a vanilla clutter texture.
    The mods stay enabled, since their loose files use paths no game record refers to.
    """
    if devbench_session.health()["game"] != GAME:
        pytest.skip(f"Needs {GAME}")
    original_parsing = archive_parsing(devbench_session)
    installed = {
        mod["name"] for mod in devbench_session.call("mods", {"action": "list"})["mods"]
    }
    enabled_mods = [install_mod(devbench_session, name, installed) for name in MODS]
    if any(enabled_mods):
        refresh(devbench_session)
    set_archive_parsing(devbench_session, request.param)
    # MO2 lists a newly enabled mod's plugins only once its refresh ends.
    wait_for(
        lambda: plugin_names(devbench_session),
        lambda names: all(plugin in names for plugin in PLUGINS),
        "Fixture plugins did not appear",
        timeout=60,
    )
    set_plugin_states(devbench_session, PLUGINS)
    names = plugin_names(devbench_session)
    assert names.index("previewnif_early.esp") < names.index("previewnif_late.esp")
    # MO2 indexes archive members only for plugins that were active at its last refresh.
    refresh(devbench_session)
    yield request.param

    # Restoring archive parsing refreshes MO2, so the plugins are disabled after it.
    set_archive_parsing(devbench_session, original_parsing)
    set_plugin_states(devbench_session, dict.fromkeys(PLUGINS, False))


@pytest.fixture
def preview(devbench, fixture_mods):
    opened = devbench.call("preview_nif", {"action": "open", "path": NIF})
    try:
        yield opened["id"]
    finally:
        devbench.call("preview_nif", {"action": "close", "id": opened["id"]})


def inspect_pane(devbench, preview: int) -> dict:
    return devbench.call("preview_nif", {"action": "inspect", "id": preview})["panes"][
        0
    ]


@pytest.fixture
def cases(devbench, preview):
    return inspect_pane(devbench, preview)


def used_copy(texture: dict) -> dict:
    return texture["copies"][texture["usedCopy"]]


def select_texture_source(devbench, preview: int, pane: dict, mod: str) -> dict:
    """Picks the mod in the pane's texture source dropdown and returns the pane once it shows that mod's textures."""
    sources = pane["textureSources"]
    index = next(s["index"] for s in sources if s.get("sourceName") == mod)
    names = [source["displayName"] for source in sources]
    controls = devbench.call("dialogs", {"action": "describe"})["controls"]
    dropdown = next(c for c in controls if c.get("options") == names)
    devbench.call("dialogs", {"action": "select", "id": dropdown["id"], "index": index})
    # The pane replaces its view, and the new one loads its textures when it first paints.
    return wait_for(
        lambda: inspect_pane(devbench, preview),
        lambda p: p["currentTextureSource"] == index and p.get("textures"),
        f"The preview did not reload its textures from {mod}",
    )


@pytest.mark.mo2
@pytest.mark.parametrize("shape", EXPECTED_TEXTURES)
def test_texture_winner(cases, shape):
    kind, suffix = EXPECTED_TEXTURES[shape]
    base = next(
        texture
        for texture in cases["textures"]
        for use in texture["uses"]
        if use["shape"] == shape and use["slot"] == 0
    )
    assert base["status"] == "loaded"
    assert not base["chosenSourceLacksFile"]
    copy = used_copy(base)
    assert copy["kind"] == kind
    assert copy["path"].endswith(suffix), copy["path"]


@pytest.mark.mo2
def test_texture_source_falls_back_to_winning_copy(devbench, preview, cases):
    pane = select_texture_source(devbench, preview, cases, FALLBACK_SOURCE)
    actual = {
        texture["path"]: (used_copy(texture)["mod"], texture["chosenSourceLacksFile"])
        for texture in pane["textures"]
    }
    assert actual == EXPECTED_FALLBACKS


@pytest.mark.mo2
def test_nif_versions(cases):
    versions = [
        (
            provider["kind"],
            PurePosixPath(provider["archive"]).name
            if provider["kind"] == "archive"
            else provider["displayName"],
        )
        for provider in cases["providers"]
    ]
    assert versions[: len(EXPECTED_VERSIONS)] == EXPECTED_VERSIONS
    assert cases["currentProvider"] == 0


@pytest.mark.mo2
def test_texture_sources_follow_mod_priority(cases):
    sources = [
        (source["kind"], source.get("sourceName", ""))
        for source in cases["textureSources"]
    ]
    assert sources == EXPECTED_TEXTURE_SOURCES


@pytest.mark.mo2
def test_render(visual, preview, fixture_mods):
    parsing = "on" if fixture_mods else "off"
    visual.check(
        "preview_nif",
        f"fixture-sources-archive-parsing-{parsing}",
        recording="skyrim-se",
        arguments={"id": preview, "pane": "left", "width": 512, "height": 512},
    )
