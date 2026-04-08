#pragma once
#include <TFT_eSPI.h>
#include <qrcode.h>

// Renders a QR code for `text` onto the display at the given position and pixel size.
// `pixelSize` controls the size of each QR module in display pixels.
inline void drawQRCode(TFT_eSPI* tft, const char* text, int xOffset, int yOffset, int pixelSize) {
  QRCode qrcode;
  uint8_t qrcodeData[qrcode_getBufferSize(3)];
  qrcode_initText(&qrcode, qrcodeData, 3, ECC_LOW, text);

  int qrSizePx = qrcode.size * pixelSize;
  int border = pixelSize * 2;
  tft->fillRect(xOffset - border, yOffset - border,
                qrSizePx + border * 2, qrSizePx + border * 2, TFT_WHITE);

  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      uint16_t color = qrcode_getModule(&qrcode, x, y) ? TFT_BLACK : TFT_WHITE;
      tft->fillRect(xOffset + x * pixelSize, yOffset + y * pixelSize,
                    pixelSize, pixelSize, color);
    }
  }
}
