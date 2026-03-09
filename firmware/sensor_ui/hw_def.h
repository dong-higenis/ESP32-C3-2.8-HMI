
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

#define LCD_BACKLIGHT 0
#define SENSOR_RX 19
#define SENSOR_TX 18 

#define LED_PIN   10