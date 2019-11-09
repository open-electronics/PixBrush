# PixBrush
Paint with light! (A light painting NeoPixel tool)

## Hardware
- Buy the kit on https://www.futurashop.it/
- Assemble the kit (including touch screen display, Arduino Mega and NeoPixel bar)

## Software
- Open the sketch and setup your Arduino IDE selecting: “Arduino/Genuino Mega or Mega 2560” board.
- Get all the libraries required (most on http://www.fishino.it/download-libraries-it.html)
- Edit file "FishinoXPT2046.h" and uncomment/set following lines:
```
    #define TOUCH_CS  6
    #define TOUCH_IRQ 3
```
- Upload the sketch
- Have fun!

## Usage
1) With Paint of other image editing software create and save a "24 bit Bitmap" image
2) The image that you want to show must be rotated to the right by 90 degrees
3) The image must have exactly the width equal to the number of NeoPixels used on the PixBrush (example: 144 NeoPixels = 144px)
4) Upload the image on the SD card and put the SD card on PixBrush
5) Switch on PixBrush and set to your smartphone or reflex a long exposure time (example: 15sec.) and low ISO (exampe: 100)
6) While taking the picture, move in front of the lens from left to right by starting the image scan and holding PixBrush vertically
7) Try to maintain a constant and fluid movement
