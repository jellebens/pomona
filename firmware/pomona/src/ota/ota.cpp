// Pomona firmware v1 — OTA module implementation (basic slice of #243).

#include "ota.h"

#include <Arduino_Portenta_OTA.h>
#include <mbed.h>
#include <sys/stat.h>
#include <cstdio>

#include "../../config.h" // OTA_DOWNLOAD_ATTEMPTS, OTA_EST_TOTAL_S
#include "../display/display.h" // progress screen (stage + countdown)

static bool capable = false;
static uint32_t applyStartMs = 0; // nonzero while an apply runs (drives ticks)

// Watchdog feed, called by the library throughout download and decompress —
// also our only hook to keep the on-screen countdown moving while the apply
// blocks the main loop.
static void kick() {
  mbed::Watchdog::get_instance().kick();
  if (applyStartMs) {
    static uint32_t lastTickMs = 0;
    uint32_t now = millis();
    if (now - lastTickMs >= 1000) {
      lastTickMs = now;
      uint32_t elapsedS = (now - applyStartMs) / 1000;
      displayOtaTick(elapsedS, (int)(OTA_EST_TOTAL_S - elapsedS));
    }
  }
}

void otaInit() {
  Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);
  capable = ota.isOtaCapable();
  if (capable) {
    Serial.println("ota: capable (bootloader OK), listening on MQTT ota_url");
  } else {
    Serial.println("ota: NOT capable — bootloader too old or QSPI not "
                   "partitioned (one-time USB prereqs, see #243 / README)");
  }
}

bool otaApplyFromUrl(const char *url, char *err, size_t errLen) {
  if (!capable) {
    snprintf(err, errLen, "not OTA-capable (bootloader/QSPI prereqs)");
    return false;
  }
  bool isHttps = strncmp(url, "https:", 6) == 0;
  if (!isHttps && strncmp(url, "http:", 5) != 0) {
    snprintf(err, errLen, "url must be http(s)");
    return false;
  }

  Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);
  // Without these callbacks the library's internal feedWatchdog() is a no-op
  // and the ~1-2 min decompress starves the 30 s IWDG — the device resets
  // mid-decompress with nothing staged (bench: "applying" then reboot into
  // the OLD version). Register our kick for both the OTA stages and the
  // socket download path.
  ota.setFeedWatchdogFunc(kick);
  WiFi.setFeedWatchdogFunc(kick);
  Arduino_Portenta_OTA::Error e = ota.begin();
  if (e != Arduino_Portenta_OTA::Error::None) {
    snprintf(err, errLen, "begin failed (%d)", (int)e);
    return false;
  }

  applyStartMs = millis(); // progress screen + countdown from here on
  displayOtaScreen("preparing...");

  // The core's download() returns the HTTP Content-Length, NOT the bytes
  // actually written — fwrite errors in its body callback are silently
  // dropped, so the QSPI file can be short (seen on the bench: repeated
  // decompress error -5). Verify the on-disk size and retry.
  int downloaded = 0;
  bool sizeOk = false;
  for (int attempt = 1; attempt <= OTA_DOWNLOAD_ATTEMPTS; attempt++) {
    kick();
    char stage[48];
    snprintf(stage, sizeof(stage), "downloading new version (try %d/%d)...",
             attempt, OTA_DOWNLOAD_ATTEMPTS);
    displayOtaScreen(stage);
    Serial.print("ota: downloading (attempt ");
    Serial.print(attempt);
    Serial.print(") ");
    Serial.println(url);
    downloaded = ota.download(url, isHttps);
    if (downloaded <= 0) {
      Serial.print("ota: download failed (");
      Serial.print(downloaded);
      Serial.println(")");
      continue;
    }
    struct stat st;
    if (stat("/fs/UPDATE.BIN.LZSS", &st) != 0) {
      Serial.println("ota: stored file missing after download");
      continue;
    }
    Serial.print("ota: reported ");
    Serial.print(downloaded);
    Serial.print(" bytes, on flash ");
    Serial.println((long)st.st_size);
    if ((long)st.st_size == (long)downloaded) {
      sizeOk = true;
      break;
    }
    Serial.println("ota: short write — removing and retrying");
    remove("/fs/UPDATE.BIN.LZSS");
  }
  if (!sizeOk) {
    snprintf(err, errLen, "download failed/truncated after %d attempts (last %d)",
             OTA_DOWNLOAD_ATTEMPTS, downloaded);
    applyStartMs = 0;
    return false;
  }

  kick();
  displayOtaScreen("decompressing (the slow part, ~2 min)...");
  int const decompressed = ota.decompress();
  if (decompressed < 0) {
    snprintf(err, errLen, "decompress failed (%d)", decompressed);
    applyStartMs = 0;
    return false;
  }
  Serial.print("ota: decompressed ");
  Serial.print(decompressed);
  Serial.println(" bytes");

  kick();
  displayOtaScreen("installing...");
  e = ota.update(); // stages bootloader parameters, applied on reset
  if (e != Arduino_Portenta_OTA::Error::None) {
    snprintf(err, errLen, "update failed (%d)", (int)e);
    applyStartMs = 0;
    return false;
  }

  Serial.println("ota: staged — resetting, bootloader applies the image");
  displayOtaScreen("rebooting into the new version...");
  delay(500); // let serial/MQTT drain
  ota.reset();
  return true; // not reached
}
