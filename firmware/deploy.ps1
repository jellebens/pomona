# Pomona firmware deploy — bump version, build, flash (Trello #243).
#
#   .\deploy.ps1                        # bump patch, build+flash bringup to COM6
#   .\deploy.ps1 -Sketch levelprobe     # other sketch
#   .\deploy.ps1 -Bump minor            # 0.1.x -> 0.2.0 (features)
#   .\deploy.ps1 -Bump major            # x.y.z -> (x+1).0.0 (breaking)
#   .\deploy.ps1 -Bump none             # rebuild/reflash without bumping
#
# The version lives in libraries/PomonaVersion/src/PomonaVersion.h and is
# the single source of truth — every sketch prints it at boot. Deploy ONLY
# through this script so the number stays truthful. Commit the bumped file
# with the change it ships. USB (COM port) today; the OTA path (#243) will
# reuse the same script with a network target.
#
# Close the Arduino IDE's Serial Monitor first — it holds the COM port.

param(
    [string]$Sketch = "bringup",
    [string]$Port = "COM6",
    [ValidateSet("patch", "minor", "major", "none")]
    [string]$Bump = "patch"
)

$ErrorActionPreference = "Stop"

$fw = $PSScriptRoot
$cli = "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$fqbn = "arduino:mbed_giga:giga"
$versionFile = Join-Path $fw "libraries\PomonaVersion\src\PomonaVersion.h"

# --- bump ---------------------------------------------------------------
$content = Get-Content $versionFile -Raw
if ($content -notmatch '#define POMONA_FW_VERSION "(\d+)\.(\d+)\.(\d+)"') {
    throw "No semver POMONA_FW_VERSION found in $versionFile"
}
$major, $minor, $patch = [int]$Matches[1], [int]$Matches[2], [int]$Matches[3]
$old = "$major.$minor.$patch"

switch ($Bump) {
    "patch" { $patch++ }
    "minor" { $minor++; $patch = 0 }
    "major" { $major++; $minor = 0; $patch = 0 }
    "none"  { }
}
$new = "$major.$minor.$patch"

if ($Bump -ne "none") {
    $content -replace '#define POMONA_FW_VERSION "\d+\.\d+\.\d+"', "#define POMONA_FW_VERSION `"$new`"" |
        Set-Content -NoNewline $versionFile
    Write-Host "version: $old -> $new"
} else {
    Write-Host "version: $new (no bump)"
}

# --- build + flash ------------------------------------------------------
& $cli compile --fqbn $fqbn --libraries (Join-Path $fw "libraries") (Join-Path $fw $Sketch)
if ($LASTEXITCODE -ne 0) { throw "compile failed — version file was already bumped; fix and rerun with -Bump none" }

& $cli upload -p $Port --fqbn $fqbn (Join-Path $fw $Sketch)
if ($LASTEXITCODE -ne 0) { throw "upload failed (Serial Monitor open? board on $Port?) — rerun with -Bump none" }

Write-Host "deployed $Sketch v$new to $Port"
