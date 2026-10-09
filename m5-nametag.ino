#include <M5Unified.h>
#include "config.h"

static int view = 0;  // 0 = name, 1 = QR code

// Audio ring settings
static const int SEGMENTS = 48;
static const int SAMPLE_RATE = 16000;
static const int SAMPLES = 384;  // 8 samples per segment
static int16_t samples[SAMPLES];
static float level[SEGMENTS];    // smoothed level per segment, 0..1
static int drawnLen[SEGMENTS];   // bar length currently on screen
static float peak = 2000.0f;     // auto-gain reference
static bool micOk = false;

static int cx, cy, ringInner, ringOuter;

// The name / QR code is drawn once into this off-screen disc, then pushed to
// the screen rotated to whatever angle keeps it upright.
static M5Canvas content(&M5.Display);
static const uint16_t KEY_COLOR = 0x0120;  // key colour for "not part of the disc"
static int contentSize;

// Orientation. ANGLE_OFFSET is what makes "held normally" upright.
static const float ANGLE_OFFSET = 90.0f;
static float gx = 0, gy = 0;      // smoothed gravity in the screen plane
static float drawnAngle = 0;      // angle the content is currently shown at
static float targetAngle = 0;

uint16_t hueColor(float h) {
  // h in 0..1, full saturation/value
  float r = fabsf(h * 6 - 3) - 1, g = 2 - fabsf(h * 6 - 2), b = 2 - fabsf(h * 6 - 4);
  r = constrain(r, 0.f, 1.f); g = constrain(g, 0.f, 1.f); b = constrain(b, 0.f, 1.f);
  return M5.Display.color565(r * 255, g * 255, b * 255);
}

// Wrap an angle difference into -180..180.
float wrap180(float a) {
  while (a > 180) a -= 360;
  while (a < -180) a += 360;
  return a;
}

float uprightAngle(float x, float y) {
  return (atan2f(x, y) * 180.0f / PI + ANGLE_OFFSET) * NAMETAG_ROTATION_DIRECTION;
}

void drawNameContent() {
  int c = contentSize / 2;
  content.fillCircle(c, c, c - 1, TFT_BLACK);
  content.setTextColor(TFT_WHITE);
  content.setTextDatum(middle_center);
  content.setFont(&fonts::FreeSansBold24pt7b);
  content.setTextSize(1);
  // Largest text box (w x h) that fits inside the disc:
  // (w/2)^2 + (h/2)^2 <= r^2, keeping the font's aspect ratio.
  float w1 = content.textWidth(NAMETAG_NAME), h1 = content.fontHeight();
  float r = c - 4;
  float scale = (2 * r) / sqrtf(w1 * w1 + h1 * h1);
  // Font glyphs have padding above/below, so allow a little extra.
  content.setTextSize(scale * 1.08f);
  content.drawString(NAMETAG_NAME, c, c);
  content.setTextSize(1);
}

void drawQRContent() {
  int c = contentSize / 2;
  // White disc gives the QR code its light background and quiet zone.
  content.fillCircle(c, c, c - 1, TFT_WHITE);
  // Largest square whose corners stay inside the disc, with a small margin.
  int side = (c - 9) * 1.414f;
  // Version 1 is the smallest QR size; longer URLs automatically use a bigger one.
  content.qrcode(NAMETAG_QR_URL, c - side / 2, c - side / 2, side, 1);
}

void pushContent() {
  content.pushRotateZoom(cx, cy, drawnAngle, 1.0f, 1.0f, KEY_COLOR);
}

// Full redraw: used at startup and when switching views.
void render() {
  M5.Display.fillScreen(TFT_BLACK);
  content.fillScreen(KEY_COLOR);
  if (view == 0) drawNameContent();
  else drawQRContent();
  pushContent();
  for (int i = 0; i < SEGMENTS; i++) drawnLen[i] = 0;
}

void updateRing() {
  if (!micOk) return;
  if (!M5.Mic.record(samples, SAMPLES, SAMPLE_RATE)) return;
  // Wait for the recording, but never hang if the mic stalls.
  uint32_t start = millis();
  while (M5.Mic.isRecording()) {
    if (millis() - start > 100) return;
    delay(1);
  }

  // Loudness of each chunk of the buffer, one chunk per segment.
  const int per = SAMPLES / SEGMENTS;
  float maxRms = 0;
  float rms[SEGMENTS];
  for (int i = 0; i < SEGMENTS; i++) {
    float sum = 0;
    for (int j = 0; j < per; j++) {
      float s = samples[i * per + j];
      sum += s * s;
    }
    rms[i] = sqrtf(sum / per);
    if (rms[i] > maxRms) maxRms = rms[i];
  }
  // Auto-gain: follow loud sounds quickly, relax slowly, never below a noise floor.
  peak = max(max(maxRms, peak * 0.995f), 800.0f);

  auto& d = M5.Display;
  int maxLen = ringOuter - ringInner;
  float segAngle = 360.0f / SEGMENTS;
  // Batch all drawing into one screen transaction instead of one per bar.
  d.startWrite();
  for (int i = 0; i < SEGMENTS; i++) {
    float v = min(rms[i] / peak, 1.0f);
    level[i] = max(v, level[i] * 0.85f);  // fast attack, smooth decay
    int len = max(2, (int)(level[i] * maxLen));
    // Skip tiny changes; they aren't visible and cost screen bandwidth.
    if (len == drawnLen[i] || (abs(len - drawnLen[i]) < 4 && len != 2)) continue;
    float a0 = i * segAngle - 90, a1 = a0 + segAngle - 2;  // 2 degree gap
    if (len > drawnLen[i]) {
      d.fillArc(cx, cy, ringInner + drawnLen[i], ringInner + len, a0, a1, hueColor((float)i / SEGMENTS));
    } else {
      d.fillArc(cx, cy, ringInner + len, ringInner + drawnLen[i], a0, a1, TFT_BLACK);
    }
    drawnLen[i] = len;
  }
  d.endWrite();
}

// Smoothly rotate the content so it stays upright.
void updateOrientation() {
  if (!M5.Imu.isEnabled()) return;
  float ax, ay, az;
  if (!M5.Imu.getAccel(&ax, &ay, &az)) return;

  // Low-pass filter the gravity direction to remove jitter.
  gx = gx * 0.8f + ax * 0.2f;
  gy = gy * 0.8f + ay * 0.2f;

  // Lying flat (gravity mostly through the screen): keep the current angle.
  if (sqrtf(gx * gx + gy * gy) > 0.4f) targetAngle = uprightAngle(gx, gy);

  // Ease towards the target, and only redraw when the change is visible.
  float diff = wrap180(targetAngle - drawnAngle);
  if (fabsf(diff) < 1.0f) return;
  drawnAngle = wrap180(drawnAngle + diff * 0.35f);
  pushContent();
}

void setup() {
  Serial.begin(115200);
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setBrightness(200);

  cx = M5.Display.width() / 2;
  cy = M5.Display.height() / 2;
  ringOuter = min(cx, cy) - 1;
  ringInner = ringOuter * 0.82f;

  contentSize = (ringInner - 2) * 2;
  content.setPsram(true);
  content.setColorDepth(16);
  if (!content.createSprite(contentSize, contentSize)) {
    Serial.println("ERROR: could not allocate content buffer (is PSRAM enabled?)");
  }

  // Speaker and mic can share the same audio bus, so turn the speaker off first.
  M5.Speaker.end();
  micOk = M5.Mic.begin();

  // Start at the current orientation instead of rotating in from 0.
  float ax, ay, az;
  if (M5.Imu.isEnabled() && M5.Imu.getAccel(&ax, &ay, &az)) {
    gx = ax;
    gy = ay;
    if (sqrtf(gx * gx + gy * gy) > 0.4f) targetAngle = drawnAngle = uprightAngle(gx, gy);
  }

  Serial.printf("nametag ready: board=%d mic=%s imu=%s\n", (int)M5.getBoard(),
                micOk ? "ok" : "no", M5.Imu.isEnabled() ? "ok" : "no");
  render();
}

void powerOff() {
  auto& d = M5.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_WHITE);
  d.setTextDatum(middle_center);
  d.setFont(&fonts::FreeSansBold24pt7b);
  d.drawString("Bye", cx, cy);
  delay(800);
  d.fillScreen(TFT_BLACK);
  M5.Power.powerOff();
}

void loop() {
  M5.update();

  // Hold a finger on the screen (or the button) for 3 seconds to power off.
  static uint32_t pressStart = 0;
  bool pressed = (M5.Touch.isEnabled() && M5.Touch.getCount() > 0) || M5.BtnA.isPressed();
  if (!pressed) pressStart = 0;
  else if (pressStart == 0) pressStart = millis();
  else if (millis() - pressStart > 3000) powerOff();

  bool tapped = false;
  if (M5.Touch.isEnabled() && M5.Touch.getDetail().wasClicked()) tapped = true;
  if (M5.BtnA.wasClicked()) tapped = true;
  if (tapped) {
    view = 1 - view;
    render();
  }

  updateOrientation();
  updateRing();
}
