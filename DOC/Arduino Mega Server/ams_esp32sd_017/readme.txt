  Arduino Mega Server for ESP32 SD
  version 0.17
  2017, Hi-Lab.ru
  
  License: Free for personal use and education, without any warranties
  Home:    http://hi-lab.ru/arduino-mega-server (Russian)
           http://hi-lab.ru/english/arduino-mega-server (English)
  E-mail:  info@hi-lab.ru

  AMS Pro: http://hi-lab.ru/arduino-mega-server/ams-pro (projects and commercial use)
  
  IDE:      Arduino 1.6.5 r2
  Hardware: ESP32 with SD card

  Connections:
  ------------
  SD Card | ESP32
    D3       SS
    CMD      MOSI
    VSS      GND
    VDD      3.3V
    CLK      SCK
    VSS      GND
    D0       MISO

  Optional:
  ---------
  LED (yellow) - D4
  LED (red)    - D21
  LED (green)  - D22
  RGB LED      - D32, D27, D33

  Pathes of the Project:
  ----------------------
  Arduino IDE settings:
  ...\Documents\Arduino

  Libraries folder:
  ...\Documents\Arduino\libraries\

  Sketches folders:
  ...\Sketches\esp32sd\Arduino\arduino_mega_server_esp32sd\
  ...\Sketches\esp32sd\arduino_serial_commander\
  
  Settings:
  ---------
  In Sketch (Wi-Fi): SSID, PASSWORD of your Wi-Fi network
  In Browser:        IP address 192.168.1.70

  Quick start:
  ------------
  0. Connect ESP32 module and microSD card
  1. Install Arduino IDE 1.6.5 (r2)
  2. Install ESP32 drivers (https://github.com/espressif/arduino-esp32)
  3. Put files from folder "sd" to microSD card
  4. Print SSID and PASSWORD of your Wi-Fi router in tab "Wi-Fi" of sketch
  5. Upload Sketch "arduino_mega_server_esp32sd" to module ESP32
  6. Open in your browser address "192.168.1.70"
  7. Enjoy and donate on page http://hi-lab.ru/arduino-mega-server/details/donate
