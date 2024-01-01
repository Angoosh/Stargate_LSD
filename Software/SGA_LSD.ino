#include <TFT_eSPI.h>            // Hardware-specific library
#include <SPI.h>
#include <Arduino.h>
#include <FS.h>
#include <ESP32AnalogRead.h>
#include "Wire.h"
#include "Adafruit_VL53L0X.h"
#include "Adafruit_SHT4x.h"
#include "SoundData.h"
#include "XT_DAC_Audio.h"

// TFT resolution
#define TFT_DISPLAY_RESOLUTION_X  320
#define TFT_DISPLAY_RESOLUTION_Y  480
#define CENTRE_Y                  TFT_DISPLAY_RESOLUTION_Y/2

// TFT SPI
#define TFT_LED          33      // TFT backlight pin
#define TFT_LED_PWM      100     // dutyCycle 0-255 last minimum was 15

// Buttons
#define BUTTON_UP        36
#define BUTTON_DOWN      16
//#define BUTTON_BACK      34
#define BUTTON_MENU      35
#define BUTTON_CENTER    39

//font
#define ALTERAN "stargate-alteran30"

#define TFT_GREY 0x7BEF
#define TFT_BG_COLOR 0xDA3E
#define TFT_POLY 0xC85D

//battery
#define BATTERY_ADC 34                      // Battery voltage mesurement
#define deviderRatio 1.7693877551  // Voltage devider ratio on ADC pin 1MOhm + 1.3MOhm

XT_Wav_Class Sound(kokoton_warning);
XT_Wav_Class Sound_beep(beep2);
XT_DAC_Audio_Class DacAudio(26,0);

ESP32AnalogRead adc;
TFT_eSPI tft = TFT_eSPI();       // Invoke custom library with default width and height
Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

bool ToF_present = true;
bool SHT4x_present = true;

bool menu_changed = false;

void clear_work_window(){
  tft.fillRect(50, 24, 230, 386, TFT_BLACK);
}

void draw_background_1(){
  uint8_t hexagon_size = 14; //length of horizontal line in px
  int hex_pos_y = 36;
  for(int c = 0; c < 16; c++){
    for(int r = 0; r < 6; r++){
      int offset = (r + 1.2) * hexagon_size * 3;
      tft.drawLine(offset, (hex_pos_y - 12), (hexagon_size / 2) + offset, hex_pos_y, TFT_BG_COLOR);
      tft.drawLine(offset, (hex_pos_y + 12), (hexagon_size / 2) + offset, hex_pos_y, TFT_BG_COLOR);
      if(r < 5){
        tft.drawLine(offset + (hexagon_size / 2), hex_pos_y, hexagon_size + offset + (hexagon_size / 2), hex_pos_y, TFT_BG_COLOR);
        tft.drawLine(hexagon_size + offset + (hexagon_size / 2), hex_pos_y, (hexagon_size * 2) + offset, (hex_pos_y - 12), TFT_BG_COLOR);
        tft.drawLine(hexagon_size + offset + (hexagon_size / 2), hex_pos_y, (hexagon_size * 2) + offset, (hex_pos_y + 12), TFT_BG_COLOR);
        tft.drawLine((hexagon_size * 2) + offset, (hex_pos_y - 12), (hexagon_size * 3) + offset, (hex_pos_y - 12), TFT_BG_COLOR);
      }
      else{
        tft.drawLine(offset + (hexagon_size / 2), hex_pos_y, hexagon_size + offset, hex_pos_y, TFT_BG_COLOR);
      }
    }
    hex_pos_y += 24;
  }
}

void draw_life_sign(int x, int y, int create, int fade_level){//5 fade levels
  if(create){
    uint16_t color = 0;
    uint16_t grey_level = 0;
    switch(fade_level){
      case 0:
        color = 0x6bf;
        grey_level = 0xffff;
        break;
      case 1:
        color = 0x579;
        grey_level = 0xce59;
        break;
      case 2:
        color = 0x433;
        grey_level = 0x9cf3;
        break;
      case 3:
        color = 0x2ed;
        grey_level = 0x6b6d;
        break;
      case 4:
        color = 0x187;
        grey_level = 0x39e7;
        break;
      default:
        color = 0x6bf;
        grey_level = 0xffff;
        break;
    }
    tft.fillCircle(x, y, 7, color);
    tft.drawCircle(x, y, 10, grey_level);
  }
  else{
    tft.fillCircle(x, y, 7, TFT_BLACK);
    tft.drawCircle(x, y, 10, TFT_BLACK);
  }
}

int Battery_Get_Percent(float voltage){
  float map[21] = {4.2,4.15,4.11,4.08,4.02,3.98,3.95,3.91,3.87,3.85,3.84,3.82,3.8,3.79,3.77,3.75,3.73,3.71,3.69,3.61,3.27};

  for(int i = 0; i < 21; i++){
    if(voltage > map[i]){
      if(i == 0){
        return 100;
      }
      else{
        float delta = map[i - 1] - map[i];
        float voltage_step = delta / 5.0;
        float voltages[6] = {map[i], map[i] + voltage_step, map[i] + voltage_step * 2.0, map[i] + voltage_step * 3.0, map[i] + voltage_step * 4.0, map[i - 1]};
        int smallest_index = 0;

        for(int j = 0; j < 6; j++){
          if(voltage > voltages[j]){
            smallest_index = j;
          }
          else{
            break;
          }
        }

        return 100 - i*5 + smallest_index;
      }
    }
    else;
  }
  return 0;
}

int last_bars = 0;
void battery_status(uint8_t state){
  int rect_size = 9;
  bool refresh = false;
  
  float voltage = adc.readVoltage() * deviderRatio;
  int percent = Battery_Get_Percent(voltage);
  int bars = (percent / 10) + 0.5;
  
  if(last_bars != bars){
    refresh = true;
    last_bars = bars;
  }

  if(refresh){
    int polygon_points[12][2] = {
      {TFT_DISPLAY_RESOLUTION_X - 40, TFT_DISPLAY_RESOLUTION_Y / 2},
      {TFT_DISPLAY_RESOLUTION_X - 40, TFT_DISPLAY_RESOLUTION_Y - 70},
      {TFT_DISPLAY_RESOLUTION_X - 80, TFT_DISPLAY_RESOLUTION_Y - 70},
      {TFT_DISPLAY_RESOLUTION_X - 95, TFT_DISPLAY_RESOLUTION_Y - 55},
      {TFT_DISPLAY_RESOLUTION_X - 140, TFT_DISPLAY_RESOLUTION_Y - 55},
      {TFT_DISPLAY_RESOLUTION_X - 155, TFT_DISPLAY_RESOLUTION_Y - 70},
      {TFT_DISPLAY_RESOLUTION_X - 180, TFT_DISPLAY_RESOLUTION_Y - 70},
      {TFT_DISPLAY_RESOLUTION_X - 180, TFT_DISPLAY_RESOLUTION_Y - 30},
      {TFT_DISPLAY_RESOLUTION_X - 165, TFT_DISPLAY_RESOLUTION_Y - 15},
      {TFT_DISPLAY_RESOLUTION_X - 60, TFT_DISPLAY_RESOLUTION_Y - 15},
      {TFT_DISPLAY_RESOLUTION_X - 10, TFT_DISPLAY_RESOLUTION_Y - 65},
      {TFT_DISPLAY_RESOLUTION_X - 10, TFT_DISPLAY_RESOLUTION_Y / 2 + 20}
    };
    if(state == 0){
      tft.fillTriangle(polygon_points[0][0], polygon_points[0][1], polygon_points[1][0], polygon_points[1][1], polygon_points[11][0], polygon_points[11][1], TFT_POLY);
      tft.fillTriangle(polygon_points[11][0], polygon_points[11][1], polygon_points[10][0], polygon_points[10][1], polygon_points[1][0], polygon_points[1][1], TFT_POLY);
      tft.fillTriangle(polygon_points[10][0], polygon_points[10][1], polygon_points[1][0], polygon_points[1][1], polygon_points[9][0], polygon_points[9][1], TFT_POLY);
      tft.fillTriangle(polygon_points[2][0], polygon_points[2][1], polygon_points[1][0], polygon_points[1][1], polygon_points[9][0], polygon_points[9][1], TFT_POLY);
      tft.fillTriangle(polygon_points[2][0], polygon_points[2][1], polygon_points[3][0], polygon_points[3][1], polygon_points[9][0], polygon_points[9][1], TFT_POLY);
      tft.fillTriangle(polygon_points[3][0], polygon_points[3][1], polygon_points[9][0], polygon_points[9][1], polygon_points[8][0], polygon_points[8][1], TFT_POLY);
      tft.fillTriangle(polygon_points[4][0], polygon_points[4][1], polygon_points[3][0], polygon_points[3][1], polygon_points[8][0], polygon_points[8][1], TFT_POLY);
      tft.fillTriangle(polygon_points[4][0], polygon_points[4][1], polygon_points[5][0], polygon_points[5][1], polygon_points[8][0], polygon_points[8][1], TFT_POLY);
      tft.fillTriangle(polygon_points[5][0], polygon_points[5][1], polygon_points[7][0], polygon_points[7][1], polygon_points[8][0], polygon_points[8][1], TFT_POLY);
      tft.fillTriangle(polygon_points[5][0], polygon_points[5][1], polygon_points[7][0], polygon_points[7][1], polygon_points[6][0], polygon_points[6][1], TFT_POLY);
    }
    int bars_empty = 10 - bars;

    tft.fillRect(TFT_DISPLAY_RESOLUTION_X - 35, ((TFT_DISPLAY_RESOLUTION_Y / 2 + 25)), 20, 140, TFT_POLY);
    for(int i = 0; i < 10; i++){
      if(bars_empty == 0){
        tft.fillRect(TFT_DISPLAY_RESOLUTION_X - 35, ((TFT_DISPLAY_RESOLUTION_Y / 2 + 25) + (i * rect_size * 1.5)), 20, rect_size, TFT_WHITE);
      }
      else{
        tft.drawRect(TFT_DISPLAY_RESOLUTION_X - 35, ((TFT_DISPLAY_RESOLUTION_Y / 2 + 25) + (i * rect_size * 1.5)), 20, rect_size, TFT_WHITE);
        bars_empty --;
      }
    }
    if(state == 0){
      for(int i = 0; i < 12; i++){
        if(i < 11){
          tft.drawLine(polygon_points[i][0], polygon_points[i][1], polygon_points[i+1][0], polygon_points[i+1][1], TFT_CYAN);
        }
        else{
          tft.drawLine(polygon_points[i][0], polygon_points[i][1], polygon_points[0][0], polygon_points[0][1], TFT_CYAN);
        }
      }
  
      tft.loadFont(ALTERAN);
      tft.setCursor(TFT_DISPLAY_RESOLUTION_X - 155, TFT_DISPLAY_RESOLUTION_Y - 48);
      tft.setTextColor(TFT_WHITE,TFT_BLACK);
      tft.print("BATTERY");
    }
  }
}

void draw_cosmetic_graphics(){
  int n_o_diamonds = 9;
  int points[n_o_diamonds][4][2] = {
    {{120,430},{90,430},{70,450},{100,450}},
    {{80,430},{50,430},{30,450},{60,450}},
    {{280,20},{255,20},{240,5},{265,5}},
    {{250,20},{225,20},{210,5},{235,5}},
    {{220,20},{195,20},{180,5},{205,5}},
    {{80,20},{55,20},{40,5},{65,5}},
    {{110,20},{85,20},{70,5},{95,5}},
    {{140,20},{115,20},{100,5},{125,5}},
    {{300,150},{280,135},{280,65},{300,50}},
  };

  int polygon[6][2] = {
    {45,30},{30,15},{30,200},{5,225},{5,325},{45,285}
  };

  tft.fillTriangle(polygon[0][0], polygon[0][1], polygon[1][0], polygon[1][1], polygon[2][0], polygon[2][1], TFT_POLY);
  tft.fillTriangle(polygon[0][0], polygon[0][1], polygon[5][0], polygon[5][1], polygon[2][0], polygon[2][1], TFT_POLY);
  tft.fillTriangle(polygon[2][0], polygon[2][1], polygon[5][0], polygon[5][1], polygon[4][0], polygon[4][1], TFT_POLY);
  tft.fillTriangle(polygon[3][0], polygon[3][1], polygon[2][0], polygon[2][1], polygon[4][0], polygon[4][1], TFT_POLY);
  for(int i = 0; i < 6; i++){
    if(i < 5){
      tft.drawLine(polygon[i][0], polygon[i][1], polygon[i+1][0], polygon[i+1][1], TFT_CYAN);
    }
    else{
      tft.drawLine(polygon[i][0], polygon[i][1], polygon[0][0], polygon[0][1], TFT_CYAN);
    }
  }

  for(int s = 0; s < n_o_diamonds; s++){
    tft.fillTriangle(points[s][0][0], points[s][0][1], points[s][1][0], points[s][1][1], points[s][2][0], points[s][2][1], TFT_POLY);
    tft.fillTriangle(points[s][0][0], points[s][0][1], points[s][2][0], points[s][2][1], points[s][3][0], points[s][3][1], TFT_POLY);
    for(int i = 0; i < 4; i++){
      if(i < 3){
        tft.drawLine(points[s][i][0], points[s][i][1], points[s][i+1][0], points[s][i+1][1], TFT_CYAN);
      }
      else{
        tft.drawLine(points[s][i][0], points[s][i][1], points[s][0][0], points[s][0][1], TFT_CYAN);
      }
    }
  }
}

int last_measurement = 0;
int ToF_read(){
  if(ToF_present){
    VL53L0X_RangingMeasurementData_t measure;
    lox.rangingTest(&measure, false);
    if (measure.RangeStatus != 4) {
      last_measurement = measure.RangeMilliMeter;
      return measure.RangeMilliMeter;
    }
    else{
      return -1;
    }
  }
  else{
    return -2;
  }
}

void SHT4x_read(float *retvals){
  if(SHT4x_present){
    sensors_event_t humidity, temp;
    sht4.getEvent(&humidity, &temp);
    retvals[0] = temp.temperature;
    retvals[1] = humidity.relative_humidity;
  }
  else{
    retvals[0] = 0;
    retvals[1] = 0;
  }
}

int env_data[2], env_data_old[2];
void teplotaVlhkost(){
  float tmp_env_data[2];
  bool refresh = false;
  SHT4x_read(tmp_env_data);
  env_data[0] = (int)tmp_env_data[0];
  env_data[1] = (int)tmp_env_data[1];
  if((env_data[0] != env_data_old[0]) || (env_data[1] != env_data_old[1])){
    refresh = true;
  }
  if(menu_changed){
    menu_changed = false;
    refresh = true;
    draw_background_1();
    tft.setCursor(50, 30);
    tft.print("teplota");
    tft.setCursor(50, 60);
    tft.print("vlhkost");
  }
  
  if(refresh){
    tft.setTextColor(TFT_BLACK,TFT_BLACK);
    tft.setCursor(150, 30);
    tft.print(env_data_old[0]);
    tft.setCursor(150, 60);
    tft.print(env_data_old[1]);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.setCursor(150, 30);
    tft.print(env_data[0]);
    tft.setCursor(150, 60);
    tft.print(env_data[1]);

    env_data_old[0] = env_data[0];
    env_data_old[1] = env_data[1];
  }
}

int kokoton_state = -1;
int koncentrace_old = 0;
int koncentrace_pure_old = 0;

void kokotonDetector(){
  int kokoton_distance = ToF_read();
  int x = TFT_DISPLAY_RESOLUTION_X / 2;
  int y = TFT_DISPLAY_RESOLUTION_Y / 2;
  bool refresh = false;

  if(menu_changed){
    menu_changed = false;
    refresh = true;
  }
  //Serial.println(kokoton_distance);

  if(kokoton_distance == -2){
    kokoton_state = -1;
    return;
  }
  if(kokoton_distance == -1){
    if(kokoton_state != 0){
      refresh = true;
    }
    kokoton_state = 0;
  }
  else if(kokoton_distance < 200){
    if(kokoton_state != 3){
      refresh = true;
    }
    kokoton_state = 3;
  }
  else if(kokoton_distance < 400){
    if(kokoton_state != 2){
      refresh = true;
    }
    kokoton_state = 2;
  }
  else if(kokoton_distance < 600){
    if(kokoton_state != 1){
      refresh = true;
    }
    kokoton_state = 1;
  }
  else{
    if(kokoton_state != 0){
      refresh = true;
    }
    kokoton_state = 0;
  }

  int koncentrace = (int)((8196.0-(float)kokoton_distance)/20.0);
  int koncentrace_pure = 8196-last_measurement;
  if(koncentrace != koncentrace_old){
    tft.setCursor(50, 90);
    tft.setTextColor(TFT_BLACK,TFT_BLACK);
    tft.print(koncentrace_pure_old);
    tft.setCursor(50, 90);
    tft.setTextColor(TFT_WHITE,TFT_BLACK);
    tft.print(koncentrace_pure);
    koncentrace_old = koncentrace;
    koncentrace_pure_old = koncentrace_pure;
  }

  if(refresh){
    clear_work_window();
    draw_background_1();
    tft.setCursor(50, 30);
    tft.print("kokotony");
    tft.setCursor(50, 60);
    tft.print("koncentrace ");
    int koncentrace = 8196-kokoton_distance;
    tft.setCursor(50, 90);
    tft.print(koncentrace);
    
    if(kokoton_state >= 1){
      tft.drawTriangle(x, y-21, x+25, y-64, x-25, y-64, TFT_WHITE);
      tft.fillTriangle(x, y-27, x+19, y-61, x-19, y-61, TFT_CYAN);
    }
    if(kokoton_state >= 2){
      tft.drawTriangle(x-19, y+10, x-69, y+10, x-44, y+54, TFT_WHITE);
      tft.fillTriangle(x-25, y+13, x-63, y+13, x-44, y+48, TFT_CYAN);
    }
    if(kokoton_state == 3){
      tft.drawTriangle(x+19, y+10, x+69, y+10, x+44, y+54, TFT_WHITE);
      tft.fillTriangle(x+25, y+13, x+63, y+13, x+44, y+48, TFT_CYAN);
    }
    refresh = false;
  }
  
  tft.fillCircle(x, y, 15, TFT_CYAN);
  tft.drawCircle(x, y, 18, TFT_WHITE);

  if(kokoton_state == 3){
    int i = 1;
    while(i != 0){
      DacAudio.FillBuffer();
      if(Sound.Playing == false){
        DacAudio.Play(&Sound);
        i --;
      }
    }
  }
}

int readButtons(){
  int buttonState = 0;
  buttonState = !digitalRead(BUTTON_UP);
  buttonState += (!digitalRead(BUTTON_DOWN) << 1);
//  buttonState += !(digitalRead(BUTTON_BACK) << 2);
  buttonState += (!digitalRead(BUTTON_MENU) << 3);
  buttonState += (!digitalRead(BUTTON_CENTER) << 4);

  return buttonState;
}

int menu_items = 3;
void menuGraphics(){
  int start_pixel[2] = {53, 33};
  int rect_size[2] = {150, 40};
  int rect_spacing = 4;
  int text_offset[2] = {3, 6};
  String menu_items_names[menu_items] = {"kokotony", "temphum", "lifesigns"};

  for(int i = 0; i < menu_items; i++){
    tft.fillRect(start_pixel[0], start_pixel[1]+(rect_spacing+2+rect_size[1])*i, rect_size[0], rect_size[1], TFT_GREY);
    tft.drawRect(start_pixel[0]-1, (start_pixel[1]-1)+(rect_spacing+rect_size[1]+2)*i, rect_size[0]+2, rect_size[1]+2, TFT_CYAN);
    tft.setCursor(start_pixel[0]+text_offset[0], start_pixel[1]+text_offset[1]+(rect_spacing+rect_size[1]+2)*i);
    tft.print(menu_items_names[i]);
  }
}

void menuSelectGraphics(int pos){
  int start_pixel[2] = {53, 33};
  int rect_size[2] = {150, 40};
  int rect_spacing = 4;

  for(int i = 0; i < menu_items; i++){
    if(i == pos){
      tft.drawRect(start_pixel[0]-2, (start_pixel[1]-2)+(rect_spacing+rect_size[1]+2)*i, rect_size[0]+4, rect_size[1]+4, TFT_WHITE);
      tft.drawRect(start_pixel[0]-3, (start_pixel[1]-3)+(rect_spacing+rect_size[1]+2)*i, rect_size[0]+6, rect_size[1]+6, TFT_WHITE);
    }
    else{
      tft.drawRect(start_pixel[0]-2, (start_pixel[1]-2)+(rect_spacing+rect_size[1]+2)*i, rect_size[0]+4, rect_size[1]+4, TFT_BLACK);
      tft.drawRect(start_pixel[0]-3, (start_pixel[1]-3)+(rect_spacing+rect_size[1]+2)*i, rect_size[0]+6, rect_size[1]+6, TFT_BLACK);
    }
  }
}

int menu_pos = 0;
int menu_pos_draw = 0;
bool menu_open = false;
void menuSelect(){//1 = up 2 = down
  if(!menu_open){
    clear_work_window();
    menuGraphics();
    menuSelectGraphics(menu_pos_draw);
    menu_pos = menu_pos_draw;
    menu_open = true;
  }
  int buttonNum = 0;
  int buttonNumOld = 0;
  while(menu_open){
    delay(100);
    buttonNum = readButtons();
    if(buttonNum != buttonNumOld){
      buttonNumOld = buttonNum;
      if(buttonNum == 8){//menu button closes menu
        menu_open = false;
        menu_changed = true;
        clear_work_window();
      }
      if(buttonNum == 1){
        menu_pos --;
        if(menu_pos < 0){
          menu_pos = menu_items-1;
        }
        menuSelectGraphics(menu_pos);
      }
      if(buttonNum == 2){
        menu_pos ++;
        if(menu_pos > menu_items-1){
          menu_pos = 0;
        }
        menuSelectGraphics(menu_pos);
      }
      if(buttonNum == 16){
        menu_pos_draw = menu_pos;
        menu_open = false;
        menu_changed = true;
        clear_work_window();
      }
    }
  }
}

uint32_t last_time_lifesign_ran = 0;
uint8_t lifesign_fadelevel = 4;
uint8_t lifesign_fade_dir = 0;

void lifeSigns(){
  bool refresh = false;
  
  if(menu_changed){
    menu_changed = false;
    refresh = true;
    draw_background_1();
  }
  if(millis() > (last_time_lifesign_ran + 200)){
    refresh = true;
    last_time_lifesign_ran = millis();
  }
  if(refresh){
    draw_life_sign(TFT_DISPLAY_RESOLUTION_X / 2, TFT_DISPLAY_RESOLUTION_Y / 2, 1, lifesign_fadelevel);
    if(lifesign_fade_dir == 0){
      lifesign_fadelevel --;
    }
    else{
      lifesign_fadelevel ++;
    }
    if(lifesign_fadelevel == 4){
      lifesign_fade_dir = 0;
    }
    if(lifesign_fadelevel == 0){
      lifesign_fade_dir = 1;
      int i = 1;
      while(i != 0){
        DacAudio.FillBuffer();
        if(Sound.Playing == false){
          DacAudio.Play(&Sound_beep);
          i --;
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  // put your setup code here, to run once:
  Wire.begin();
  adc.attach(BATTERY_ADC);
  
  ledcSetup(1, 5000, 8);              // ledChannel, freq, resolution
  ledcAttachPin(TFT_LED, 1);          // ledPin, ledChannel
  ledcWrite(1, TFT_LED_PWM);          // dutyCycle 0-255

  //Buttons
  pinMode(BUTTON_UP, INPUT);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
 // pinMode(BUTTON_BACK, INPUT);
  pinMode(BUTTON_MENU, INPUT);
  pinMode(BUTTON_CENTER, INPUT);

  if (!lox.begin()) {
    ToF_present = false;
  }
  if (! sht4.begin()) {
    SHT4x_present = false;
  }
  else{
    sht4.setPrecision(SHT4X_HIGH_PRECISION);
    sht4.setHeater(SHT4X_NO_HEATER);
  }

  // Setup the LCD
  tft.init();
  tft.setRotation(2);
  tft.fillScreen(TFT_BLACK);

  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS initialisation failed!");
    while (1) yield(); // Stay here twiddling thumbs waiting
  }
  Serial.println("\r\nSPIFFS available!");

  // ESP32 will crash if any of the fonts are missing
  bool font_missing = false;
  if (SPIFFS.exists("/stargate-alteran30.vlw")    == false) font_missing = true;

  if (font_missing)
  {
    Serial.println("\r\nFont missing in SPIFFS, did you upload it?");
    while(1) yield();
  }
  else Serial.println("\r\nFonts found OK.");

  draw_background_1();
  draw_cosmetic_graphics();

  battery_status(0);

  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.loadFont(ALTERAN); // Must load the font first
  
/*  for(int i = 0; i < 30; i++){
    draw_life_sign(120 + i*3, 200 + i*5, 1);
    tft.setCursor(20, 20); // Set cursor at top left of screen
    tft.println("WHOAREYOU"); // println moves cursor down for a new line
    delay(500);
    draw_life_sign(120 + i*3, 200 + i*5, 0);
    draw_background();
  }*/
//  tft.setCursor(50, 60);
//  tft.print("uptime");

  menuSelect();
}

int uptime = 0;
void loop() {
  // put your main code here, to run repeatedly:
  battery_status(1);
 
  if(readButtons() == 8){
    menuSelect();
  }
  switch(menu_pos_draw){
    case 0:
      kokotonDetector();
      break;
    case 1:
      teplotaVlhkost();
      break;
    case 2:
      lifeSigns();
      break;
    default:
      kokotonDetector();
      break;
  }

  delay(10);
}
