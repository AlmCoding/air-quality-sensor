#include "Display.h"

#include "Clock.h"

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

void Display::show(const SampleData &sample) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  char line[32] = {};
  DateTime dateTime = {};
  display.setCursor(0, 0);
  if (Clock::instance().isSet() && Clock::instance().getDateTime(dateTime)) {
    std::snprintf(
        line, sizeof(line), "%04ld-%02ld-%02ld %02ld:%02ld",
        static_cast<long>(dateTime.year), static_cast<long>(dateTime.month),
        static_cast<long>(dateTime.day), static_cast<long>(dateTime.hour),
        static_cast<long>(dateTime.minute));
  } else {
    std::snprintf(line, sizeof(line), "Date not set!");
  }
  display.print(line);

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
