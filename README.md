# preview_nif

Supports previewing NIF files

This project uses
[ModOrganizerPluginKit (MOPK)](https://github.com/gabriel-andreescu/ModOrganizerPluginKit)
for project generation and development tooling.

## Development

```powershell
xmake
xmake package
```

- [Build instructions](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/projects.md#build-a-generated-project)
- [Deployment and packaging](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/packaging.md)
- [Formatting setup](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/defaults.md#formatting)
- [Clang tooling](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/clang.md)
- [DevBench native integration](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/devbench.md#native-api)

## Tests

See
[DevBench setup](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/defaults.md#devbench)
and the
[Python client and fixtures](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/devbench.md).

## CI

See
[workflow setup](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/github-actions.md)
for build inputs and releases.
