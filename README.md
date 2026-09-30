# preview_nif

This project uses
[ModOrganizerPluginKit (MOPK)](https://github.com/gabriel-andreescu/ModOrganizerPluginKit)
for project generation and development tooling.

## Development

See MOPK's
[project build instructions](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/projects.md#build-a-generated-project)
and
[deployment and packaging](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/packaging.md).

See
[formatting setup](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/defaults.md#formatting).

See
[Clang tooling](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/clang.md)
for formatting and linting.

See
[DevBench native integration](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/devbench.md#native-api).

```sh
xmake
xmake package
```

## Tests

See
[DevBench setup](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/template/defaults.md#devbench)
and the
[Python client and fixtures](https://github.com/gabriel-andreescu/ModOrganizerPluginKit/blob/main/docs/plugin-authors/tooling/devbench.md).
