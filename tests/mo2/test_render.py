"""Offscreen renders of vanilla meshes compared with committed goldens."""

from dataclasses import dataclass
from pathlib import PurePath

import pytest


@dataclass(frozen=True)
class Game:
    name: str
    recording: str
    archive_prefixes: tuple[str, ...]


@dataclass(frozen=True)
class Case:
    checkpoint: str
    path: str
    collision: bool = False
    split: bool = False
    yaw: float = 0
    pitch: float = 0


SKYRIM = Game("Skyrim Special Edition", "skyrim-se", ("Skyrim - ",))
FALLOUT4 = Game("Fallout 4", "fallout4", ("Fallout4 - ", "DLC"))

CASES = [
    # Blades and the stained glass lie in the view axis at the default yaw.
    (SKYRIM, Case("iron-longsword", "meshes/weapons/iron/longsword.nif", yaw=90)),
    (
        SKYRIM,
        Case(
            "female-body",
            "meshes/actors/character/character assets/femalebody_1.nif",
        ),
    ),
    (SKYRIM, Case("barrel", "meshes/clutter/barrel01.nif")),
    (SKYRIM, Case("barrel-collision", "meshes/clutter/barrel01.nif", collision=True)),
    (SKYRIM, Case("glass-cuirass", "meshes/armor/glass/m/cuirass_1.nif")),
    (SKYRIM, Case("aspen", "meshes/landscape/trees/treeaspen01.nif")),
    (SKYRIM, Case("field-grass", "meshes/landscape/grass/fieldgrass01.nif")),
    (SKYRIM, Case("fire-volume", "meshes/effects/bfxfirevol01.nif")),
    (
        SKYRIM,
        Case("stained-glass", "meshes/clutter/dbhood/dbstainglass01.nif", yaw=90),
    ),
    (SKYRIM, Case("farmhouse", "meshes/architecture/farmhouse/farmhouse01.nif")),
    (
        SKYRIM,
        Case(
            "npc-head",
            "meshes/actors/character/facegendata/facegeom/skyrim.esm/000a2c8e.nif",
        ),
    ),
    (
        SKYRIM,
        Case(
            "whiterun-house",
            "meshes/architecture/whiterun/wrbuildings/wrhousewind01.nif",
        ),
    ),
    (
        SKYRIM,
        Case(
            "whiterun-wall", "meshes/architecture/whiterun/wrcitywalls/wrwallstr01.nif"
        ),
    ),
    # Flat meshes and the cliff's one-sided face need a view from above or from the front.
    (SKYRIM, Case("road", "meshes/landscape/roads/roadchunkm01.nif", pitch=45)),
    (SKYRIM, Case("iron-bow", "meshes/weapons/iron/ironbow.nif", yaw=90, pitch=90)),
    (
        SKYRIM,
        Case("iron-arrow-quiver", "meshes/weapons/iron/ironarrow.nif", yaw=90),
    ),
    (SKYRIM, Case("mountain", "meshes/landscape/mountains/mountaincliff01.nif")),
    (SKYRIM, Case("boulder", "meshes/landscape/rocks/rockl01.nif")),
    (
        SKYRIM,
        Case(
            "dirt-cliff",
            "meshes/landscape/dirtcliffs/dirtcliffs01.nif",
            yaw=180,
            pitch=20,
        ),
    ),
    (SKYRIM, Case("fern", "meshes/landscape/plants/swordferncluster01.nif")),
    (SKYRIM, Case("shrub", "meshes/landscape/plants/tundrashrub01.nif")),
    (
        SKYRIM,
        Case(
            "blacksmith-forge", "meshes/furniture/blacksmithforgemarker.nif", pitch=35
        ),
    ),
    (
        FALLOUT4,
        Case("10mm-long-barrel", "meshes/weapons/10mmpistol/10mmlongbarrel.nif"),
    ),
    (
        FALLOUT4,
        Case("female-body", "meshes/actors/character/characterassets/femalebody.nif"),
    ),
    # Nuka-World ships a second version of the body, shown in the right pane.
    (
        FALLOUT4,
        Case(
            "female-body-split",
            "meshes/actors/character/characterassets/femalebody.nif",
            split=True,
        ),
    ),
    (FALLOUT4, Case("combat-armor", "meshes/armor/combatarmor/f_torso_heavy.nif")),
    (
        FALLOUT4,
        Case(
            "power-armor-torso",
            "meshes/actors/powerarmor/characterassets/go_t60a_torso.nif",
        ),
    ),
    (FALLOUT4, Case("blasted-tree", "meshes/landscape/trees/treeblasted01.nif")),
    (FALLOUT4, Case("beach-grass", "meshes/landscape/grass/beachgrassobj01.nif")),
    (FALLOUT4, Case("fire", "meshes/effects/fxfiremed01.nif")),
    (
        FALLOUT4,
        Case(
            "flammable-barrel",
            "meshes/setdressing/barrelflammable/barrelflammable.nif",
        ),
    ),
    (
        FALLOUT4,
        Case("baby-bottle", "meshes/setdressing/babybottle/babybottleclean01milk.nif"),
    ),
    (
        FALLOUT4,
        Case(
            "npc-head",
            "meshes/actors/character/facegendata/facegeom/fallout4.esm/00002f1e.nif",
        ),
    ),
    (FALLOUT4, Case("cabin", "meshes/architecture/unique/cabins/cabin01nofence.nif")),
    (
        FALLOUT4,
        Case("road", "meshes/landscape/roads/alley/roadalley1way01.nif", pitch=45),
    ),
    (
        FALLOUT4,
        Case("laser-receiver", "meshes/weapons/laserweapons/laserreceiver.nif", yaw=90),
    ),
    (
        FALLOUT4,
        Case(
            "power-armor-helmet",
            "meshes/actors/powerarmor/characterassets/go_t60a_helmet.nif",
        ),
    ),
    (FALLOUT4, Case("car", "meshes/vehicles/automotive/car01.nif", yaw=45, pitch=20)),
    (FALLOUT4, Case("boulder", "meshes/landscape/rocks/rockboulder01.nif")),
    (FALLOUT4, Case("cliff", "meshes/landscape/rocks/rockcliff01.nif")),
    (FALLOUT4, Case("corn", "meshes/landscape/plants/cornstalk01.nif")),
    (
        FALLOUT4,
        Case("chainlink-fence", "meshes/setdressing/fencesgrates/fencechainlink01.nif"),
    ),
    (
        FALLOUT4,
        Case("diner-sign", "meshes/setdressing/diner/dinerentrancesign01.nif", yaw=90),
    ),
    (FALLOUT4, Case("nuka-quantum", "meshes/props/nukacolabottlequantum.nif")),
]


def foreign_sources(details: dict, archive_prefixes: tuple[str, ...]) -> list[str]:
    """Data paths the preview resolved from anything but the game's own archives."""
    sources = []
    for pane in details["panes"]:
        sources.extend(p.get("archive") or p.get("file") for p in pane["providers"])
        for shape in pane["shapes"]:
            origins = [slot["origin"] for slot in shape["textures"] if "origin" in slot]
            if "material" in shape:
                origins.append(shape["material"]["origin"])
            sources.extend(o.get("archive") or o.get("file") for o in origins)
    return sorted(
        {
            source
            for source in sources
            if source and not PurePath(source).name.startswith(archive_prefixes)
        }
    )


def check(devbench, label):
    dialog = devbench.call("dialogs", {"action": "describe"})
    checkbox = next(
        control for control in dialog["controls"] if control.get("text") == label
    )
    devbench.call(
        "dialogs", {"action": "setChecked", "id": checkbox["id"], "checked": True}
    )


@pytest.fixture(scope="session")
def managed_game(devbench_session):
    return devbench_session.health()["game"]


@pytest.mark.mo2
@pytest.mark.parametrize(
    ("game", "case"), CASES, ids=[f"{g.recording}-{c.checkpoint}" for g, c in CASES]
)
def test_render(devbench, visual, managed_game, game, case):
    if managed_game != game.name:
        pytest.skip(f"Needs {game.name}. MO2 manages {managed_game}")
    preview = devbench.call("preview_nif", {"action": "open", "path": case.path})
    try:
        if case.collision:
            check(devbench, "Show Collision")
        if case.split:
            check(devbench, "Split Preview")
        details = devbench.call(
            "preview_nif", {"action": "inspect", "id": preview["id"]}
        )
        foreign = foreign_sources(details, game.archive_prefixes)
        if foreign:
            pytest.skip(f"{case.path} uses data outside the game archives: {foreign}")
        visual.check(
            "preview_nif",
            case.checkpoint,
            recording=game.recording,
            arguments={
                "id": preview["id"],
                "pane": "right" if case.split else "left",
                "width": 1024,
                "height": 1024,
                "yaw": case.yaw,
                "pitch": case.pitch,
            },
        )
    finally:
        devbench.call("preview_nif", {"action": "close", "id": preview["id"]})
