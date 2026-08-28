// Pomona firmware v1 — display module implementation (Trello #229).
//
// LVGL 9 on Arduino_H7_Video (core-bundled; it also supplies lv_conf.h) +
// Arduino_GigaDisplayTouch (auto-registers the LVGL pointer indev). Only
// Montserrat 14 is enabled in the core's lv_conf, so value labels are
// scaled up via transform_scale instead of larger font binaries.

#include "display.h"
#include "../../config.h"

#include <Arduino_GigaDisplay.h> // GigaDisplayBacklight (blanking, #248)
#include <Arduino_H7_Video.h>
#include <Arduino_GigaDisplayTouch.h>
#include <lvgl.h>
#include <PomonaVersion.h>

static Arduino_H7_Video panel(800, 480, GigaDisplayShield);
static Arduino_GigaDisplayTouch touchDetector;
static GigaDisplayBacklight backlight;

// one tile per metric
struct Tile {
  lv_obj_t *value;
};

static lv_obj_t *wifiIcon; // header status strip: red down / green up
static lv_obj_t *mqttIcon;
static lv_obj_t *blankShield; // full-screen touch catcher while blanked
static bool blanked = false;
static Tile tWaterTemp, tEc, tPh, tLevel, tProbe;
static Tile tAirTemp, tRh, tPressure, tLux;
static Readings lastReadings; // reapplied after screen rebuilds
static int lastWifi = -1, lastMqtt = -1; // link icon change detection
static lv_obj_t *otaStageLabel = nullptr; // OTA progress view (see below)
static lv_obj_t *otaTimeLabel = nullptr;

static void titleClicked(lv_event_t *e); // tap "Pomona" -> boot/info screen

#define COL_BG lv_color_hex(0x101418)
#define COL_TILE lv_color_hex(0x1c242c)
#define COL_TEXT lv_color_hex(0xe8eef2)
#define COL_DIM lv_color_hex(0x8a9aa8)
#define COL_ACCENT lv_color_hex(0x4cc87a)
#define COL_OK lv_color_hex(0x4cc87a)  // connected / healthy
#define COL_WARN lv_color_hex(0xe0b13e) // getting low
#define COL_BAD lv_color_hex(0xe05252) // disconnected / refill

// ---- screen construction ---------------------------------------------

static Tile makeTile(lv_obj_t *parent, const char *caption) {
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_set_style_bg_color(card, COL_TILE, 0);
  lv_obj_set_style_border_width(card, 0, 0);
  lv_obj_set_style_radius(card, 10, 0);
  lv_obj_set_style_pad_all(card, 8, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(card, 1);
  lv_obj_set_height(card, lv_pct(100));

  lv_obj_t *cap = lv_label_create(card);
  lv_label_set_text(cap, caption);
  lv_obj_set_style_text_color(cap, COL_DIM, 0);
  lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 0, 0);

  Tile t;
  t.value = lv_label_create(card);
  lv_label_set_text(t.value, "--");
  lv_obj_set_style_text_color(t.value, COL_TEXT, 0);
  // 2x scale: only Montserrat 14 is compiled in (see file header)
  lv_obj_set_style_transform_scale(t.value, 512, 0);
  lv_obj_set_style_transform_pivot_x(t.value, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(t.value, lv_pct(50), 0);
  lv_obj_align(t.value, LV_ALIGN_CENTER, 0, 10);
  return t;
}

static lv_obj_t *makeRow(lv_obj_t *parent, int heightPx) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, lv_pct(100), heightPx);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 10, 0);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  return row;
}

static void buildScreen() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_pad_all(scr, 10, 0);
  lv_obj_set_style_pad_row(scr, 10, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);

  // header: title left, WiFi/MQTT status right
  lv_obj_t *header = makeRow(scr, 36);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t *title = lv_label_create(header);
  lv_label_set_text(title, "Pomona");
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  // tapping the title opens the boot/info screen (fresh I2C scan)
  lv_obj_add_flag(title, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(title, 20);
  lv_obj_add_event_cb(title, titleClicked, LV_EVENT_CLICKED, NULL);

  // status strip, top-right: WiFi + MQTT icons, red down / green up
  // (updated live by displayLinkStatus; version label stays bottom-right)
  lv_obj_t *strip = lv_obj_create(header);
  lv_obj_set_size(strip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_style_bg_opa(strip, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(strip, 0, 0);
  lv_obj_set_style_pad_all(strip, 0, 0);
  lv_obj_set_style_pad_column(strip, 16, 0);
  lv_obj_clear_flag(strip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(strip, LV_FLEX_FLOW_ROW);
  wifiIcon = lv_label_create(strip);
  lv_label_set_text(wifiIcon, LV_SYMBOL_WIFI " WiFi");
  lv_obj_set_style_text_color(wifiIcon, COL_BAD, 0);
  mqttIcon = lv_label_create(strip);
  lv_label_set_text(mqttIcon, LV_SYMBOL_ENVELOPE " MQTT");
  lv_obj_set_style_text_color(mqttIcon, COL_BAD, 0);

  // two tile rows: water on top, air + level below
  lv_obj_t *row1 = makeRow(scr, 190);
  tWaterTemp = makeTile(row1, "water temp  degC");
  tEc = makeTile(row1, "EC  mS/cm");
  tPh = makeTile(row1, "pH");
  tProbe = makeTile(row1, "water level");

  lv_obj_t *row2 = makeRow(scr, 190);
  tAirTemp = makeTile(row2, "air temp  degC");
  tRh = makeTile(row2, "humidity  %");
  tPressure = makeTile(row2, "pressure  hPa");
  tLux = makeTile(row2, "light  lux");
  tLevel = makeTile(row2, "level  %");

  // firmware version, bottom-right (dynamic from PomonaVersion.h)
  lv_obj_t *ver = lv_label_create(scr);
  lv_obj_add_flag(ver, LV_OBJ_FLAG_IGNORE_LAYOUT); // scr is a flex column
  lv_label_set_text(ver, "v" POMONA_FW_VERSION);
  lv_obj_set_style_text_color(ver, COL_DIM, 0);
  lv_obj_align(ver, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
}

// ---- screen blanking (#248) ------------------------------------------
// After DISPLAY_BLANK_TIMEOUT_MS without touch input the backlight goes
// off and an opaque full-screen shield on the top layer catches input.
// The touch that wakes the screen lands on the shield, never on the UI
// underneath. Everything else (sensors, MQTT, OTA) keeps running.

static void blankShieldEvent(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED) {
    backlight.set(100); // wake on touch-down
  } else if (code == LV_EVENT_RELEASED) {
    lv_obj_add_flag(blankShield, LV_OBJ_FLAG_HIDDEN); // gesture fully eaten
    blanked = false;
  }
}

static void makeBlankShield() {
  blankShield = lv_obj_create(lv_layer_top());
  lv_obj_set_size(blankShield, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_color(blankShield, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(blankShield, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(blankShield, 0, 0);
  lv_obj_set_style_radius(blankShield, 0, 0);
  lv_obj_clear_flag(blankShield, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(blankShield, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(blankShield, blankShieldEvent, LV_EVENT_ALL, NULL);
}

static void displayBlankingService() {
  if (!blanked &&
      lv_display_get_inactive_time(NULL) > DISPLAY_BLANK_TIMEOUT_MS) {
    blanked = true;
    lv_obj_remove_flag(blankShield, LV_OBJ_FLAG_HIDDEN);
    backlight.set(0);
  }
}

// ---- boot screen ------------------------------------------------------
// Shown by displayInit() the moment the panel is up, so a power cycle is
// visibly "booting" right away. displayBootStatus() updates the status
// line (I2C scan results etc.) and renders synchronously — setup() has no
// service loop yet. displayShowMain() swaps in the tiles UI when ready.

static lv_obj_t *bootStatusLabel = nullptr;

// The tiles screen leaves flex+padding styles on scr; any full-screen view
// built after it must neutralize them or its labels get flex-stacked and
// scaled titles clip off-screen (bench bugs 0.1.20 info screen, 0.1.23
// "ating firmware" OTA screen).
static lv_obj_t *freshScreen() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_clean(scr);
  lv_obj_set_layout(scr, LV_LAYOUT_NONE);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_obj_set_style_pad_row(scr, 0, 0);
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  return scr;
}

static void buildBootScreen() {
  lv_obj_t *scr = freshScreen();

  lv_obj_t *title = lv_label_create(scr);
  lv_label_set_text(title, "Pomona");
  lv_obj_set_style_text_color(title, COL_ACCENT, 0);
  lv_obj_set_style_transform_scale(title, 768, 0); // 3x (Montserrat 14 only)
  lv_obj_set_style_transform_pivot_x(title, lv_pct(50), 0);
  lv_obj_set_style_transform_pivot_y(title, lv_pct(50), 0);
  lv_obj_align(title, LV_ALIGN_CENTER, 0, -80);

  lv_obj_t *ver = lv_label_create(scr);
  lv_label_set_text(ver, "v" POMONA_FW_VERSION "  (built " __DATE__ ")");
  lv_obj_set_style_text_color(ver, COL_TEXT, 0);
  lv_obj_align(ver, LV_ALIGN_CENTER, 0, 0);

  bootStatusLabel = lv_label_create(scr);
  lv_label_set_text(bootStatusLabel, "booting...");
  lv_obj_set_style_text_color(bootStatusLabel, COL_DIM, 0);
  lv_obj_set_style_text_align(bootStatusLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(bootStatusLabel, LV_ALIGN_CENTER, 0, 50);
}

void displayBootStatus(const char *line) {
  if (!bootStatusLabel) return;
  lv_label_set_text(bootStatusLabel, line);
  lv_refr_now(NULL); // setup() isn't pumping displayService yet
}

void displayShowMain() {
  bootStatusLabel = nullptr;
  otaStageLabel = otaTimeLabel = nullptr;
  lv_obj_clean(lv_screen_active());
  buildScreen();
  if (!blankShield) makeBlankShield(); // created once, lives on the top layer
  lastWifi = lastMqtt = -1; // force the link icons to repaint
  lv_display_trigger_activity(NULL); // blanking idle clock starts now
  lv_refr_now(NULL);
}

void displayRestoreMain() {
  displayShowMain();
  displayUpdate(lastReadings); // tiles show the latest sweep right away
}

// ---- OTA progress view ------------------------------------------------
// The OTA apply blocks the main loop, so nothing pumps displayService while
// it runs: every update here renders synchronously via lv_refr_now.

void displayOtaScreen(const char *stage) {
  if (!otaStageLabel) {
    bootStatusLabel = nullptr;
    lv_obj_t *scr = freshScreen(); // neutralizes the tiles' flex layout

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "Updating firmware");
    lv_obj_set_style_text_color(title, COL_ACCENT, 0);
    lv_obj_set_style_transform_scale(title, 512, 0); // 2x (Montserrat 14)
    lv_obj_set_style_transform_pivot_x(title, lv_pct(50), 0);
    lv_obj_set_style_transform_pivot_y(title, lv_pct(50), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -90);

    otaStageLabel = lv_label_create(scr);
    lv_obj_set_style_text_color(otaStageLabel, COL_TEXT, 0);
    lv_obj_align(otaStageLabel, LV_ALIGN_CENTER, 0, -10);

    otaTimeLabel = lv_label_create(scr);
    lv_label_set_text(otaTimeLabel, "");
    lv_obj_set_style_text_color(otaTimeLabel, COL_DIM, 0);
    lv_obj_align(otaTimeLabel, LV_ALIGN_CENTER, 0, 30);

    lv_obj_t *warn = lv_label_create(scr);
    lv_label_set_text(warn, "do not power off");
    lv_obj_set_style_text_color(warn, COL_BAD, 0);
    lv_obj_align(warn, LV_ALIGN_CENTER, 0, 90);

    backlight.set(100); // make sure a blanked screen wakes for this
  }
  lv_label_set_text(otaStageLabel, stage);
  lv_refr_now(NULL);
}

void displayOtaTick(uint32_t elapsedS, int remainingEstS) {
  if (!otaTimeLabel) return;
  char buf[64];
  if (remainingEstS > 0)
    snprintf(buf, sizeof(buf), "%lus elapsed  ~%ds left",
             (unsigned long)elapsedS, remainingEstS);
  else
    snprintf(buf, sizeof(buf), "%lus elapsed  almost done...",
             (unsigned long)elapsedS);
  lv_label_set_text(otaTimeLabel, buf);
  lv_refr_now(NULL);
}

// ---- boot/info screen on demand (tap the "Pomona" header) -------------

static void infoDismissed(lv_event_t * /*e*/) {
  displayRestoreMain();
}

static void titleClicked(lv_event_t * /*e*/) {
  lv_obj_clean(lv_screen_active());
  buildBootScreen();

  char addrs[96];
  int found = sensorsI2CScan(addrs, sizeof(addrs));
  char line[128];
  snprintf(line, sizeof(line), "I2C: %s (%d found)\n\ntap to return",
           found ? addrs : "nothing", found);
  lv_label_set_text(bootStatusLabel, line);
  bootStatusLabel = nullptr; // not the boot path; no displayBootStatus use

  // full-screen transparent catcher: any tap returns to the tiles
  lv_obj_t *catcher = lv_obj_create(lv_screen_active());
  lv_obj_add_flag(catcher, LV_OBJ_FLAG_IGNORE_LAYOUT); // pin at 0,0 full-size
  lv_obj_set_pos(catcher, 0, 0);
  lv_obj_set_size(catcher, lv_pct(100), lv_pct(100));
  lv_obj_set_style_bg_opa(catcher, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(catcher, 0, 0);
  lv_obj_set_style_radius(catcher, 0, 0);
  lv_obj_clear_flag(catcher, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(catcher, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(catcher, infoDismissed, LV_EVENT_CLICKED, NULL);
}

// ---- value formatting ------------------------------------------------

static void setFloat(Tile &t, bool ok, float v, uint8_t decimals) {
  if (!ok || isnan(v)) {
    lv_label_set_text(t.value, "--");
    return;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", decimals, (double)v);
  lv_label_set_text(t.value, buf);
}

static void setInt(Tile &t, bool ok, int v) {
  if (!ok) {
    lv_label_set_text(t.value, "--");
    return;
  }
  char buf[12];
  snprintf(buf, sizeof(buf), "%d", v);
  lv_label_set_text(t.value, buf);
}

// ---- public API ------------------------------------------------------

void displayInit() {
  panel.begin(); // registers the LVGL display + tick
  touchDetector.begin(); // registers the LVGL pointer indev
  backlight.begin(100);
  buildBootScreen(); // tiles UI comes later via displayShowMain()
  lv_refr_now(NULL);
}

void displayService() {
  lv_timer_handler();
  displayBlankingService();
}

// Cheap enough to call every loop: styles are only written on change.
void displayLinkStatus(bool wifiUp, bool mqttUp) {
  if ((int)wifiUp != lastWifi) {
    lastWifi = wifiUp;
    lv_obj_set_style_text_color(wifiIcon, wifiUp ? COL_OK : COL_BAD, 0);
  }
  if ((int)mqttUp != lastMqtt) {
    lastMqtt = mqttUp;
    lv_obj_set_style_text_color(mqttIcon, mqttUp ? COL_OK : COL_BAD, 0);
  }
}

void displayUpdate(const Readings &r) {
  lastReadings = r; // cached: reapplied when the tiles screen is rebuilt
  setFloat(tWaterTemp, r.waterTempOk, r.waterTempC, 1);
  setFloat(tEc, true, r.ecMsCm, 2);
  // pH: uncalibrated -> show raw probe voltage so bench work has a number
  if (r.phOk) {
    setFloat(tPh, true, r.ph, 2);
  } else if (!isnan(r.phRawV)) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.3fV", (double)r.phRawV);
    lv_label_set_text(tPh.value, buf);
  } else {
    lv_label_set_text(tPh.value, "--");
  }
  // water level as a status word: probe points >=3 OK / 1-2 WRN / 0 CRIT
  // (pt 1 = 8.2 L, pt 3 = 9.7 L — docs/sensors/level-probe.md ladder)
  if (r.probePoints < 0) {
    lv_label_set_text(tProbe.value, "--");
    lv_obj_set_style_text_color(tProbe.value, COL_TEXT, 0);
  } else if (r.probePoints >= 3) {
    lv_label_set_text(tProbe.value, "OK");
    lv_obj_set_style_text_color(tProbe.value, COL_OK, 0);
  } else if (r.probePoints >= 1) {
    lv_label_set_text(tProbe.value, "WRN");
    lv_obj_set_style_text_color(tProbe.value, COL_WARN, 0);
  } else {
    lv_label_set_text(tProbe.value, "CRIT");
    lv_obj_set_style_text_color(tProbe.value, COL_BAD, 0);
  }
  setInt(tLevel, r.levelOk && r.levelPct >= 0, r.levelPct);
  setFloat(tAirTemp, r.bmeOk, r.airTempC, 1);
  setFloat(tRh, r.bmeOk, r.humidityPct, 1);
  setFloat(tPressure, r.bmeOk, r.pressureHpa, 1);
  setFloat(tLux, r.luxOk, r.lux, 0);
}
