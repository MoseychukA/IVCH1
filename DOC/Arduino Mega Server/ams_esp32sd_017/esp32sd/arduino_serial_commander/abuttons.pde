/*
  Modul Buttons
  part of Arduino Serial Commander
  part of Arduino Mega Server project
*/

// Button "Upload"
int    butUploadX = 50;
int    butUploadY = 296;
byte   butUploadLenX = 100;
byte   butUploadLenY = 20;
String butUploadTitle = "Upload!";
color  butUploadColorActive = color(130, 80, 80);
color  butUploadStrokeActive = color(160, 120, 120);
color  butUploadColorPassive = color(140);
color  butUploadStrokePassive = color(153);
boolean mouseOverButUpload = false;

// Button "Open"
int    butOpenX = 165;
int    butOpenY = 296;
byte   butOpenLenX = 40;
byte   butOpenLenY = 20;
String butOpenTitle = "Open";
color  butOpenColorActive   = color( 80, 130,  80);
color  butOpenStrokeActive  = color(120, 160, 120);
color  butOpenColorPassive  = color(140);
color  butOpenStrokePassive = color(153);

// Button "Send command"
int    butCommandX = 50;
int    butCommandY = 296;
byte   butCommandLenX = 120;
byte   butCommandLenY = 20;
String butCommandTitle = "Send command";
color  butCommandColorActive   = color(160,  90,  60);
color  butCommandStrokeActive  = color(160, 120, 120);
color  butCommandColorPassive  = color(140);
color  butCommandStrokePassive = color(153);

// Button "Time Sync"
int    butTimeX = 190;
int    butTimeY = 296;
byte   butTimeLenX = 100;
byte   butTimeLenY = 20;
String butTimeTitle = "Time Sync";
color  butTimeColorActive   = color( 60,  90, 160);
color  butTimeStrokeActive  = color(120, 120, 160);
color  butTimeColorPassive  = color(140);
color  butTimeStrokePassive = color(153);

void drawButton(int x, int y, byte lenX, byte lenY, String title, color butColor, color butStroke) {
  fill(butColor);
  stroke(butStroke);
  rect(x, y, lenX, lenY);
  fill(255);
  text(title, x + (lenX / 2 - title.length() * 3) - 2, butUploadY + 15);
}

void butOpenActive()    {drawButton(butOpenX,    butOpenY,    butOpenLenX,    butOpenLenY,    butOpenTitle,    butOpenColorActive,     butOpenStrokeActive);}
void butOpenPassive()   {drawButton(butOpenX,    butOpenY,    butOpenLenX,    butOpenLenY,    butOpenTitle,    butOpenColorPassive,    butOpenStrokePassive);}
void butUploadActive()  {drawButton(butUploadX,  butUploadY,  butUploadLenX,  butUploadLenY,  butUploadTitle,  butUploadColorActive,   butUploadStrokeActive);}
void butUploadPassive() {drawButton(butUploadX,  butUploadY,  butUploadLenX,  butUploadLenY,  butUploadTitle,  butUploadColorPassive,  butUploadStrokePassive);}
void butCommandActive() {drawButton(butCommandX, butCommandY, butCommandLenX, butCommandLenY, butCommandTitle, butCommandColorActive,  butCommandStrokeActive);}
void butCommandPassive(){drawButton(butCommandX, butCommandY, butCommandLenX, butCommandLenY, butCommandTitle, butCommandColorPassive, butCommandStrokePassive);}
void butTimeActive()    {drawButton(butTimeX,    butTimeY,    butTimeLenX,    butTimeLenY,    butTimeTitle,    butTimeColorActive,     butTimeStrokeActive);}
void butTimePassive()   {drawButton(butTimeX,    butTimeY,    butTimeLenX,    butTimeLenY,    butTimeTitle,    butTimeColorPassive,    butTimeStrokePassive);}

boolean mouseOverButtOpen()    {if (regionMouseOver(butOpenX,    butOpenY,    butOpenLenX,    butOpenLenY))    {return true;} else {return false;}}
boolean mouseOverButtCommand() {if (regionMouseOver(butCommandX, butCommandY, butCommandLenX, butCommandLenY)) {return true;} else {return false;}}
boolean mouseOverButtTime()    {if (regionMouseOver(butTimeX,    butTimeY,    butTimeLenX,    butTimeLenY))    {return true;} else {return false;}}

boolean mouseOverButtUpload(){
  if (regionMouseOver(butUploadX, butUploadY, butUploadLenX, butUploadLenY)){
    mouseOverButUpload = true;
    return true;
  } else {
      mouseOverButUpload = false;
      return false;
   }
}

