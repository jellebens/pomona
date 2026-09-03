// Pomona firmware version — single source of truth (semver).
//
// Bumped AUTOMATICALLY by firmware/deploy.ps1 on every deploy (patch by
// default; -Bump minor/major for feature/breaking releases). Do not edit
// by hand except to correct a botched bump. Printed at boot by every
// sketch; later compared against the update server's version for OTA
// (#243, docs/ota-and-secrets.md).

#pragma once

#define POMONA_FW_VERSION "1.2.0"
