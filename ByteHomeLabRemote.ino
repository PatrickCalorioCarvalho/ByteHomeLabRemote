#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <FastLED.h>

#define TFT_CS     14
#define TFT_RST    21
#define TFT_DC     15
#define TFT_MOSI    6
#define TFT_SCLK    7
#define TFT_BL     22
#define HEADER_HEIGHT 20

#define JOY_UP     4
#define JOY_DOWN   5
#define JOY_LEFT   23
#define JOY_RIGHT  20
#define JOY_SET    19
#define JOY_RESET  18
#define JOY_MID     9

#define LED_WHITE   CRGB(143, 143, 143)
#define LED_BLUE    CRGB(1, 122, 255)
#define LED_GREEN   CRGB(125, 184, 82)

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

#define RGB_PIN 8
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

const char* menuItems[] = {
  "Luzes",
  "Clima",
  "Tomadas",
  "Portao",
  "Sensores",
  "Cameras",
  "Config"
};

const int MENU_SIZE = sizeof(menuItems) / sizeof(menuItems[0]);

int selectedItem = 0;
int firstVisibleItem = 0;

const int ITEMS_PER_PAGE = 5;
const int ITEM_HEIGHT = 26;

unsigned long lastInput = 0;
const int inputDelay = 180;

void drawFrame() {

  for (int i = 0; i < 5; i++) {
    tft.drawRoundRect(
      0 + i,
      0 + i,
      320 - i * 2,
      172 - i * 2,
      18,
      hexTo565(0x8f8f91)
    );
  }

  drawHeader("ByteHomeLab");
}
void drawHeader(const char* title) {
  tft.fillRect(0, 0, 320, HEADER_HEIGHT, hexTo565(0x8f8f91));
  tft.drawLine(0, HEADER_HEIGHT, 312, HEADER_HEIGHT, hexTo565(0x8f8f91));

  tft.setTextSize(2);
  tft.setTextColor(hexTo565(0x181617), hexTo565(0x8f8f91));

  int textWidth = strlen(title) * 12;
  int x = (320 - textWidth) / 2;

  tft.setCursor(x, 4);
  tft.print(title);
}

void drawMenu() {
  tft.fillRect(12, 22, 296, 130, hexTo565(0xffffff));

  tft.setTextSize(2);

  const int textOffsetY = 5;
  const int arrowX = 26;
  const int textX  = 42;

  for (int i = 0; i < ITEMS_PER_PAGE; i++) {
    int index = firstVisibleItem + i;
    if (index >= MENU_SIZE) break;

    int y = 28 + i * ITEM_HEIGHT;
    tft.fillRect(20, y - 2, 280, ITEM_HEIGHT, hexTo565(0xffffff));
    if (index == selectedItem) {
      tft.fillRoundRect(20, y - 2, 280, ITEM_HEIGHT, 8, hexTo565(0x017aff));

      tft.setTextColor(hexTo565(0xe8ffff));
      tft.setCursor(textX, y + textOffsetY);
      tft.print(menuItems[index]);

      tft.setCursor(arrowX, y + textOffsetY);
      tft.print(">");

    } else {
      tft.setTextColor(hexTo565(0x181617));
      tft.setCursor(textX, y + textOffsetY);
      tft.print(menuItems[index]);
    }
  }
}

void executarAcao(int item) {
  tft.fillRect(12, 22, 296, 136, hexTo565(0xffffff));
  tft.setTextSize(2);
  tft.setTextColor(hexTo565(0x181617));
  tft.setCursor(30, 60);

  tft.print("Selecionado:");
  tft.setCursor(30, 90);
  tft.print(menuItems[item]);

  Serial.print("Selecionado: ");
  Serial.println(menuItems[item]);
}

void ledPulse(CRGB color, int duration = 120) {
  leds[0] = color;
  FastLED.show();
  delay(duration);
  ledIdle();
}

void ledIdle() {
  leds[0] = LED_WHITE;
  FastLED.show();
}

uint16_t hexTo565(uint32_t hex) {
  uint8_t r = (hex >> 16) & 0xFF;
  uint8_t g = (hex >> 8) & 0xFF;
  uint8_t b = hex & 0xFF;
  return rgb565(r, g, b);
}

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) |
         ((g & 0xFC) << 3) |
         (b >> 3);
}

void setup() {
  Serial.begin(115200);

  pinMode(JOY_UP,     INPUT_PULLUP);
  pinMode(JOY_DOWN,   INPUT_PULLUP);
  pinMode(JOY_LEFT,   INPUT_PULLUP);
  pinMode(JOY_RIGHT,  INPUT_PULLUP);
  pinMode(JOY_MID,    INPUT_PULLUP);
  pinMode(JOY_SET,    INPUT_PULLUP);
  pinMode(JOY_RESET,  INPUT_PULLUP);

  FastLED.addLeds<WS2812, RGB_PIN, RGB>(leds, NUM_LEDS);
  ledIdle();

  SPI.begin(TFT_SCLK, -1, TFT_MOSI);
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init(172, 320);
  tft.setRotation(1);
  tft.fillScreen(hexTo565(0xffffff));

  drawFrame();
  drawMenu();
}

void loop() {
  if (millis() - lastInput < inputDelay) return;

  if (!digitalRead(JOY_UP)) {
    if (selectedItem > 0) {
      selectedItem--;
      if (selectedItem < firstVisibleItem) {
        firstVisibleItem--;
      }
      ledPulse(LED_BLUE);
      drawMenu();
    }
    lastInput = millis();
  }

  if (!digitalRead(JOY_DOWN)) {
    if (selectedItem < MENU_SIZE - 1) {
      selectedItem++;
      if (selectedItem >= firstVisibleItem + ITEMS_PER_PAGE) {
        firstVisibleItem++;
      }
      ledPulse(LED_BLUE);
      drawMenu();
    }
    lastInput = millis();
  }

  if (!digitalRead(JOY_RIGHT) ||
      !digitalRead(JOY_MID)   ||
      !digitalRead(JOY_RESET)) {
    ledPulse(LED_GREEN);
    executarAcao(selectedItem);
    lastInput = millis();
  }

  if (!digitalRead(JOY_LEFT) ||
      !digitalRead(JOY_SET)) {
    ledPulse(LED_GREEN);
    selectedItem = 0;
    firstVisibleItem = 0;
    drawMenu();
    lastInput = millis();
  }
}

