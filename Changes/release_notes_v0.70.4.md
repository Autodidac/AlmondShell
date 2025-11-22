# AlmondShell v0.70.4 Release Notes

## Summary
- Unified the internal OpenGL renderer around the backend-managed GL state so quad and debug-outline helpers reuse the shader/VAO pipeline seeded by `opengl_initialize()` on Windows and Linux.
- Bumped the engine metadata and documentation to v0.70.4 so diagnostics and packagers report the refreshed runtime snapshot.

## Upgrade Notes
- Rebuild or reconfigure existing worktrees to pick up the new version string and ensure downstream packages include the renderer fix.
- If you ship platform-specific manifests, update them to reference v0.70.4 so launchers advertise the synced metadata.
