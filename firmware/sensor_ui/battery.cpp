#include "battery.h"

/**
 * @brief 충전 상태
 * 
 */
typedef enum
{
  CHARGE_STATE_DISCONNECTED = 0,
  CHARGE_STATE_CHARGING,
  CHARGE_STATE_DONE,
} ChargeState_t;

/**
 * @brief 배터리 상태
 */
typedef struct
{
  // 실제 관리 기준으로 사용할 배터리 잔량 추정값
  float soc_percent;
  // EMA로 평활화된 전압값
  float filtered_voltage;
  // 화면 표시용 배터리 잔량값
  float display_filtered_percent;
  // 충전 상태
  ChargeState_t charge_state;
  // 배터리 저전압 셧다운 플래그
  bool is_need_shutdown;
} BatteryState_t;

static float readVoltage(void);
static float voltageToPercent(float voltage);
static bool readActiveLowPin(uint8_t pin);
static bool isChargePinsOk(bool *charge_detected, bool *charge_done);
static ChargeState_t initChargeState(void);
static ChargeState_t updateChargeState(ChargeState_t current_charge_state);
static float getNextSocPercent(float current_soc_percent, float current_voltage, ChargeState_t current_charge_state);
static float updateDisplayPercent(float current_display_percent, float target_percent);
static int8_t percentToDisplay(float percent);

/**
 * @brief 실제 측정값 기준
 */
static const float ocv_table[][OCV_MAX] =
{
  {4.20f, 100.0f},
  {4.15f,  99.0f},
  {4.13f,  98.0f},
  {4.10f,  97.0f},
  {4.06f,  96.0f},
  {4.03f,  95.0f},
  {4.01f,  94.0f},
  {3.99f,  93.0f},
  {3.97f,  92.0f},
  {3.95f,  91.0f},
  {3.93f,  90.0f},
  {3.91f,  88.0f},
  {3.90f,  86.0f},
  {3.89f,  84.0f},
  {3.88f,  82.0f},
  {3.87f,  80.0f},
  {3.86f,  78.0f},
  {3.85f,  76.0f},
  {3.84f,  73.0f},
  {3.83f,  70.0f},
  {3.82f,  67.0f},
  {3.81f,  64.0f},
  {3.80f,  61.0f},
  {3.79f,  58.0f},
  {3.78f,  55.0f},
  {3.77f,  52.0f},
  {3.76f,  49.0f},
  {3.75f,  46.0f},
  {3.74f,  43.0f},
  {3.73f,  40.0f},
  {3.72f,  37.0f},
  {3.71f,  34.0f},
  {3.70f,  31.0f},
  {3.69f,  28.0f},
  {3.68f,  25.0f},
  {3.67f,  23.0f},
  {3.66f,  21.0f},
  {3.65f,  19.0f},
  {3.64f,  17.0f},
  {3.62f,  15.0f},
  {3.60f,  13.0f},
  {3.58f,  11.0f},
  {3.56f,   9.0f},
  {3.54f,   8.0f},
  {3.52f,   7.0f},
  {3.49f,   6.0f},
  {3.45f,   4.5f},
  {3.42f,   3.5f},
  {3.36f,   2.5f},
  {3.16f,   1.0f},
  {3.00f,   0.0f},
};





static const int ocv_table_size = sizeof(ocv_table) / sizeof(ocv_table[0]);

static BatteryState_t battery_state =
{
  BAT_INIT_VALUE,
  BAT_INIT_VALUE,
  BAT_INIT_VALUE,
  CHARGE_STATE_DISCONNECTED,
  false,
};

void batteryInit(void)
{
  pinMode(BATT_CHARGE_DETECT, INPUT_PULLUP);
  pinMode(BATT_CHARGE_DONE, INPUT_PULLUP);
  analogSetPinAttenuation(BATT_CURRENT_VOLT, ADC_11db);

  // *1). 충전 state 초기화
  battery_state.charge_state = initChargeState();

  // ESP의 ADC 설정 직후 바로 읽으면 불안정한 값이 나올 수 있다.

  // *2). 잠시 대기한 뒤 여러 샘플을 평균 내어 초기 베이스값을 안정화한다.
  delay(10);

  float sum = 0.0f;
  for (int i = 0; i < BAT_INIT_SAMPLES; i++)
  {
    sum += readVoltage();
  }

  float avg_volt = sum / BAT_INIT_SAMPLES;

  // *3). 현재 상태가 충전중이라면 offset만큼 조절하여 전압값을 맞춰준다.
  if (battery_state.charge_state == CHARGE_STATE_CHARGING)
  {
    avg_volt -= BAT_CHARGE_VOLTAGE_OFFSET;
  }
  
  // 3-1). 전압 시작점
  battery_state.filtered_voltage = avg_volt;
  
  // *4). 전압을 퍼센트로 바꿔준다.
  float avg_percent = voltageToPercent(avg_volt);
  
  // *5). 만약 충전 완료 상태라면 100을 넘어가지 않도록 한다.
  if (battery_state.charge_state == CHARGE_STATE_DONE)
  {
    avg_percent = 100.0f;
  }

  // 배터리 잔량 시작점
  battery_state.soc_percent = avg_percent;

  // display 표시 시작점
  battery_state.display_filtered_percent = battery_state.soc_percent;

  // 보호 해제
  battery_state.is_need_shutdown = false;
}

void batteryUpdate(void)
{
  // *1). 충전 여부를 검사한다.
  battery_state.charge_state = updateChargeState(battery_state.charge_state);

  // *2). 현재 전압값을 읽는다.
  float raw_voltage = readVoltage();
  float current_voltage = raw_voltage;

  // *3). 방전중 + 임계 전압 아래로 떨어질 경우 Shutdown 플래그를 올린다.
  if (battery_state.charge_state == CHARGE_STATE_DISCONNECTED && current_voltage <= BAT_SHUTDOWN_VOLTAGE)
  {
    battery_state.is_need_shutdown = true;
#if TEST_BATT_DEBUG
    printBatteryState(raw_voltage, current_voltage);
#endif
    return;
  } 
  // 반대로 충전중이면 플래그는 false로 두고 deep sleep모드로 빠지지 않게 만든다.
  else if (battery_state.charge_state != CHARGE_STATE_DISCONNECTED)
  {
    battery_state.is_need_shutdown = false;
  }

  // *4). 전압을 EMA로 평활화한다. (ADC의 이산 노이즈에 의해 필수..)
  //     충전 중이면 오프셋을 빼서 OCV에 가깝게 보정한다.
  if (battery_state.charge_state == CHARGE_STATE_CHARGING)
  {
    current_voltage -= BAT_CHARGE_VOLTAGE_OFFSET;
  }
  
  // 처음 해당 루프에 들어왔을 경우 초기 전압값으로 넣어준다.
  if (battery_state.filtered_voltage == BAT_INIT_VALUE)
  {
    battery_state.filtered_voltage = current_voltage;
  }
  else
  {
    battery_state.filtered_voltage += BAT_VOLTAGE_EMA_ALPHA * (current_voltage - battery_state.filtered_voltage);
  }

  // *5). 필터로 평활화된 전압을 퍼센트로 변환하고 단조성 방향 규칙으로 SOC 잔량을 갱신한다.
  battery_state.soc_percent = getNextSocPercent(battery_state.soc_percent,
                                                battery_state.filtered_voltage,
                                                battery_state.charge_state);

  // *6). 사용자 인터페이스를 위해 디스플레이 필터를 적용한다.
  battery_state.display_filtered_percent = updateDisplayPercent(battery_state.display_filtered_percent,
                                                                battery_state.soc_percent);

#if TEST_BATT_DEBUG
  printBatteryState(raw_voltage, current_voltage);
#endif
}

int8_t batteryGetPercent(void)
{
  return percentToDisplay(battery_state.display_filtered_percent);
}

bool batteryIsCharging(void)
{
  return battery_state.charge_state == CHARGE_STATE_CHARGING;
}

bool batteryIsChargeDone(void)
{
  return battery_state.charge_state == CHARGE_STATE_DONE;
}

bool batteryIsDown(void)
{
  return battery_state.is_need_shutdown;
}

/**
 * @brief 분압된 ADC 값을 읽고 실제 배터리 전압으로 역산
 */
static float readVoltage(void)
{
  int   raw_mv  = analogReadMilliVolts(BATT_CURRENT_VOLT);
  float voltage = (float)raw_mv / 1000.0f * BAT_DIVIDER_RATIO;
  voltage      += BAT_CALIBRATION_OFFSET;
  return voltage;
}

/**
 * @brief 룩업 테이블에서 전압 → 퍼센트 변환 (선형 보간)
 */
static float voltageToPercent(float voltage)
{
  if (voltage >= ocv_table[0][VOLTAGE])
    return ocv_table[0][PERCENTAGE];

  if (voltage <= ocv_table[ocv_table_size - 1][VOLTAGE])
    return ocv_table[ocv_table_size - 1][PERCENTAGE];

  for (int i = 0; i < ocv_table_size - 1; i++)
  {
    float v_high = ocv_table[i][VOLTAGE];
    float v_low  = ocv_table[i + 1][VOLTAGE];

    if (voltage >= v_low)
    {
      float p_high = ocv_table[i][PERCENTAGE];
      float p_low  = ocv_table[i + 1][PERCENTAGE];
      return p_low + (voltage - v_low) / (v_high - v_low) * (p_high - p_low);
    }
  }
  return 0.0f;
}

/**
 * @brief pin이 low상태인지 체크하는 함수
 */
static bool readActiveLowPin(uint8_t pin)
{
  return digitalRead(pin) == LOW;
}

/**
 * @brief 충전 감지핀과 완료핀을 읽는다. 
 * @return 유효한 상태이면 true, 동시 활성(비정상)이면 false
 */
static bool isChargePinsOk(bool *charge_detected, bool *charge_done)
{
  *charge_detected = readActiveLowPin(BATT_CHARGE_DETECT);
  *charge_done     = readActiveLowPin(BATT_CHARGE_DONE);
  
  // *두 핀 모두 LOW인 상태는 예외로 처리한다.
  if (*charge_detected && *charge_done)
  {
    return false;
  }
  return true;
}

/**
 * @brief 부팅 시 충전 상태를 초기화한다.
 *        두 핀에 대한 동시 활성 상태가 감지되면 예외이므로 짧은 간격으로 재시도하여 유효 상태를 확보한다.
 */
static ChargeState_t initChargeState(void)
{
  bool charge_detected = false;
  bool charge_done     = false;
 

  // *핀이 모순 상태가 아닐때까지 retry한다.
  for (int i = 0; i < BAT_INIT_CHARGE_RETRY_COUNT; i++)
  {
    if (isChargePinsOk(&charge_detected, &charge_done))
    {
      if (charge_done)
      {
        return CHARGE_STATE_DONE;
      }
      else if (charge_detected)
      {
        return CHARGE_STATE_CHARGING;
      }
      else
      {
        return CHARGE_STATE_DISCONNECTED;
      }
    }
    delay(BAT_INIT_CHARGE_RETRY_MS);
  }

  return CHARGE_STATE_DISCONNECTED;
}

/**
 * @brief 운용 중 충전 상태를 갱신한다.
 *        핀 상태가 안정될 때까지 디바운싱을 적용한다.
 *        동시 활성 상태는 무시하고 마지막 유효 상태를 유지한다.
 */
static ChargeState_t updateChargeState(ChargeState_t current_charge_state)
{
  bool charge_detected = false;
  bool charge_done     = false;
  
  // *1). 현재 충전핀 상태에 모순은 없는지 체크한다.
  if (!isChargePinsOk(&charge_detected, &charge_done))
  {
    return current_charge_state;
  }
  
  // *2). 다음 충전 상태를 갱신한다.
  ChargeState_t next_charge_state = CHARGE_STATE_DISCONNECTED;

  if (charge_done)
  {
    next_charge_state = CHARGE_STATE_DONE;
  }
  else if (charge_detected)
  {
    next_charge_state = CHARGE_STATE_CHARGING;
  }

  // *3). 충전 상태 핀은 노이즈에 의해 순간적으로 튈수도있으므로, 이전에 읽은 충전 상태가 일정시간 유지된 후에 반영하도록 한다.
  static ChargeState_t prev_charge_state = CHARGE_STATE_DISCONNECTED;
  static uint32_t prev_charge_state_ms   = 0;
  
  // *4). next_charge_state가 바뀌었다면, 현재 시간을 기록해둔다.
  if (next_charge_state != prev_charge_state)
  {
    prev_charge_state = next_charge_state;
    prev_charge_state_ms = millis();
    return current_charge_state;
  }
  
  // *5). "BAT_CHARGE_DEBOUNCE_MS" 만큼의 시간이 지났음에도 상태가 변하지 않았다면 
  if (millis() - prev_charge_state_ms < BAT_CHARGE_DEBOUNCE_MS)
  {
    return current_charge_state;
  }
  
  // *6). 상태를 확정시킨다.
  return prev_charge_state;
}

/**
 * @brief 평활화된 전압으로 남은 배터리 잔량(SOC)를 갱신한다.
 */
static float getNextSocPercent(float current_soc_percent, float current_voltage, ChargeState_t current_charge_state)
{
  // *1). 인자로 받은 전압을 퍼센트로 변환한다.
  float voltage_percent = voltageToPercent(current_voltage);

  // *2). 관측 잔량과 현재 관리 잔량의 차이를 계산한다.
  float diff = voltage_percent - current_soc_percent;

  // *3). 기본값은 현재 관리 잔량 유지.
  float next_soc_percent = current_soc_percent;

  switch (current_charge_state)
  {
    case CHARGE_STATE_DONE:
      // 충전 완료 상태는 전압 추정값을 기다리지 않고 즉시 100%로 고정한다.
      next_soc_percent = 100.0f;
      break;

    case CHARGE_STATE_DISCONNECTED:
      // 방전 중에는 잔량 하강만 허용한다.
      if (diff < 0.0f)
      {
        next_soc_percent = current_soc_percent + BAT_SOC_EMA_ALPHA * diff;
      }
      break;

    case CHARGE_STATE_CHARGING:
      // 충전 중에는 잔량 상승만 허용한다.
      if (diff > 0.0f)
      {
        next_soc_percent = current_soc_percent + BAT_SOC_EMA_ALPHA * diff;

        // 충전 완료 신호 전에는 100%로 표시하지 않기 위해 99%에서 제한한다.
        if (next_soc_percent > 99.0f)
        {
          next_soc_percent = 99.0f;
        }
      }
      break;

    default:
      // 정의되지 않은 상태에서는 기존 잔량을 유지한다.
      break;
  }

  return next_soc_percent;
}

/**
 * @brief SOC 목표값을 향해 EMA 필터로 부드럽게 추종한다.
 *        차이가 ±0.5 이내이면 즉시 수렴시킨다.
 */
static float updateDisplayPercent(float current_display_percent, float target_percent)
{
  if (current_display_percent < 0)
  {
    return target_percent;
  }

  float diff = target_percent - current_display_percent;
  if (diff > -0.5f && diff < 0.5f)
  {
    return target_percent;
  }

  return current_display_percent + BAT_DISPLAY_EMA_ALPHA * diff;
}

/**
 * @brief 실수 퍼센트를 표시용 정수(0~100)로 변환한다.
 */
static int8_t percentToDisplay(float percent)
{
  if (percent <= 0.0f)
  {
    return 0;
  }
  if (percent >= 100.0f)
  {
    return 100;
  }
  return (int8_t)(percent + 0.5f);
}
