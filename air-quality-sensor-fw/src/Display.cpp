#include "Display.h"

#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include <cstdio>

namespace {

constexpr int kScreenWidth = 128;
constexpr int kScreenHeight = 64;
constexpr int kResetPin = -1;
constexpr int kLineHeight = 14;

Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, kResetPin);

} // namespace

Display &Display::instance() {
  static Display displayInstance;
  return displayInstance;
}

bool Display::init() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, kI2cAddress)) {
    return false;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.display();
  return true;
}

void Display::drawTime(const TimeData &time) {
  char line[32] = {};
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.fillRect(0, 0, kScreenWidth, kLineHeight, SSD1306_BLACK);
  display.setCursor(0, 0);
  if (time.isSet) {
    std::snprintf(line, sizeof(line), "%04ld-%02ld-%02ld %02ld:%02ld",
                  static_cast<long>(time.year), static_cast<long>(time.month),
                  static_cast<long>(time.day), static_cast<long>(time.hour),
                  static_cast<long>(time.minute));
  } else {
    std::snprintf(line, sizeof(line), "Time not set!");
  }
  display.print(line);
  display.display();
}

void Display::drawSample(const SampleData &sample) {
  display.fillRect(0, kLineHeight, kScreenWidth, kScreenHeight - kLineHeight,
                   SSD1306_BLACK);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  display.setCursor(0, kLineHeight);
  display.printf("CO2: %u ppm", sample.co2Ppm);
  display.setCursor(0, kLineHeight * 2);
  display.printf("Temp: %.1f C", sample.tempC);
  display.setCursor(0, kLineHeight * 3);
  display.printf("Humidity: %.1f %%", sample.humidityPct);
  display.setCursor(0, kLineHeight * 4);
  display.printf("Window: %s", sample.windowOpen ? "OPEN" : "CLOSED");
  display.display();
}

void Display::drawSyncing() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("WiFi time syncing...");
  display.display();
}
