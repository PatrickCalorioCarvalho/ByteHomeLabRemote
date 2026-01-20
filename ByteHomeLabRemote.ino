#include <SPI.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <FastLED.h>

/* ================= HARDWARE ================= */
#define TFT_CS     14
#define TFT_RST    21
#define TFT_DC     15
#define TFT_MOSI    6
#define TFT_SCLK    7
#define TFT_BL     22

#define JOY_UP     4
#define JOY_DOWN   5
#define JOY_LEFT   23
#define JOY_RIGHT  20
#define JOY_SET    19
#define JOY_RESET  18
#define JOY_MID     9

#define RGB_PIN 8
#define NUM_LEDS 1

#define HEADER_HEIGHT 20
#define ITEMS_PER_PAGE 5
#define ITEM_HEIGHT 26

Adafruit_ST7789 tft(TFT_CS, TFT_DC, TFT_RST);
CRGB leds[NUM_LEDS];

/* ================= CORES ================= */
#define LED_IDLE   CRGB(143,143,143)
#define LED_MOVE   CRGB(1,122,255)
#define LED_OK     CRGB(125,184,82)

/* ================= WIFI ================= */
const char* WIFI_SSID = "WIFI";
const char* WIFI_PASS = "SENHA";
const char* HA_URL    = "http://IP:8123/";

/* ================= UTIL ================= */
uint16_t rgb565(uint8_t r,uint8_t g,uint8_t b){
  return ((r & 0xF8) << 8)|((g & 0xFC) << 3)|(b >> 3);
}
uint16_t hexTo565(uint32_t hex){
  return rgb565(hex>>16,hex>>8,hex);
}

/* ================= LED ================= */
void ledIdle(){ leds[0]=LED_IDLE; FastLED.show(); }
void ledPulse(CRGB c){ leds[0]=c; FastLED.show(); delay(120); ledIdle(); }

/* ================= STATUS ================= */
enum HAStatus { NO_WIFI, WIFI_ONLY, HA_OK };
enum AppState { APP_LOADING, APP_ERROR, APP_READY };

HAStatus haStatus = NO_WIFI;
AppState appState = APP_LOADING;

/* ================= MENU ENGINE ================= */
struct Menu {
  const char* title;
  const char** items;
  int size;
};

enum MenuLevel {
  MENU_AREA,
  MENU_DEVICE,
  MENU_ACTION
};

struct MenuState {
  char title[32];
  const char** items;
  int size;
  int selected;
  int firstVisible;
  MenuLevel level;
};
#define MENU_STACK_SIZE 8
MenuState menuStack[MENU_STACK_SIZE];
int menuStackTop = -1;

char dynamicTitle[32];
MenuState stack[10];
int stackTop = -1;

Menu activeMenu;
MenuLevel currentLevel = MENU_AREA;
int selectedItem = 0;
int firstVisibleItem = 0;

/* ================= MOCK DATA ================= */
const char* areas[] = {"Sala","Quarto","Cozinha","Banheiro","Corredor","Externo","Quarto 2"};
const char* sala_devices[] = {"Luz","TV","Sensor"};
const char* quarto_devices[] = {"Luz","Ar"};
const char* actions_onoff[] = {"Ligar","Desligar"};

/* ================= STACK ================= */
void pushMenu() {
  if(menuStackTop >= MENU_STACK_SIZE - 1) return;

  menuStackTop++;

  strncpy(menuStack[menuStackTop].title,
          activeMenu.title,
          sizeof(menuStack[menuStackTop].title));

  menuStack[menuStackTop].items = activeMenu.items;
  menuStack[menuStackTop].size  = activeMenu.size;
  menuStack[menuStackTop].selected = selectedItem;
  menuStack[menuStackTop].firstVisible = firstVisibleItem;
  menuStack[menuStackTop].level = currentLevel;
}

bool popMenu() {
  if(menuStackTop < 0) return false;

  MenuState &m = menuStack[menuStackTop--];

  activeMenu.title = m.title;
  activeMenu.items = m.items;
  activeMenu.size  = m.size;
  selectedItem     = m.selected;
  firstVisibleItem = m.firstVisible;
  currentLevel     = m.level;

  return true;
}

void reconnectAll()
{
  connectWiFi();
  checkHA();
}
/* ================= WIFI ================= */
bool connectWiFi(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long t = millis();
  while(WiFi.status()!=WL_CONNECTED){
    if(millis()-t > 10000) return false;
    delay(200);
  }
  return true;
}

void checkHA(){
  if(WiFi.status()!=WL_CONNECTED){
    haStatus = NO_WIFI;
    appState = APP_ERROR;
    return;
  }
  HTTPClient http;
  http.begin(HA_URL);
  int code = http.GET();
  http.end();
  haStatus = code > 0 ? HA_OK : WIFI_ONLY;
  appState = (haStatus == HA_OK) ? APP_READY : APP_ERROR;
}

/* ================= UI ================= */
uint16_t statusColor(){
  if(haStatus==NO_WIFI) return hexTo565(0xFF3B30);
  if(haStatus==WIFI_ONLY) return hexTo565(0xFF9500);
  return hexTo565(0x34C759);
}

void drawHeader(const char* title){
  tft.fillRect(0,0,320,HEADER_HEIGHT,hexTo565(0x8f8f91));
  tft.setTextSize(2);
  tft.setTextColor(hexTo565(0x181617),hexTo565(0x8f8f91));
  int x=(320-(strlen(title)*12))/2;
  tft.setCursor(x,4);
  tft.print(title);
  tft.fillCircle(300,HEADER_HEIGHT/2+1,5,statusColor());
}

void drawFrame(){
  for(int i=0;i<4;i++)
    tft.drawRoundRect(i,i,320-i*2,172-i*2,18,hexTo565(0x8f8f91));
}

void drawErrorScreen(){
  tft.fillScreen(hexTo565(0xffffff));
  drawHeader("ByteHomeLab");

  tft.setTextSize(2);
  tft.setTextColor(hexTo565(0x181617));

  tft.setCursor(30, 60);
  tft.print("Falha de conexao");

  if(haStatus == NO_WIFI){
    tft.setCursor(30, 90);
    tft.print("Wi-Fi nao conectado");
  } else {
    tft.setCursor(30, 90);
    tft.print("Home Assistant");
    tft.setCursor(30, 110);
    tft.print("nao encontrado");
  }

  tft.setTextSize(1);
  tft.setCursor(30, 145);
  tft.print("SET + RESET");
  tft.setCursor(30, 158);
  tft.print("para tentar novamente");
}


void clearListArea(){
  tft.fillRect(12, HEADER_HEIGHT + 2, 296, 172 - HEADER_HEIGHT - 6, hexTo565(0xffffff));
}

void drawMenu(){
  drawHeader(activeMenu.title);
  clearListArea();
  tft.setTextSize(2);

  for(int i=0;i<ITEMS_PER_PAGE;i++){
    int idx = firstVisibleItem + i;
    if(idx >= activeMenu.size) break;

    int y = HEADER_HEIGHT + 8 + i * ITEM_HEIGHT;

    if(idx == selectedItem){
      tft.fillRoundRect(20,y-2,280,ITEM_HEIGHT,8,hexTo565(0x017aff));
      tft.setTextColor(hexTo565(0xffffff));
      tft.setCursor(26,y+5); tft.print(">");
    } else {
      tft.setTextColor(hexTo565(0x181617));
    }
    tft.setCursor(42,y+5);
    tft.print(activeMenu.items[idx]);
  }
}

/* ================= FLOW ================= */
void enterMenu(){
  ledPulse(LED_OK);

  // salva menu atual COMPLETO
  pushMenu();

  // novo título = item selecionado
  strncpy(dynamicTitle,
          activeMenu.items[selectedItem],
          sizeof(dynamicTitle));
  dynamicTitle[sizeof(dynamicTitle)-1] = '\0';

  selectedItem = 0;
  firstVisibleItem = 0;

  if(currentLevel == MENU_AREA){
    activeMenu = { dynamicTitle, sala_devices, 3 };
    currentLevel = MENU_DEVICE;
  }
  else if(currentLevel == MENU_DEVICE){
    activeMenu = { dynamicTitle, actions_onoff, 2 };
    currentLevel = MENU_ACTION;
  }

  drawMenu();
}


void backMenu(){
  ledPulse(LED_OK);
  if(popMenu()){
    drawMenu();
  }
}

/* ================= SETUP ================= */
void setup(){
  Serial.begin(115200);

  pinMode(JOY_UP,INPUT_PULLUP);
  pinMode(JOY_DOWN,INPUT_PULLUP);
  pinMode(JOY_LEFT,INPUT_PULLUP);
  pinMode(JOY_RIGHT,INPUT_PULLUP);
  pinMode(JOY_SET,INPUT_PULLUP);
  pinMode(JOY_RESET,INPUT_PULLUP);
  pinMode(JOY_MID,INPUT_PULLUP);

  FastLED.addLeds<WS2812,RGB_PIN,RGB>(leds,NUM_LEDS);
  ledIdle();

  SPI.begin(TFT_SCLK,-1,TFT_MOSI);
  pinMode(TFT_BL,OUTPUT);
  digitalWrite(TFT_BL,HIGH);

  tft.init(172,320);
  tft.setRotation(1);
  tft.fillScreen(hexTo565(0xffffff));
  drawFrame();

  connectWiFi();
  checkHA();

  activeMenu = {"ByteHomeLab", areas, 7};
  if(appState == APP_READY){
    drawMenu();
  } else {
    drawErrorScreen();
  }
}

/* ================= LOOP ================= */
unsigned long lastInput = 0;

void loop(){

  if(appState == APP_ERROR){

    if(!digitalRead(JOY_SET) && !digitalRead(JOY_RESET)){
      ledPulse(LED_OK);

      reconnectAll();

      if(appState == APP_READY){
        tft.fillScreen(hexTo565(0xffffff));
        drawFrame();
        drawMenu();
      } else {
        drawErrorScreen();
      }

      delay(500);
    }

    return;
  }

  if(millis()-lastInput < 180) return;

  if(!digitalRead(JOY_UP) && selectedItem > 0){
    selectedItem--;
    if(selectedItem < firstVisibleItem) firstVisibleItem--;
    ledPulse(LED_MOVE);
    drawMenu();
  }

  if(!digitalRead(JOY_DOWN) && selectedItem < activeMenu.size-1){
    selectedItem++;
    if(selectedItem >= firstVisibleItem + ITEMS_PER_PAGE) firstVisibleItem++;
    ledPulse(LED_MOVE);
    drawMenu();
  }

  if(!digitalRead(JOY_RIGHT) || !digitalRead(JOY_MID) || !digitalRead(JOY_RESET)){
    enterMenu();
  }

  if(!digitalRead(JOY_LEFT) || !digitalRead(JOY_SET)){
    backMenu();
  }

  lastInput = millis();
}
