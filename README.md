# NIF Preview

NIF preview plugin for Mod Organizer 2.

See [CHANGELOG.md](CHANGELOG.md) for release history.

## MO2 Compatibility

MO2 2.5.2 and MO2 2.5.3beta11 use different `uibase` plugin ABIs. Build and
release separate DLLs for them.

This repository includes the required ABI header snapshots in `mo2-abi/`:

- `mo2-abi/2.5.2/include`
- `mo2-abi/2.5.3beta11/include`

Do not use current MO2 `master` headers for the 2.5.3beta11 release build.

## CLion

In CLion, select one of the versioned configure presets and reload CMake:

- `ninja-multi-mo2-2.5.2`
- `ninja-multi-mo2-2.5.3beta11`

Then build `preview_nif` with the `RelWithDebInfo` configuration.

To create release archives from CLion, build `preview_nif_zip` under each
versioned profile.

## Command Line

Configure both release build directories:

```powershell
cmake --preset ninja-multi-mo2-2.5.2
cmake --preset ninja-multi-mo2-2.5.3beta11
```

Build both DLLs:

```powershell
cmake --build --preset ninja-multi-mo2-2.5.2-relwithdebinfo
cmake --build --preset ninja-multi-mo2-2.5.3beta11-relwithdebinfo
```

Create both Nexus-style ZIP files:

```powershell
cmake --build --preset package-ninja-multi-mo2-2.5.2-relwithdebinfo
cmake --build --preset package-ninja-multi-mo2-2.5.3beta11-relwithdebinfo
```

Output:

- `dist/NIF.Preview.MO2-2.5.2.zip`
- `dist/NIF.Preview.MO2-2.5.3beta11.zip`

Each ZIP contains:

- `preview_nif.dll`
- `data/shaders`

## GitHub Release Builds

The GitHub Actions workflow builds both MO2 targets with `mob`, then configures
the plugin against the checked-in ABI header snapshot for each matrix entry.

On tag pushes, the workflow uploads these release assets:

- `NIF.Preview.MO2-2.5.2.zip`
- `NIF.Preview.MO2-2.5.3beta11.zip`

Those asset names must stay in sync with `plugindefinition.json`.

## Manual Deployment

Extract the matching package into the matching MO2 install's `plugins/`
directory:

- MO2 2.5.2: extract `dist/NIF.Preview.MO2-2.5.2.zip` into `<MO2 2.5.2 install>/plugins/`
- MO2 2.5.3beta11: extract `dist/NIF.Preview.MO2-2.5.3beta11.zip` into `<MO2 2.5.3beta11 install>/plugins/`

After extraction, the install should include:

- `<MO2 install>/plugins/preview_nif.dll`
- `<MO2 install>/plugins/data/shaders`

Do not mix the two DLLs. They may load, but the plugin interface ABI is different.
