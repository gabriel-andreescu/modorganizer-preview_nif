# MO2 ABI Headers

This directory contains checked-in `uibase` header snapshots used to build
release DLLs against specific MO2 plugin ABIs.

- `2.5.2/include` comes from `modorganizer-uibase` branch `2_5_x` at tag
  `v2.5.2`.
- `2.5.3beta11/include` comes from the official `Mod.Organizer-2.5.3beta11-src`
  source archive.

These headers are intentionally part of the repository because the plugin ABI is
not stable across those MO2 versions. Do not replace the 2.5.3beta11 snapshot
with current `master` headers.
