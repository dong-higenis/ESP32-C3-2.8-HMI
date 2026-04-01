
#include <Arduino.h>
#include "lib/TFT_eSPI/TFT_eSPI.h" // adafruit의 overhead로 인한 spi라이브러리 교체

#include "hw_def.h"

#include "battery.h"

#include "ui_draw.h" 

#include <esp_sleep.h>     // Deep Sleep 
#include <driver/gpio.h>   // 백라이트 hold용

/** 
 * @note 업로드 환경
 * 
 *   esp32 by Espressif Systems - ver 2.0.10
 */

static void sensorTask(void);
static void parseFrame(char *s);
static Button_t readButton(void);
static void enterDeepSleep(void);
static void lcdSleep(TFT_eSPI &disp);

/**
 * @brief s/w 설정
 */ 
#define RX_BUF_SIZE 64 // 센서 데이터 버퍼

#define UI_DRAW_PERIOD      30     // LCD UI draw 주기
#define BUTTON_READ_PERIOD  30     // 버튼 read 주기
#define BATT_UPDATE_PERIOD  100    // 배터리 상태 업데이트 주기
#define UART_TX_PERIOD      100    // uart 송신 주기

#define USE_TEST            1      // 테스트 케이스 사용 여부
#define USE_SLEEP           1      // sleep 모드 사용 여부
#define USE_DEBUG           1      // 테스트 printf 출력 여부

char sensor_rx_buffer[RX_BUF_SIZE]; // uart로 받는 값 저장
uint8_t sensor_rx_index = 0; // uart 버퍼 관리용 인덱스

#if USE_TEST
float sensor_x = 0.00f;
float sensor_y = 0.00f;
#else
float sensor_x = 0.0f;
float sensor_y = 0.0f;
#endif

TFT_eSPI display = TFT_eSPI();

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  // sleep -> wake-up시, 핀 hold 해제.
  gpio_hold_dis((gpio_num_t)LCD_BACKLIGHT);
  // hold_system
  gpio_deep_sleep_hold_dis();
  
  // 디버깅용으로 wake-up원인을 읽어옴 (사용 고려중)
  #if USE_TEST
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  #endif

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  pinMode(LCD_BACKLIGHT, OUTPUT);
  digitalWrite(LCD_BACKLIGHT, LOW);

  batteryInit();  // 배터리

  Serial1.begin(9600, SERIAL_8N1, SENSOR_RX, SENSOR_TX); // 센서
  
  display.begin();
  display.setRotation(1); // 가로 모드

  digitalWrite(LCD_BACKLIGHT, HIGH); // 백 라이트 ON
  uiInit(&display); // display 객체 ui에게 넘겨주기
}

void loop() 
{
  sensorTask();

  static float filtered_x = 0.0f;
  static float filtered_y = 0.0f;

  const float alpha = 0.8f; // 낮을수록 부드럽지만 반응 속도가 느리다.

#if USE_TEST
  // EMA Low-pass filter
  filtered_x += alpha * (sensor_x - filtered_x);
  filtered_y += alpha * (sensor_y - filtered_y);
#else       
  filtered_x = sensor_x;
  filtered_y = sensor_y;
#endif

  static uint32_t pre_draw;

  if (millis() - pre_draw > UI_DRAW_PERIOD)
  {
    pre_draw = millis();

    uiDraw(filtered_x, filtered_y);
  }

  // sleep 모드 타이머
  static uint32_t pre_sleep;

   // 버튼 타이머
  static uint32_t pre_button;
  
  if (millis() - pre_button > BUTTON_READ_PERIOD)
  {
    Button_t button = readButton();

    switch (button)
    {
      case BTN_NONE:
        break;
      
      default:
        Serial.printf("button [%d] pressed\n", button);

        // 버튼을 누르면 sleep모드로의 전환을 늦춘다.
        pre_sleep = millis();
        break;
    }
    
    pre_button = millis();
  }

  static uint32_t pre_bat;

  if (millis() - pre_bat > BATT_UPDATE_PERIOD)
  {
    pre_bat = millis();

    batteryUpdate();

    // 배터리 잔량이 없다면 강제 deep sleep모드로 진입
    if (batteryIsDown())
    {
    #if USE_SLEEP
      enterDeepSleep();
    #endif
    }
    int8_t charge = batteryGetPercent();
    uiSetBattPercent(charge, batteryIsCharging());
    
    #if USE_DEBUG
      Serial.printf("percent : %d\n", charge);
    #endif
  }

  if (millis() - pre_sleep > SLEEP_TIMEOUT_MS)
  {
    #if USE_SLEEP
      enterDeepSleep();
    #endif
  }
}

static void sensorTask(void)
{
  static uint32_t pre_time;

  // 주기적으로 정보 요청 신호를 보낸다.
  if (millis() - pre_time > UART_TX_PERIOD)
  {
    Serial1.write('$');
    Serial1.write('T');
    Serial1.write('\r');   // CR
    pre_time = millis();
  }

  // 수신을 처리한다.
  while (Serial1.available())
  {
    char c = Serial1.read();
    
    if (c == '\n') // 끝 문자열 도착
    {
      sensor_rx_buffer[sensor_rx_index] = 0;
      parseFrame(sensor_rx_buffer); // 한 줄 파싱
      sensor_rx_index = 0;
    }
    else if (sensor_rx_index < RX_BUF_SIZE - 1)
    {
      sensor_rx_buffer[sensor_rx_index++] = c;
    }
    else
    {
      sensor_rx_index = 0;
    }
  }
}

/**
 * @brief 실제 센서값을 받아서 파싱하여, 좌표 x y에 저장
 */
static void parseFrame(char *s)
{
  if (s[0] != '$') 
  {
    return;
  }

  char *p_x = strchr(s, 'X'); // X라는 문자열을 검색해서 해당 주소를 카피한다.
  char *p_y = strchr(s, 'Y'); // Y라는 문자열을 검색해서 해당 주소를 카피한다.

  if (!p_x || !p_y) // 둘중에 하나라도 없는경우 무효
  {
    return; 
  }

  sensor_x = atof(p_x + 1); // 프로토콜의 $X-120,456Y 까지 받는다. Y는 문자이므로 해당 함수는 -120,456 까지만 받는다 (-는 숫자로 봄)
  sensor_y = atof(p_y + 1); // Y이후부터 값을 float으로 파싱
}

/**
 * @brief 버튼 전압 값(mv) 읽기
 */
static Button_t readButton(void)
{
  uint32_t sensing_voltage = analogReadMilliVolts(BTN_PIN);  // 캘리브레이션된 mV 값
  // 분압 공식: mV = 3300 * R2 / (R1 + R2)
  float s1_mv = 0;
  float s2_mv = REFERENCE_MILLI_VOLTAGE * RESIST_S2_R2_VALUE / (RESISTOR_PULLUP_VALUE + RESIST_S2_R2_VALUE);
  float s3_mv = REFERENCE_MILLI_VOLTAGE * RESIST_S3_R2_VALUE / (RESISTOR_PULLUP_VALUE + RESIST_S3_R2_VALUE);
  float s4_mv = REFERENCE_MILLI_VOLTAGE * RESIST_S4_R2_VALUE / (RESISTOR_PULLUP_VALUE + RESIST_S4_R2_VALUE);
  float released_mv = REFERENCE_MILLI_VOLTAGE * RESIST_RELEASED_R2_VALUE / (RESISTOR_PULLUP_VALUE + RESIST_RELEASED_R2_VALUE);

  if (sensing_voltage < (uint32_t)(s1_mv + s2_mv) / 2)
    return BTN_S1;    
  else if (sensing_voltage < (uint32_t)(s2_mv + s3_mv) / 2)   
    return BTN_S2;
  else if (sensing_voltage < (uint32_t)(s3_mv + s4_mv) / 2)  
    return BTN_S3;
  else if (sensing_voltage < (uint32_t)(s4_mv + released_mv) / 2)
    return BTN_S4;

  return BTN_NONE;
}

/**
 * @brief DEEP SLEEP 모드 진입
 */
static void enterDeepSleep(void)
{
  #if USE_DEBUG
    Serial.println("[SLEEP] Entering Deep-sleep...");
    Serial.flush();
  #endif

  // 1). LCD 소프트웨어 종료
  lcdSleep(display);

  // 2). SPI 버스 종료
  display.getSPIinstance().end();

  // 3). 센서 UART 종료
  Serial1.end();

  // 4). 디버그 UART 종료
  Serial.end();

  // 5). 백라이트 OFF + hold
  gpio_set_direction((gpio_num_t)LCD_BACKLIGHT, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)LCD_BACKLIGHT, LOW);
  gpio_hold_en((gpio_num_t)LCD_BACKLIGHT);

  // 6). LED OFF + hold (Active LOW면 HIGH)
  gpio_set_direction((gpio_num_t)LED_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)LED_PIN, HIGH);
  gpio_hold_en((gpio_num_t)LED_PIN);

  // 7). SPI 핀 정리 + hold
  gpio_set_direction((gpio_num_t)SPI_MOSI_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)SPI_MOSI_PIN, LOW);
  gpio_hold_en((gpio_num_t)SPI_MOSI_PIN);

  gpio_set_direction((gpio_num_t)SPI_SCLK_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)SPI_SCLK_PIN, LOW);
  gpio_hold_en((gpio_num_t)SPI_SCLK_PIN);

  gpio_set_direction((gpio_num_t)LCD_CS_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)LCD_CS_PIN, HIGH);   // CS deselect
  gpio_hold_en((gpio_num_t)LCD_CS_PIN);

  gpio_set_direction((gpio_num_t)LCD_DC_PIN, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)LCD_DC_PIN, LOW);
  gpio_hold_en((gpio_num_t)LCD_DC_PIN);

  // 8). SENSOR TX idle HIGH + hold
  //     ※ gpio_reset_pin() 절대 호출하지 말 것 — 설정 초기화됨
  gpio_set_direction((gpio_num_t)SENSOR_TX, GPIO_MODE_OUTPUT);
  gpio_set_level((gpio_num_t)SENSOR_TX, HIGH);    // UART idle = HIGH
  gpio_hold_en((gpio_num_t)SENSOR_TX);

  // 9). SENSOR_RX — pull-up으로 floating 방지
  gpio_set_direction((gpio_num_t)SENSOR_RX, GPIO_MODE_INPUT);
  gpio_pullup_en((gpio_num_t)SENSOR_RX);
  gpio_pulldown_dis((gpio_num_t)SENSOR_RX);
  // ※ RX는 hold 불필요 (입력 핀)

  // 10). deep sleep hold 시스템 활성화 (모든 gpio_hold_en 이후에 1회만)
  gpio_deep_sleep_hold_en();

  // 11). wake-up 조건 등록
  gpio_set_direction((gpio_num_t)BTN_PIN, GPIO_MODE_INPUT);
  gpio_pullup_en((gpio_num_t)BTN_PIN);      // 내부 pull-up도 함께 활성화
  gpio_pulldown_dis((gpio_num_t)BTN_PIN);
  delay(50);                                 // 핀 안정화 대기 (10ms → 50ms)
  esp_deep_sleep_enable_gpio_wakeup(BIT(BTN_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);
  esp_deep_sleep_start();

  // 12). Deep Sleep 진입
  esp_deep_sleep_start();
}
/**
 * @brief LCD 리셋핀이 상시 HIGH인 관계로 직접 ILI9431을 OFF 시킨다.
 */
static void lcdSleep(TFT_eSPI &disp) 
{
  disp.startWrite();          // CS를 명시적으로 LOW로 잡기
  disp.writecommand(0x28);    // Display OFF
  disp.endWrite();
  delay(20);
  
  disp.startWrite();
  disp.writecommand(0x10);    // Sleep IN
  disp.endWrite();
  delay(120);                 // 반드시 120ms 대기
}