# Changelog

## 0.5.0 - Unreleased

### Added

- Adds skinned mesh preview support.

### Fixed

- Fixes preview crashes and loading failures for valid luminance DDS parallax
  height maps, which now load for parallax height-map preview.

## 0.4.4 - 2026-05-09

### Added

- Adds MO2 2.5.3beta11 support while keeping a separate MO2 2.5.2 build.
- Adds checked-in `uibase` ABI header snapshots and versioned CMake presets for both supported MO2 versions.
- Adds package and GitHub Actions release targets that produce one ZIP per supported MO2 version.
- Adds release notes publishing from the tagged `CHANGELOG.md` section.

### Changed

- Packages both `preview_nif.dll` and `data/shaders`.
- Updates the plugin finder definition for the two 0.4.4 packages.
- Updates the plugin version to 0.4.4.0 beta.
- Uses vcpkg manifest dependencies for MO2 DDS headers and libbsarch.

### Fixed

- Fixes MO2 2.5.3beta11 crashes caused by plugin ABI mismatch.
- Handles NIF parse failures without crashing MO2.
- Skips invalid, null, hidden, or unsupported shapes during preview setup.
- Guards OpenGL setup, drawing, cleanup, projection, and buffer allocation against invalid state.
- Handles missing shader texture sets and unsupported texture slots.
- Handles loose, mod-provided, and BSA texture failures without crashing.
