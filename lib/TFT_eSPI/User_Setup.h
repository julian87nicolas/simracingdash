// User_Setup.h for NodeMCU (ESP8266) SPI TFT display (480x320)
// Edit pins below to match your wiring. Uncomment the driver for your panel.

// Uncomment the driver you use (one only)
#define ILI9488_DRIVER
//#define ST7796_DRIVER

// Display size
#define TFT_WIDTH  480
#define TFT_HEIGHT 320

// ESP8266 SPI pins (NodeMCU v2)
#define TFT_MOSI D7
#define TFT_SCLK D5
#define TFT_CS   D0
#define TFT_DC   D1
#define TFT_RST  D2
// Optional backlight pin
//#define TFT_BL  D2

// SPI frequency (ILI9488 on ESP8266 is usually more stable at 20MHz)
#define SPI_FREQUENCY 20000000

// Use hardware SPI
#define SPI_HAS_TRANSACTION

// Color definitions are handled by library

// If your display needs different pins or a different driver, edit accordingly.
