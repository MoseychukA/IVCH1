/* -----------------------------------------   

  scripts.js 0.16
  part of Arduino Mega Server project
-------------------------------------------- */

// Date & time

function month(mon) {
  switch (mon) {
    case "1": return "января"; break;
    case "2": return "февраля"; break;
    case "3": return "марта"; break;
    case "4": return "апреля"; break;
    case "5": return "мая"; break;
    case "6": return "июня"; break;
    case "7": return "июля"; break;
    case "8": return "августа"; break;
    case "9": return "сентября"; break;
    case "10": return "октября"; break;
    case "11": return "ноября"; break;
    case "12": return "декабря"; break;
    default: return ""; break;
  }
}

function weekday(day) {
  switch (day) {
    case "1": return "Воскресенье"; break;
    case "2": return "Понедельник"; break;
    case "3": return "Вторник"; break;
    case "4": return "Среда"; break;
    case "5": return "Четверг"; break;
    case "6": return "Пятница"; break;
    case "7": return "Суббота"; break;
     default: return ""; break;
  }
}

// Network

function randomNoCache() {
  var random = Math.round(Math.random() * 10000);
  if (random < 10) {random *= 1000;}
  if (random < 100) {random *= 100;}
  if (random < 1000) {random *= 10;}
  return "&nocache=" + random;
}

// Data colors

var modulDisable = "#555555";
var modulPassive = "#887711";
var modulActive  = "#1188dd";

function modulBackground       (state) {switch (state) {case "0": return modulPassive; break; case "1": return modulActive; break; default: return modulDisable; break;}}
function modulColor            (state) {switch (state) {case "0": return "#eeeeee"; break; case "1": return "#ffffff"; break; default: return "#aaaaaa"; break;}}

function forceBackground       (state) {switch (state) {case "0": return "#6EA1D2"; break; case "1": return "#F4A509"; break; default: return "#555555"; break;}}
function forceColor            (state) {switch (state) {case "0": return "#ffffff"; break; case "1": return "#ffffff"; break; default: return "#aaaaaa"; break;}}
function timeProviderBackground(state) {switch (state) {case "1": return "#91A46B"; break; case "2": return "#A28D85"; break; default: return "#555555"; break;}}
function timeProviderColor     (state) {switch (state) {case "0": return "#ffffff"; break; case "1": return "#ffffff"; break; case "2": return "#ffffff"; break; default: return "#aaaaaa"; break;}}
function authBackground        (state) {switch (state) {case "0": return "#03B882"; break; case "1": return "#DA7063"; break; default: return "#555555"; break;}}
function authColor             (state) {switch (state) {case "0": return "#ffffff"; break; case "1": return "#ffffff"; break; default: return "#aaaaaa"; break;}}

function onlineBackground      (state) {switch (state) {case "0": return "#555555"; break; case "1": return "#3EAF3B"; break; default: return "#555555"; break;}}
function onlineColor           (state) {switch (state) {case "0": return "#aaaaaa"; break; case "1": return "#ffffff"; break; default: return "#aaaaaa"; break;}}

function contactBackground     (state) {switch (state) {case "0": return "#68974C"; break; case "1": return "#B93737"; break; default: return "#555555"; break;}}
function contactColor          (state) {switch (state) {case "0": return "#eeeeee"; break; case "1": return "#ffffff"; break; default: return "#aaaaaa"; break;}}
function pirBackground         (state) {switch (state) {case "0": return "#09999E"; break; case "1": return "#eedd22"; break; default: return "#555555"; break;}}
function pirColor              (state) {switch (state) {case "0": return "#ffffff"; break; case "1": return "#555555"; break; default: return "#aaaaaa"; break;}}

function stateOnOffBackground (state) {switch (state) {case "0": return "#555555"; break; case "1": return "#FAF39B"; break; default: return "#555555"; break;}}
function stateOnOffColor      (state) {switch (state) {case "0": return "#ffffff"; break; case "1": return "#000000"; break; default: return "#aaaaaa"; break;}}
function stateOnOff           (state) {switch (state) {case "0": return "OFF";     break; case "1": return "ON";      break; default: return "-";       break;}}

/* ------------------------------
  Graphics Logo Works
--------------------------------- */

var shiftX = 20;
var shiftY = 10;
var lenX = 136;
var lenY = 136;
var logoColor = 'rgba(120, 170, 220, 1)';
var logoColorLight = 'rgba(50, 110, 150, 1)';
var width = 1;

function drawBorder(x, y, lenX, lenY, color) {
  var cor = 2;
  drawXline(x - cor, x + lenX + cor, y - 1,        1, color);
  drawXline(x - cor, x + lenX + cor, y + lenY + 1, 1, color);
  drawYline(x - 1,        y - cor, y + lenY + cor, 1, color);
  drawYline(x + lenX + 1, y - cor, y + lenY + cor, 1, color);
}

function clearLogo(place) {
  // context works
  try {
    element = document.getElementById(place);
    context = element.getContext('2d');
  } catch (err) {
      return;
    }

  // clear background
  context.fillStyle = 'black';
  context.fillRect(0, 0, element.width, element.height);

  // draw border
  drawBorder(shiftX, shiftY, lenX, lenY, logoColor);
}

/* ------------------------------
  Ping-Pong logo [1]

--------------------------------- */

function drawPong(place, x, y, color) {
  // context works
  try {
    element = document.getElementById(place);
    context = element.getContext('2d');
  } catch (err) {
      return;
    }
  
  // clear background
  context.fillStyle = 'black';
  context.fillRect(0, 0, element.width, element.height);

  // draw Pong
  context.fillStyle = color;
  context.arc(shiftX + x, shiftY + y, 6, 0, Math.PI * 2);
  context.fill();
  context.beginPath();
  context.fill();

  // draw border
  drawBorder(shiftX, shiftY, lenX, lenY, logoColor);
} // drawPong( )

var pongDim = 6;
var pongMinX = 0 + pongDim;
var pongMinY = 0 + pongDim;
var pongMaxX = 136 - pongDim;
var pongMaxY = 136 - pongDim;
var dX = 1;
var dY = 1;
var pongX = 7;
var pongY = 25;

function logoPong() {
  var pongColor = logoColor;

  if (pongX < pongMinX || pongX > pongMaxX) {dX = -dX;}
  if (pongY < pongMinY || pongY > pongMaxY) {dY = -dY;}
  if (pongY == 100) {pongY += dY * 2;}

  pongX += dX;
  pongY += dY;

  drawPong('logo', pongX, pongY, pongColor);
}

/* ------------------------------
  Radar logo [2]

--------------------------------- */

function drawRadar(place, liferPos, width, color) {
  var halfX = lenX / 2;
  var halfY = lenY / 2;
  liferPos = 34 - liferPos;

  // context works
  try {
    element = document.getElementById(place);
    context = element.getContext('2d');
  } catch (err) {
      return;
    }
  
  // draw radar
  context.lineWidth = width;
  context.beginPath();
  context.moveTo(shiftX, shiftY + (lenY - liferPos * 4));
  context.lineTo(shiftX + halfX, shiftY + halfX);
  context.lineTo(shiftX + (lenX - liferPos * 4), shiftY + lenY);
  context.moveTo(shiftX + (lenX - liferPos * 4), shiftY + lenY);
  context.moveTo(shiftX + (lenX - liferPos * 4), shiftY);
  context.lineTo(shiftX + halfX, shiftY + halfX);
  context.lineTo(shiftX + lenX, shiftY + (lenY - liferPos * 4));
  context.moveTo(shiftX + lenX, shiftY + (lenY - liferPos * 4));
  context.closePath();
  context.strokeStyle = color;
  context.stroke();

  // draw border
  drawBorder(shiftX, shiftY, lenX, lenY, logoColorLight);
} // drawRadar( )

var liferLogo = 0;
var phase = 1;
var radarColor = logoColor;

function logoRadar() {
  liferLogo++;
  if (liferLogo > 34) {liferLogo = 0;}

  if (liferLogo == 0) {
    phase = -phase;
    if (phase == 1) {
      radarColor = logoColor;
      width = 1;
    } else {
        radarColor = 'black';
        width = 2;
      }
  }
  // draw
  drawRadar('logo', liferLogo, width, radarColor);
}

/* ------------------------------
  Random Rects logo [3]

--------------------------------- */

function drawRect(place, x, y, color) {
  var step = 12;
  var shift = 3;
  var dim = 10;

  // context works
  try {
    element = document.getElementById(place);
    context = element.getContext('2d');
  } catch (err) {
      return;
    }
  
  // draw rect
  context.fillStyle = color;
  context.fillRect(shiftX + x*step + shift, shiftY + y*step + shift, dim, dim);
  context.fill();
  context.beginPath();
  context.fill();

  // draw border
  drawBorder(shiftX, shiftY, lenX, lenY, logoColorLight);
}

function logoRandomRects() {
  var show = 'rgba(100, 120, 200, 0.7)';
  var hide ='rgba(0, 0, 0, 1)';

  // random position
  var x = Math.round(Math.random() * 10);
  var y = Math.round(Math.random() * 10);

  // random color
  var colorRnd = Math.round(Math.random() * 1000);
  if (colorRnd > 500) {
    var color = show;
  } else {
      var color = hide;
    }

  // anomal colors
  if (colorRnd == 60) {color = 'rgba(40, 218, 34, 1)';} // green
  if (colorRnd == 70) {color = 'rgba(228, 185, 34, 1)';} // yellow
  if (colorRnd == 80) {color = 'rgba(218, 50, 34, 1)';} // red

  // draw
  drawRect('logo', x, y, color);
}

/* ------------------------------
  Function logoRotator()
    Show graphics logo
--------------------------------- */

var curLogo = 0;
var maxLogo = 3;
var logo1timer;
var logo2timer;
var logo3timer;
var netActivtimer;

function logoRotator() {

  // network act
  clearInterval(netActivtimer);
  netActivtimer = setInterval('netLoad()', 1000);
  
  // random logo
  if (curLogo == 0) { // start
    var logoRnd = Math.round(Math.random() * 10);
    curLogo = 1;
    if (logoRnd > 0 && logoRnd <= 3) {curLogo = 1;}
    if (logoRnd > 3 && logoRnd <= 6) {curLogo = 2;}
    if (logoRnd > 6 && logoRnd <= 10) {curLogo = 3;}
  } else { // cycle
      curLogo++;
    }

  if (curLogo > maxLogo) {curLogo = 1;}

  // clear timers
  clearInterval(logo1timer);
  clearInterval(logo2timer);
  clearInterval(logo3timer);

  // show current logo
  switch (curLogo) {
    case 1:
      logo1timer = setInterval('logoPong()', 20);
      break;
    case 2:
      clearLogo('logo');
      logo2timer = setInterval('logoRadar()', 80);
      break;
    case 3:
      clearLogo('logo');
      logo3timer = setInterval('logoRandomRects()', 120);
      break;
    default:
      break;
    } // switch (type)

  // change logo (period 1 minute)
  setTimeout('logoRotator()', 60000);
} // logoRotator()

function rndNumber() {
  var random = Math.round(Math.random() * 100000);
  if (random < 10) {random *= 1000;}
  if (random < 100) {random *= 100;}
  if (random < 1000) {random *= 10;}
  return random;
}

function genNumber() {
  try {
    var magicNumber = rndNumber();
    document.getElementById("magic-number").innerHTML = magicNumber;
  } catch (err) { }
}

function netLoad() {
  try {
    load('cpu-load2', 2, 0, 0, bufferCpuLoad2, bufferCpuLoad3, 'net', 'rgba(100, 160, 230, 1)', 'rgba(100, 160, 230, 1)', 'rgba(100, 160, 230, 1)');
  } catch (err) { }
}

/* end */
