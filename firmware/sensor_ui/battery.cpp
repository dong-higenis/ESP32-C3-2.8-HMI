#include "battery.h"

static float readVoltage(void);
static float voltageToPercent(float voltage);

/**
 * @brief 실제 측정값 기준
 */
static const float ocv_table[][OCV_MAX] =
{
  {4.20f, 100.0f}, 
  {4.05f,  96.0f},  
  {3.98f,  93.0f},  //  10분
  {3.93f,  89.0f},  //  15분
  {3.90f,  85.0f},  //  20분
  {3.87f,  81.0f},  //  25분
  {3.85f,  74.0f},  //  35분
  {3.83f,  67.0f},  //  45분
  {3.79f,  56.0f},  //  60분
  {3.76f,  44.0f},  //  75분
  {3.72f,  33.0f},  //  90분
  {3.69f,  26.0f},  // 100분
  {3.66f,  19.0f},  // 110분
  {3.61f,  13.0f},  // 117분
  {3.57f,   9.0f},  // 123분
  {3.52f,   6.0f},  // 127분
  {3.45f,   4.0f},  // 130분
  {3.35f,   2.0f},  // 132분
  {3.10f,   1.0f},  // 134분
  {2.44f,   0.0f},  // 135분
};


static const int ocv_table_size = sizeof(ocv_table) / sizeof(ocv_table[0]);

// EMA 필터가 적용되어질 배터리 잔량값
static float filtered_percent = -1.0f;
// 디스플레이에 출력할 배터리 잔량값
static int8_t display_percent = 0;
// 충전중 여부
static bool is_charging  = false;
static bool was_charging = false;
// 셧다운 필요 여부
static bool is_batt_need_shutdown = false;

void batteryInit(void)
{
  pinMode(BATT_CHARGE_DETECT, INPUT);
  analogSetPinAttenuation(BATT_CURRENT_VOLT, ADC_11db);

  // ADC 설정 직후 바로 읽으면 불안정한 값이 나올 수 있다.
  // 잠시 대기한 뒤 여러 샘플을 평균 내어 초기 베이스값을 안정화한다.
  delay(10);

  float sum = 0.0f;
  for (int i = 0; i < BAT_INIT_SAMPLES; i++)
  {
    sum += readVoltage();
  }

  float avg_volt    = sum / BAT_INIT_SAMPLES;
  float avg_percent = voltageToPercent(avg_volt);

  filtered_percent = avg_percent;
  display_percent  = (int8_t)(avg_percent + 0.5f);
}

void batteryUpdate(void)
{
  // 1). 충전 여부를 검사한다.
  is_charging = (digitalRead(BATT_CHARGE_DETECT) == LOW);

  // 2). 현재 전압값을 읽고
  float current_voltage = readVoltage();

  // ------------변할수있는 로직
  
  // 3). 충전중이라면 OFFSET값을 빼서 실제 배터리 전압값에 맞춰준다.
  if (is_charging)
    current_voltage -= BAT_CHARGE_VOLTAGE_OFFSET;


  // 4). 방전중 + 임계 전압 아래로 떨어질 경우 Shutdown 플래그를 올린다.
  if (!is_charging && current_voltage <= BAT_SHUTDOWN_VOLTAGE)
  {
    display_percent       = 0;
    is_batt_need_shutdown = true;
    return;
  }

  // 5). 현재 전압값을 퍼센트로 환산한다.
  float current_percent = voltageToPercent(current_voltage);
  
  
  // 6). EMA 필터를 걸어준다. (노이즈 방지)
  if (filtered_percent < 0)
    filtered_percent = current_percent;
  else
    filtered_percent += BAT_EMA_ALPHA * (current_percent - filtered_percent);

  // 7). EMA 필터가 적용된 값을 반올림하여 정수로 만들어 출력할 값을 계산한다.
  display_percent = (int8_t)(filtered_percent + 0.5f);
  
    Serial.printf("volt : %.3f | filtered : %.2f | display : %d\n", 
               current_voltage, filtered_percent, display_percent);

  // 8). 이전 상태를 기억한 뒤 마무리한다.
  was_charging = is_charging;
}




int8_t batteryGetPercent(void)
{
  return display_percent;
}

bool batteryIsCharging(void)
{
  return is_charging;
}

bool batteryIsDown(void)
{
  return is_batt_need_shutdown;
}

/**
 * @brief 분압된 ADC 값을 읽고 실제 배터리 전압으로 역산
 */
static float readVoltage(void)
{
  int   raw_mv   = analogReadMilliVolts(BATT_CURRENT_VOLT);
  float voltage  = (float)raw_mv / 1000.0f * BAT_DIVIDER_RATIO;
  voltage       += BAT_CALIBRATION_OFFSET;
  return voltage;
}

/**
 * @brief 룩업 테이블에서 전압 → 퍼센트 변환 (선형 보간)
 */
static float voltageToPercent(float voltage)
{
  // 1). 범위 초과를 체크 한다.

  // 측정 전압이 범위를 초과할 경우
  if (voltage >= ocv_table[0][VOLTAGE])
    // 100%를 반환
    return ocv_table[0][PERCENTAGE];

  // 측정 전압이 범위의 하한 미만일경우
  if (voltage <= ocv_table[ocv_table_size - 1][VOLTAGE])
    // 0%를 반환
    return ocv_table[ocv_table_size - 1][PERCENTAGE];

  // 2). 테이블에서 현재 전압이 속하는 구간을 찾는다.
  for (int i = 0; i < ocv_table_size - 1; i++)
  {
    // 전압 구간 상한
    float v_high = ocv_table[i][VOLTAGE];
    // 전압 구간 하한
    float v_low = ocv_table[i + 1][VOLTAGE];

    // 현재 전압이 이 구간안에 있는지 체크한다.
    if (voltage >= v_low)
    {
      // 퍼센트 구간 상한
      float p_high = ocv_table[i][PERCENTAGE];
      // 퍼센트 구간 하한
      float p_low = ocv_table[i + 1][PERCENTAGE];

      // 3). 선형 보간 로직을 적용한다.
      //
      // 구간 안에서 전압의 위치를 비율로 계산해서 퍼센트를 구한다.
      //
      // 예: v_high=4.10V, v_low=3.95V, voltage=4.00V
      //     p_high=90%,   p_low=75%
      //
      //     (voltage - v_low) / (v_high - v_low)
      //     = (4.00 - 3.95) / (4.10 - 3.95)
      //     = 0.05 / 0.15
      //     = 0.333  → 구간의 33.3% 지점에 있다
      //
      //     p_low + 0.333 * (p_high - p_low)
      //     = 75 + 0.333 * (90 - 75)
      //     = 75 + 5
      //     = 80%
      //
      return p_low + (voltage - v_low) / (v_high - v_low) * (p_high - p_low);
    }
  }
  return 0.0f; // 컴파일러 에러 방지
}