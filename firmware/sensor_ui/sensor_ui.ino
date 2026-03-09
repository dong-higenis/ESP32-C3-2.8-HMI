
#include <Arduino.h>
#include "lib/TFT_eSPI/TFT_eSPI.h" // adafruit의 overhead로 인한 spi라이브러리 교체

#include "hw_def.h"

#include "ui_draw.h" 

/**
 * @note 업로드 환경
 * 
 *   esp32 by Espressif Systems - ver 2.0.10
 */

static void sensorTask(void);
static void parseFrame(char *s);

/**
 * @brief s/w 설정
 */ 
#define RX_BUF_SIZE 64 // 센서 데이터 버퍼

#define UI_DRAW_PERIOD 30 // LCD UI draw 주기
#define UART_TX_PERIOD 100 // uart 송신 주기

#define USE_TEST 1 // 테스트 케이스 사용 여부

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
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(LCD_BACKLIGHT,OUTPUT); // 백라이트 ON
  digitalWrite(LCD_BACKLIGHT,HIGH);
  
  Serial1.begin(9600, SERIAL_8N1, SENSOR_RX, SENSOR_TX); // 센서
  
  display.begin();
  display.setRotation(1); // 가로 모드

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

