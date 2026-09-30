# Render goldens

Reference renders for `tests/mo2/test_render.py`, one directory per game:
`<game>/default/<case>.png`. Each case opens a vanilla mesh, renders it
offscreen at 1024×1024 with the camera reset, and compares the result with its
golden using DevBench's SSIM scoring.

Renders repeat exactly on one machine, so `thresholds.json` sets 0.999. A
missing collision overlay on the barrel scores about 0.995.

Cases skip when MO2 manages another game, or when a mod overrides the mesh, its
textures or its material. Run the suite on an instance whose game data provides
them from the game's own archives.

## Updating

After an intended rendering change, rerun the affected cases with
`--visual-update` and review every changed image before committing:

```powershell
uv run pytest tests/mo2 --mo2-tests --visual-update -k barrel
```
