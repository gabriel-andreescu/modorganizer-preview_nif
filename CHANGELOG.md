# Changelog

## 0.5.0 - Unreleased

### Added

- Adds skinned mesh preview support.
- Adds split-view NIF comparison with synchronized camera support.
- Adds a dropdown for choosing which mod or archive the preview is shown from.
- Adds a per-pane texture source selector for comparing texture providers.
- Adds a Reset Camera button.
- Adds BSLighting refraction preview support for glass meshes with authored
  refractive surface textures.
- Adds Community Shaders True PBR material preview support, including RMAOS,
  displacement, emissive, and feature textures.
- Adds a Show Collision preview overlay for bhk collision shapes, BSBound, and
  BSMultiBound volumes.

### Fixed

- Fixes preview crashes and loading failures for valid luminance DDS parallax
  height maps, which now load for parallax height-map preview.
- Fixes previews for alpha-blended and effect-shader meshes whose transparent
  areas could incorrectly hide other geometry.
- Fixes refraction distortion meshes rendering as blue/purple normal-map sheets
  while preserving their heat-haze and glass distortion effect.
- Fixes effect-shader previews that ignored direct source and greyscale
  textures, causing embers, glow, fire, and similar effect surfaces to render
  black.

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
