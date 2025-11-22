# AlmondShell v0.62.7 Release Notes

## Highlights

- 🛠️ **OpenGL renderer unblocked** – The renderer now references the shared
  `openglcontext::OpenGL4State` when drawing quads and debug outlines, fixing the
  missing `s_openglstate` symbol that previously broke builds.

## Fixes & Improvements

- **OpenGL backend** – Scoped all quad and debug-outline draws to the global
  renderer state so OpenGL builds can link successfully again.

## Documentation

- Updated the README snapshot, changelog, and engine analysis to record the
  OpenGL renderer fix and bumped version.

## Upgrade Notes

No manual action is required—pull the release and rebuild to pick up the
OpenGL renderer fix and refreshed documentation.
