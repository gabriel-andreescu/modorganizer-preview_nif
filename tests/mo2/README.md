# MO2 tests

Start MO2 with DevBench installed before running the suite. MO2 needs no ready
wait: DevBench answers once MO2 has loaded its plugins. Add plugin-specific
tests using MOPK's
[DevBench fixtures](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/devbench.md#pytest).

```powershell
uv run pytest tests/mo2 --mo2-tests
```
