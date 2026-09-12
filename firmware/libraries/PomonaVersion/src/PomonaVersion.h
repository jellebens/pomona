// Pomona firmware version — single source of truth (semver).
//
// Bumped AUTOMATICALLY by firmware/deploy.ps1 on every deploy (patch by
// default; -Bump minor/major for feature/breaking releases). Do not edit
// by hand except to correct a botched bump. Printed at boot by every
// sketch; later compared against the update server's version for OTA
// (#243, docs/ota-and-secrets.md).

#pragma once

// 2.0.0 (#295): the v2 wire — demeter/pomona-0001/… (ADR-0008); 2.1.0 (#297, ADR-0012):
// the root is ceres/pomona-0001/… — Demeter became Ceres before anything v2 was flashed. Set by
// hand for the breaking release; deploy it with `-Bump none`.
#define POMONA_FW_VERSION "2.1.0"
