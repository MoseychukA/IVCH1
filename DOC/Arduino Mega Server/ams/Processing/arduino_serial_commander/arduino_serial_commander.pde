/*
  Arduino Serial Commander
  part of Arduino Mega Server project
  version 0.17
  ©Hi-Lab.ru, 2018
  
  License: Free for personal use, as is and without any warranties
  Home:    http://hi-lab.ru/arduino-mega-server (russian)
           http://hi-lab.ru/english/arduino-mega-server (english)
  Mail:    info@hi-lab.ru
  
  For commercial use version AMS Pro:
  https://hi-lab.ru/arduino-mega-server/ams-pro
*/

import processing.serial.*;
import java.util.Date;
import java.util.Calendar;
import java.util.GregorianCalendar;

String version  = "0.17";
String COM_PORT = "COM12"; // <----- your Arduino COM port
int COM_SPEED   = 115200;
Serial port;

byte menuActive2 = 0;

byte NONE    = 0;
byte RECEIVE = 1;
byte SEND    = 2;
byte listingMode = RECEIVE;

byte ON  = 1;
byte OFF = 0;
byte ledMode = OFF;

String filePath    = "";
String currentFile = "index.htm";

String lines[];
byte binarys[];

int currentLine         = 0;
long currentByte        = 0;
String receivingLines[] = {"", "", "", "", "", "", "", "", "", "", "", "", ""};
int currentReceiving    = 0;

int currentBinary = 0;
boolean protect   = false;

// upload prof
int startTime = 0;
int upTime    = 0;
int upSpeed   = 0;

// Font
PFont font;

// Work logic
boolean TRANSFER = false;
boolean DONE     = false;
boolean ANSWER   = false;

/* Functions
--------------------------------------- */

void sendWorks() {
  listingMode = SEND;
  clearListingBox();
  
  // TRANSFER
  if (currentBinary < binarys.length && TRANSFER) {
    transfer();
  }

  // DONE
  if (currentBinary >= binarys.length && !DONE) {
    sendEndTransfer();
    DONE     = true; 
    TRANSFER = false;
    protect  = false;
    currentBinary = 0;
    upTime = millis() - startTime;
    //ANSWER = true;
    //clearLines();
  }
  
  // ANSWER
  if (ANSWER) {
    answer();
  }

  // Button "Open"
  if (mouseOverButtOpen()) {
    butOpenActive();

    if (mousePressed && !TRANSFER) {
      delay(300);
      dialogOpenFile(); 
    }
  } else {
      butOpenPassive();
    }

  // Button "Upload"
  if (mouseOverButtUpload() && mousePressed && !TRANSFER) {
    delay(300);
    TRANSFER = true;
    DONE     = false; 
    reInit();
    loadBinarys();
    sendStartTransfer();
    startTime = millis();
  }
  
  if (TRANSFER || mouseOverButUpload) {
    butUploadActive();
  } else {
      butUploadPassive();
    }
  
  // draw interface
  drawInfoFile(40);
  drawInfoPort(40);
  
  if (DONE) {
    drawProgress(100, 100);
  } else {
      if (TRANSFER) {
        drawProgress(currentBinary, binarys.length);       
      } else {
          drawProgress(0, 100);
        }
    }
    
  drawListing();
} // sendWorks()

void receiveWorks() {
  listingMode = RECEIVE;
  clearListingBox();
  
  readPort();
  
  // Button "Send command"
  if (mouseOverButtCommand()) {
    butCommandActive();

    if (mousePressed == true) {
      delay(300);
      if (ledMode == ON) {
      port.write(LED_OFF);
      port.write('\n');
      ledMode = OFF;
    } else {
        port.write(LED_ON);
        port.write('\n');
        ledMode = ON;
      }
    delay(40);
    }
  } else {
      butCommandPassive();
    } // if (mouseOverButtCommand()
    
  // Button "Time Sync"
  if (mouseOverButtTime()) {
    butTimeActive();

    if (mousePressed) {
      delay(300);
      sendTime(getTimeNow());
    }
  } else {
      butTimePassive();
    }
    
  drawMonitoring(65);
  drawInfoPort(65);    
  drawListing();
} // receiveWorks()

void helpWorks() {
  listingMode = NONE;
  drawHelp();
}

/* Setup
--------------------------------------- */

void setup() {
  viewInit();
  fontInit();
  serialInit();
  receivingInit();
  loadLines();
  loadBinarys();
}

/* Draw
--------------------------------------- */

void draw() {
  clearBackground();
  drawMenu();
  
  switch(menuActive2) {
    case 0:
      sendWorks();
      break;
    case 1:
      if (!TRANSFER) {
        receiveWorks();
      }
      break;
    case 2:
      if (!TRANSFER) {
        helpWorks();
      }
      break;      
  }
  drawLogo(360, 304);
} // draw()

