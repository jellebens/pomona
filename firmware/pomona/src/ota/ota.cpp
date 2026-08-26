// Pomona firmware v1 — OTA module implementation (basic slice of #243).

#include "ota.h"

#include <Arduino_Portenta_OTA.h>
#include <mbed.h>

static bool capable = false;

static void kick() { mbed::Watchdog::get_instance().kick(); }

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
  Arduino_Portenta_OTA::Error e = ota.begin();
  if (e != Arduino_Portenta_OTA::Error::None) {
    snprintf(err, errLen, "begin failed (%d)", (int)e);
    return false;
  }

  kick();
  Serial.print("ota: downloading ");
  Serial.println(url);
  int const downloaded = ota.download(url, isHttps);
  if (downloaded <= 0) {
    snprintf(err, errLen, "download failed (%d)", downloaded);
    return false;
  }
  Serial.print("ota: stored ");
  Serial.print(downloaded);
  Serial.println(" bytes");

  kick();
  int const decompressed = ota.decompress();
  if (decompressed < 0) {
    snprintf(err, errLen, "decompress failed (%d)", decompressed);
    return false;
  }
  Serial.print("ota: decompressed ");
  Serial.print(decompressed);
  Serial.println(" bytes");

  kick();
  e = ota.update(); // stages bootloader parameters, applied on reset
  if (e != Arduino_Portenta_OTA::Error::None) {
    snprintf(err, errLen, "update failed (%d)", (int)e);
    return false;
  }

  Serial.println("ota: staged — resetting, bootloader applies the image");
  delay(500); // let serial/MQTT drain
  ota.reset();
  return true; // not reached
}
