# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/2.0.0/).

## [Unreleased]

### Added

- Adds a Show Editor Markers toggle for the Creation Kit markers the game hides.
- Adds a textures list showing each texture's slot and the mod or archive it
  comes from, with missing and unreadable textures flagged.
- Adds a Show Textures toggle.
- Adds a dropdown per texture for previewing any of its copies.
- Adds Hide Other Copies, which makes the shown copy of the mesh or a texture
  the winner by hiding the other loose copies, with Undo.
- Adds Unhide for hidden copies of the mesh and its textures.
- Adds Open in Explorer and Copy Path for textures.
- Marks the textures that differ between the two sides of Split Preview.

### Changed

- Support MO2 2.5.3beta12 instead of 2.5.3beta11
- Marks overwritten and hidden copies in the mesh and texture dropdowns.
- Renames the "Auto: MO2 winners" texture source to "Winning files".
- Wraps the toolbar in narrow windows.

### Removed

- Removes the arrow buttons beside the dropdowns.
- Removes the texture summary line. The textures list replaces it.

### Fixed

- Fixes small meshes opening with their front cut away, which made them look
  hollow until zoomed out.
- Fits the whole mesh in view when opening a preview or resetting the camera.
  Large and multi-part meshes were cropped or off-center.
- Cuts out the transparent parts of alpha-tested meshes, such as Fallout 4
  grass, using the Fallout 4 material's alpha settings.
- Fixes Skyrim grass showing only its blade tips. Models under
  `meshes/landscape/grass/` now render the way the game's grass shader draws
  them.
- Clamps textures where the mesh or its Fallout 4 material says to, instead of
  repeating them. Skyrim grass no longer shows stray patches above its blades.
- Fixes Fallout 4 FaceGen heads rendering turned, with the neck cap stretched
  into a spike.
- Fixes Fallout 4 bodies and other skin-tinted meshes rendering black.
- Fixes Fallout 4 meshes colored through a palette texture, such as weapons,
  armor and FaceGen hair, rendering white or grey.
- Fixes palette-colored effects, such as Skyrim forge flames, picking up colors
  from the wrong end of their palette.
- Fixes Fallout 4 FaceGen heads with gaps around the eyes and hairline, and with
  the eyelashes and eye shadow out of place.
- Draws transparent parts from back to front, so nearer layers no longer hide
  the ones behind them.
- Fades Fallout 4 blended layers, such as the wet film on the eyes, by their
  vertex alpha.
- Hides editor markers, such as the blue dome next to Fallout 4 plants, unless
  Show Editor Markers is on.
- Draws only the variant a switch node selects, instead of stacking every
  variant of trees and plants on top of each other.
- Hides shapes under hidden nodes.
- Fixes effect meshes, such as blowing snow and glass, rendering too
  transparent, and effects whose falloff angles run in reverse rendering
  unpredictably.
- Fixes Show Collision drawing nothing for Fallout 4 meshes.
- Fixes Skyrim collision parts that are moved or rotated within their shape,
  such as windmill frames, drawing piled up at the object's origin.
- Draws convex collision as its face outlines, without diagonals across each
  face.
- Shows NIFs without meshes, such as skeletons, by framing and drawing their
  collision, even with Show Collision off.
- Draws the per-bone collision of Fallout 4 skeletons.
- Draws cylinder and plane collision shapes, which Show Collision left out.
- Fixes packed triangle strip collision in Skyrim meshes drawing at a tenth of
  its size and ignoring its scale.
- Finds meshes and textures packed in mod archives without MO2's archive
  parsing.
- Picks between archives by plugin load order, as the game does, and ignores
  archives the game never loads, such as those of disabled plugins.
- Logs a mesh's warnings once instead of on every texture change.

## [0.5.1] - 2026-05-14

### Added

- Persists the split preview toggle per MO2 profile.

### Fixed

- Fixes NIF and texture source navigation so the dropdowns and arrow buttons
  follow MO2 conflict priority from winning mod to losing mod, with game
  archives last.

## [0.5.0] - 2026-05-11

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
- Fixes Fallout 4 effect-shader previews by loading their normal, environment,
  and environment mask texture slots.
- Fixes Fallout 4 material-backed previews by loading textures from BGSM/BGEM
  material files and resolving material texture paths that omit the `textures/`
  prefix.
- Fixes vanilla Fallout 4 archive texture lookup for previews whose referenced
  DDS files live in texture index BA2 archives.
- Fixes multilayer parallax texture source labels so inner-layer textures are
  identified correctly instead of being confused with height maps.

## [0.4.4] - 2026-05-09

### Added

- Adds MO2 2.5.3beta11 support while keeping a separate MO2 2.5.2 build.
- Adds checked-in `uibase` ABI header snapshots and versioned CMake presets for
  both supported MO2 versions.
- Adds package and GitHub Actions release targets that produce one ZIP per
  supported MO2 version.
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
- Guards OpenGL setup, drawing, cleanup, projection, and buffer allocation
  against invalid state.
- Handles missing shader texture sets and unsupported texture slots.
- Handles loose, mod-provided, and BSA texture failures without crashing.
