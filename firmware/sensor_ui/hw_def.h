#ifndef HW_DEF_H
#define HW_DEF_H

/**  
 * @brief h/w 설정
 */

/**
* @brief 사용 lcd
*/
#define _USE_ER_TFT028A2_4        1
#define _USE_THL_CD12             0

/**
 * @brief lcd 해상도
 */
#define LCD_WIDTH               320
#define LCD_HEIGHT              240

/**
 * @brief 핀 설정
 */
#define SPI_MISO_PIN             -1
#define SPI_MOSI_PIN              4
#define SPI_SCLK_PIN              6
#define LCD_CS_PIN                5
#define LCD_DC_PIN                7
#define LCD_RESET_PIN            -1

#define LCD_BACKLIGHT             0
#define SENSOR_RX                19
#define SENSOR_TX                18 

#define LED_PIN                  10

#define BTN_PIN                   2

#define BATT_CHARGE_DETECT        1
#define BATT_CURRENT_VOLT         3

/**
 * @brief 버튼 분압 저항 
 */
#define RESISTOR_PULLUP_VALUE     10000.0f 

#define RESIST_S1_R2_VALUE        0.0f
#define RESIST_S2_R2_VALUE        10000.0f
#define RESIST_S3_R2_VALUE        20000.0f
#define RESIST_S4_R2_VALUE        30000.0f
#define RESIST_RELEASED_R2_VALUE  40000.0f

#define REFERENCE_MILLI_VOLTAGE   3300.0f

/**
 * @brief 배터리 상수
 */
#define BATT_VOLTAGE_MAX        4.096f  // 완충 전압 (V), 정확히는 회로상 VBUS가 최대 충전량이라고 유추 가능하기때문. 4.096은 VBUS 실측값
#define BATT_VOLTAGE_NOM        3.76f
#define BATT_VOLTAGE_LOW        3.37f
#define BATT_VOLTAGE_MIN        3.00f

// BAT ADC 분압 회로 (R44, R42)
#define BATT_R1_BAT             126.0f  // 상단 저항 (kΩ)
#define BATT_R2_GND             330.0f  // 하단 저항 (kΩ)
#define BAT_DIVIDER_RATIO       ((BATT_R1_BAT + BATT_R2_GND) / BATT_R2_GND)  // 1.3818

#define BAT_CALIBRATION_OFFSET  0.06f  // 실측 기반 보정값 (V)

/**
 * @brief SLEEP 모드
 */
#define SLEEP_TIMEOUT_MS        60000

typedef enum
{
  BTN_NONE = 0,
  BTN_S1,
  BTN_S2,
  BTN_S3,
  BTN_S4,
  BUTTON_PIN_MAX,
} Button_t;

#endif