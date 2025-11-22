# AlmondShell v0.62.1 Release Notes

## Highlights
- Fixes a cosmetic regression in the updater where `curl` error output was glued to AlmondShell's diagnostics without a newline.
- Keeps the updater's failure handling defensive by trimming empty artifacts and surfacing the failure reason immediately.
- Synchronises the bundled version metadata and changelog so integrators can verify the expected runtime snapshot.

## Known Issues
- Automated renderer regression scenes remain under development; see `docs/renderer_regression_plan.md` for the intended coverage.
- Crash reporting hooks have not landed yet, so crashes must be reproduced locally with a debugger attached.

## Roadmap Alignment
This release ticks off the Phase 5 documentation task to "Draft release notes summarising new features, known issues, and roadmap alignment" from `Changes/roadmap.txt`.
