// Pomona ATECC608A provisioning sketch (#244, Layer 2 of docs/ota-and-secrets.md).
//
// One-time-per-change USB tool. Writes WiFi SSID/PSK and the MQTT password into
// the GIGA's onboard ATECC608A so the real firmware image contains NO secrets
// (clean OTA images), and so credentials can be OVERWRITTEN later over USB (a
// WiFi change, an MQTT rotation) WITHOUT a rebuild — the credential slots stay
// clear-writable even after the config lock.
//
// Design + slot map + the irreversibility warnings: docs/atecc-provisioning.md.
//
// Credentials are typed in over Serial and are NEVER compiled into this binary.
//
// Menu (Serial @ 115200):
//   status                 chip serial, lock state, stored lengths
//   provision              (only if UNLOCKED) write the stock default config and,
//                          after you type LOCK, perform the IRREVERSIBLE lock
//   set ssid | psk | mqtt  write a credential slot (works before AND after lock)
//   verify                 read the clear slots back, print lengths only
//   help
//
// Build (from firmware/):
//   arduino-cli lib install ArduinoECCX08
//   arduino-cli compile --fqbn arduino:mbed_giga:giga --libraries libraries provision
//
// ⚠ The config lock is PERMANENT and the chip is soldered to the GIGA. Prove the
// whole flow on a SPARE GIGA first, then set the flag below and reflash to allow
// the lock on the real unit.
#define I_HAVE_VALIDATED_ON_A_SPARE 0

#include <ArduinoECCX08.h>

// Slot map (docs/atecc-provisioning.md). Slots 8-15 are open clear r/w data slots
// in the stock ECCX08 default TLS config; slot 0 is the reserved Layer-3 key slot.
static const int SLOT_PSK  = 9;   // WiFi PSK      (72 B)
static const int SLOT_MQTT = 10;  // MQTT password (72 B)
static const int SLOT_SSID = 11;  // WiFi SSID     (72 B)
static const int CRED_SLOT_SIZE = 72;
static const int MAX_VALUE = CRED_SLOT_SIZE - 1; // 1 byte reserved for length

// The library's public default TLS config, written UNMODIFIED (see the design
// note — no bespoke bytes, so no hand-authored-config brick risk). Bytes 0-15 are
// read-only and ignored by writeConfiguration().
static const byte DEFAULT_TLS_CONFIG[128] = {
  0x01, 0x23, 0x00, 0x00, 0x00, 0x00, 0x50, 0x00, 0x04, 0x05, 0x06, 0x07, 0xEE, 0x00, 0x01, 0x00,
  0xC0, 0x00, 0x00, 0x00, 0x83, 0x20, 0x87, 0x20, 0x87, 0x20, 0x87, 0x2F, 0x87, 0x2F, 0x8F, 0x8F,
  0x9F, 0x8F, 0xAF, 0x8F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0xAF, 0x8F, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x55, 0x55, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x33, 0x00, 0x1C, 0x00, 0x13, 0x00, 0x13, 0x00, 0x7C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x33, 0x00,
  0x1C, 0x00, 0x1C, 0x00, 0x3C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1C, 0x00, 0x1C, 0x00
};

static String readLine() {
  String s;
  while (true) {
    while (!Serial.available()) { /* wait */ }
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (s.length() == 0) continue; // swallow blank/CRLF pairs
      return s;
    }
    s += c;
  }
}

static void printStatus() {
  Serial.print("serial:  ");
  Serial.println(ECCX08.serialNumber());
  Serial.print("config zone: ");
  Serial.println(ECCX08.locked() ? "LOCKED" : "unlocked");
  for (int slot = 0; slot < 16; slot++) {
    if (slot != SLOT_SSID && slot != SLOT_PSK && slot != SLOT_MQTT) continue;
    byte buf[CRED_SLOT_SIZE];
    const char *name = slot == SLOT_SSID ? "ssid" : slot == SLOT_PSK ? "psk" : "mqtt";
    if (ECCX08.readSlot(slot, buf, CRED_SLOT_SIZE) == 1) {
      int len = buf[0];
      if (len < 0 || len > MAX_VALUE) len = 0; // unwritten slots read as junk
      Serial.print("slot "); Serial.print(slot); Serial.print(" ("); Serial.print(name);
      Serial.print("): "); Serial.print(len); Serial.println(len ? " bytes stored" : " bytes (empty)");
    } else {
      Serial.print("slot "); Serial.print(slot); Serial.println(": read failed");
    }
  }
}

static void doProvision() {
  if (ECCX08.locked()) {
    Serial.println("config zone is already LOCKED — nothing to provision. Use 'set' to (over)write credentials.");
    return;
  }
  Serial.println("Writing the stock default TLS config...");
  if (ECCX08.writeConfiguration(DEFAULT_TLS_CONFIG) != 1) {
    Serial.println("writeConfiguration FAILED — aborting, chip NOT locked.");
    return;
  }
  Serial.println("config written (not yet locked). Read-back:");
  printStatus();

  if (!I_HAVE_VALIDATED_ON_A_SPARE) {
    Serial.println();
    Serial.println("REFUSING TO LOCK: set I_HAVE_VALIDATED_ON_A_SPARE to 1 and reflash");
    Serial.println("only after proving the full flow on a spare GIGA. The lock is PERMANENT");
    Serial.println("and this chip is soldered to the board.");
    return;
  }
  Serial.println();
  Serial.println("*** IRREVERSIBLE ***  Type exactly  LOCK  to lock config+data, or anything else to abort.");
  String c = readLine();
  if (c != "LOCK") { Serial.println("aborted — chip NOT locked."); return; }
  if (ECCX08.lock() != 1) { Serial.println("lock FAILED."); return; }
  Serial.println("LOCKED. Now 'set ssid', 'set psk', 'set mqtt', then 'verify'.");
}

static void doSet(int slot, const char *name) {
  Serial.print("paste the "); Serial.print(name);
  Serial.print(" value on the next line (max "); Serial.print(MAX_VALUE); Serial.println(" chars):");
  String value = readLine();
  if ((int)value.length() > MAX_VALUE) {
    Serial.print("too long ("); Serial.print(value.length()); Serial.println(" chars) — not written.");
    return;
  }
  byte buf[CRED_SLOT_SIZE];
  memset(buf, 0, sizeof(buf));
  buf[0] = (byte)value.length();
  memcpy(buf + 1, value.c_str(), value.length());
  if (ECCX08.writeSlot(slot, buf, CRED_SLOT_SIZE) != 1) {
    Serial.println("writeSlot FAILED (is the slot writable / is the data zone locked correctly?).");
    return;
  }
  // Read back the length as a cheap integrity check (value itself is not echoed).
  byte rb[CRED_SLOT_SIZE];
  if (ECCX08.readSlot(slot, rb, CRED_SLOT_SIZE) == 1 && rb[0] == buf[0]) {
    Serial.print("ok — "); Serial.print(name); Serial.print(": "); Serial.print((int)buf[0]);
    Serial.println(" bytes written and verified.");
  } else {
    Serial.println("WARNING: read-back length mismatch — verify manually.");
  }
}

static void help() {
  Serial.println("commands: status | provision | set ssid|psk|mqtt | verify | help");
}

void setup() {
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 4000) {}
  Serial.println();
  Serial.println("=== Pomona ATECC608A provisioning (#244) ===");
  if (!ECCX08.begin()) {
    Serial.println("ECCX08.begin() FAILED — no ATECC608A on the I2C bus? Halting.");
    while (true) {}
  }
  printStatus();
  help();
}

void loop() {
  Serial.println();
  Serial.print("> ");
  String cmd = readLine();
  cmd.trim();
  if (cmd == "status") printStatus();
  else if (cmd == "provision") doProvision();
  else if (cmd == "set ssid") doSet(SLOT_SSID, "ssid");
  else if (cmd == "set psk") doSet(SLOT_PSK, "psk");
  else if (cmd == "set mqtt") doSet(SLOT_MQTT, "mqtt");
  else if (cmd == "verify") printStatus();
  else if (cmd == "help") help();
  else { Serial.print("unknown: "); Serial.println(cmd); help(); }
}
