#ifndef PINS_H
#define PINS_H

/************************************
 * @brief pinouts - IIOT_V4
*************************************/

/**
 * @brief Ethernet connection
 */
#define PIN_ETH_CS 15
#define PIN_ETH_SCLK 10
#define PIN_ETH_MISO 11
#define PIN_ETH_MOSI 12
#define PIN_ETH_INT  13
#define PIN_ETH_RST  14

/*******
 * RS485 pinouts
 *******/
#define PIN_RS485_1_RX 5
#define PIN_RS485_1_TX 6
#define PIN_RS485_1_RTS 7

#define PIN_RS485_2_RX 40
#define PIN_RS485_2_TX 41
#define PIN_RS485_2_RTS 42

#define PIN_WS2812B 38

#define PIN_SD_CARD_CLK  10
#define PIN_SD_CARD_MISO 11
#define PIN_SD_CARD_MOSI 12
#define PIN_SD_CARD_CS 36    

#define PIN_I2C_SDA 1
#define PIN_I2C_SCL 2

/**
 * @brief 4G Module 
 */
#define PIN_GSM_PWR_KEY 21
#define PIN_SIM_RX      17
#define PIN_SIM_TX      18

// For fast led
#define PIN_LED_WS2812_DATA 38

#define PIN_BUTTON_ON_BOARD 4

#define PIN_BUZZER 16

#endif // PINS_H