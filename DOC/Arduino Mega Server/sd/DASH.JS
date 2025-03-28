/* -----------------------------------------
  dash.js
  part of Arduino Mega Server project
-------------------------------------------- */

var currentPage = '';

// Var's for Processing
var dash_marker1 = 0;   // ok XML func
var dash_marker2 = 0;   // not ok XML func
var dash_marker3 = 0;   // in XML func
var liferFloat   = 0.0; // lifer volume
var fig          = 2;   // type of 3D model

// Timeline graphics
const MAX_CPU_LOAD_BUFFER = 16;
const MAX_GRAPH_BUFFER    = 150;
const MAX_FORM_BUFFER     = 90;

// CPU Buffers
var bufferCpuLoad1 = [];
var bufferCpuLoad2 = [];
var bufferCpuLoad3 = [];
var bufferCpuLoad4 = [];
var bufferCpuLoad5 = [];
var bufferCpuLoad6 = [];

// Electro buffers
var bufferElectro1 = [];
var bufferElectro2 = [];
var bufferElectro3 = [];
var bufferElectro4 = [];
var bufferElectro5 = [];
var bufferElectro6 = [];

// Modiles
var moduleName = ['NTP',        'SDI',            'SDF',             'SND',         'MAJ',          'LRT',            'UPL',           'PIR',         'CNT',             'TDS',            'EPM',            'LED',         'KEY',         'NMR',           'NMT',           'NRF'];
var moduleXml  = ['mNtp',       'mSdInfo',        'mSdFiles',        'mSend',       'mMajor',       'mLaurent',       'mUpload',       'mPirs',       'mConts',          'mDs18b20',       'mPm',            'mLeds',       'mKeys',       'mNooMr',        'mNooMt',        'mNrf24'];
var moduleId   = ['module-ntp', 'module-sd-info', 'module-sd-files', 'module-send', 'module-major', 'module-laurent', 'module-upload', 'module-pirs', 'module-contacts', 'module-ds18b20', 'dash-module-pm', 'module-leds', 'module-keys', 'module-mr1132', 'module-mt1132', 'module-nrf24'];

// Contacts
var contName = ['CN1',        'CN2'];
var contXml  = ['cont1',      'cont2'];
var contId   = ['dash-cont1', 'dash-cont2'];

// PIR's
var pirName = ['PR1',       'PR2',       'PR3',       'PR4',       'PR5'];
var pirXml  = ['pir1',      'pir2',      'pir3',      'pir4',      'pir5'];
var pirId   = ['dash-pir1', 'dash-pir2', 'dash-pir3', 'dash-pir4', 'dash-pir5'];

// Ports
var digitalPins = ['D2','D3','D4','D5','D6','D7','D8','D9','D10', 'D22','D23','D24','D25','D26','D27', 'D30','D31'];
var analogPins  = ['A0','A1','A2','A3','A4','A5','A6','A7','A8','A9','A10','A11','A12','A13','A14','A15','A16'];

// Electro
var UiId = ['volt', 'p1', 'p2', 'p3', 'p4', 'p5', 'p6', 'p7', 'p8', 'p9', 'p10', 'p11', 'p12', 'p13'];
var ui = [];

// Get Dash Data

function getDashData() {
  dash_marker2 = parseFloat('1.0');
  dash_marker3 = parseFloat('1.0');

  var request = new XMLHttpRequest();
  request.onreadystatechange = function() {
    if (this.readyState == 4) {
      if (this.status == 200) {
        if (this.responseXML != null) {
          // Processing markers
          dash_marker1 = parseFloat('1.0');
          dash_marker2 = parseFloat('0.0');

          // Modules

          for (var i = 0; i < moduleName.length; i++) {
            try {
              var module = this.responseXML.getElementsByTagName(moduleXml[i])[0].childNodes[0].nodeValue;
            } catch (err) {
                module = '-1';
              }
            try {
              document.getElementById(moduleId[i]).innerHTML        = moduleName[i];
              document.getElementById(moduleId[i]).style.background = modulBackground(module);
              document.getElementById(moduleId[i]).style.color      = modulColor(module);
            } catch (err) { }
          }

          // Contacts

          for (var i = 0; i < contName.length; i++) {
            try {
              var dcont = this.responseXML.getElementsByTagName(contXml[i])[0].childNodes[0].nodeValue;
            } catch (err) {
                dcont = '-1';
              }
            try {
              document.getElementById(contId[i]).innerHTML        = contName[i];
              document.getElementById(contId[i]).style.background = contactBackground(dcont);
              document.getElementById(contId[i]).style.color      = contactColor(dcont);
            } catch (err) { }
          }

          // PIR's

          for (var i = 0; i < pirName.length; i++) {
            try {
              var dpir = this.responseXML.getElementsByTagName(pirXml[i])[0].childNodes[0].nodeValue;
            } catch (err) {
                dpir = '-1';
                //alert('Error' + err.name + ':' + err.message + '\n' + err.stack);
              }
            try {
              document.getElementById(pirId[i]).innerHTML        = pirName[i];
              document.getElementById(pirId[i]).style.background = pirBackground(dpir);
              document.getElementById(pirId[i]).style.color      = pirColor(dpir);
            } catch (err) { }
          }

          // Lifer

          var MAX_LIFER = 7;
          var liferStr = '';
          try {
            var lifer = this.responseXML.getElementsByTagName('lifer')[0].childNodes[0].nodeValue;
          } catch (err) {
              lifer = 1;
            }
          liferFloat = parseFloat(lifer);

          for (var i = 0; i < MAX_LIFER; i++) {
            var ch = '·';
            if (i == liferFloat) {ch = '•';}
            liferStr += ch;
          }
          try {
            document.getElementById('lifer').innerHTML = liferStr;
          } catch (err) { }


          // Time & date

          try {
            var time = this.responseXML.getElementsByTagName('time')[0].childNodes[0].nodeValue;
          } catch (err) {
            time = '-';
            }
          try {
            document.getElementById('time').innerHTML = time;
            document.getElementById('time-hide').innerHTML = time;
          } catch (err) { }

          try {
            var day = this.responseXML.getElementsByTagName('day')[0].childNodes[0].nodeValue;
          } catch (err) {
              day = '-';
            }
          try {
            document.getElementById('day').innerHTML = day;
            document.getElementById('day-hide').innerHTML = day;
          } catch (err) {

            }

          try {
            var mon = this.responseXML.getElementsByTagName('month')[0].childNodes[0].nodeValue;
          } catch (err) {
              mon = '-';
            }
          try {
            monthStr = month(mon);
            document.getElementById('month').innerHTML = monthStr;
            document.getElementById('month-hide').innerHTML = monthStr;
          } catch (err) { }

          try {
            var wday = this.responseXML.getElementsByTagName('weekday')[0].childNodes[0].nodeValue;
          } catch (err) {
              wday = '-';
            }
          try {
            weekdayStr = weekday(wday);
            document.getElementById('week-day').innerHTML      = weekdayStr;
            document.getElementById('week-day-hide').innerHTML = weekdayStr;
          } catch (err) { }

          // Temperature

          try {
            var tempSTR = this.responseXML.getElementsByTagName('temp1')[0].childNodes[0].nodeValue;
          } catch (err) {
              tempSTR = '0';
            }
          try {
            var tempSTR_ = Math.round(tempSTR, 1);
            document.getElementById('dash-temp').innerHTML      = tempSTR_;
            document.getElementById('dash-temp-hide').innerHTML = tempSTR_;
          } catch (err) { }

          // Voltage
          
          try {
            var dvolt = this.responseXML.getElementsByTagName('voltage')[0].childNodes[0].nodeValue;
          } catch (err) {
              dvolt = '0';
            }
          try {
            var dvoltage = Math.round(parseInt(dvolt), 0);
            //document.getElementById('dash-voltage').innerHTML = dvoltage;
            //document.getElementById('dash-voltage-hide').innerHTML = dvoltage;
          } catch (err) { }
          
          // Power

          try {
            var pwr = this.responseXML.getElementsByTagName('power')[0].childNodes[0].nodeValue;
          } catch (err) {
              pwr = '0';
            }
          try {
            var power = Math.round(parseInt(pwr), 0);
            document.getElementById('dash-power').innerHTML      = power;
            document.getElementById('dash-power-hide').innerHTML = power;
          } catch (err) { }

          // Uptime

          try {
            var uptm = this.responseXML.getElementsByTagName('uptime')[0].childNodes[0].nodeValue;
          } catch (err) {
              uptm = '-';
            }
          try {
            document.getElementById('dash-uptime').innerHTML = uptm;
          } catch (err) { }

          // free mem

          try {
            var freeRam = this.responseXML.getElementsByTagName('freeRAM')[0].childNodes[0].nodeValue;
          } catch (err) {
              freeRam = '-';
            }
          try {
            var freeRamPer = Math.round(freeRam / 80, 0);
            document.getElementById('dash-free-ram').innerHTML = freeRam + ' / ' + freeRamPer + '%';
          } catch (err) { }

          // CPU load

          try {
            var cyclosDelay = this.responseXML.getElementsByTagName('cycDelay')[0].childNodes[0].nodeValue;
          } catch (err) {
              cyclosDelay = '-';
            }
          try {
            var cyclosInSec = this.responseXML.getElementsByTagName('cycInSec')[0].childNodes[0].nodeValue;
          } catch (err) {
              cyclosInSec = '-';
            }

          try {load('cpu-load',  1, freeRamPer, '',           bufferCpuLoad1, '',             'mem', 'rgba(100, 160, 230, 1)', 'yellow', 'cyan');}      catch (err) { }
          insertTick(16, bufferCpuLoad3);
        //try {load('cpu-load2', 2, cyclosDelay, cyclosInSec, bufferCpuLoad2, bufferCpuLoad3, 'cpu', 'rgba(100, 160, 230, 1)', 'yellow', 'cyan');}      catch (err) { }
          try {load('cpu-load3', 3, cyclosDelay, '',          bufferCpuLoad4, '',             'cpu', 'rgba(100, 160, 230, 1)', 'cyan',   'lightblue');} catch (err) { }
          try {load('cpu-load4', 4, cyclosDelay, cyclosInSec, bufferCpuLoad5, bufferCpuLoad6, 'cpu', 'rgba(100, 160, 230, 1)', 'yellow', 'cyan');}      catch (err) { }

          // Force

          try {
            var force = this.responseXML.getElementsByTagName('force')[0].childNodes[0].nodeValue;
          } catch (err) {
              force = '-';
            }
          if (force == 1) {
            forceText = 'FRC';
          } else {
              forceText = 'NRM';
            }
          try {
            document.getElementById('force').innerHTML = forceText;
            document.getElementById('force').style.background = forceBackground(force);
            document.getElementById('force').style.color = forceColor(force);
          } catch (err) { }

          // Time provider

          try {
            var timeProv = this.responseXML.getElementsByTagName('timeProv')[0].childNodes[0].nodeValue;
          } catch (err) {
              timeProv = '-';
            }
          switch (timeProv) {
            case '0': timeProvText = 'NON'; break;
            case '1': timeProvText = 'NET'; break;
             default: timeProvText = 'RTC'; break;
          }
          try {
            document.getElementById('time-sync').innerHTML = timeProvText;
            document.getElementById('time-sync').style.background = timeProviderBackground(timeProv);
            document.getElementById('time-sync').style.color = timeProviderColor(timeProv);
          } catch (err) { }

          // Authorization

          try {
            var auth = this.responseXML.getElementsByTagName('auth')[0].childNodes[0].nodeValue;
          } catch (err) {
              auth = '-';
            }
          var authText = '';
          if (auth == 1) {
            authText = 'AUT';
          } else {
              authText = 'FRE';
            }
          try {
            document.getElementById('auth').innerHTML        = authText;
            document.getElementById('auth').style.background = authBackground(auth);
            document.getElementById('auth').style.color      = authColor(auth);
          } catch (err) { }
          try {
            document.getElementById('auth-footer').innerHTML        = authText;
            document.getElementById('auth-footer').style.background = authBackground(auth);
            document.getElementById('auth-footer').style.color      = authColor(auth);
          } catch (err) { }

          // Ports page

          if (currentPage == 'ports-page') {
            // digital pins
            for (var i = 0; i < digitalPins.length; i++) {
              try {
                var pinState = this.responseXML.getElementsByTagName(digitalPins[i])[0].childNodes[0].nodeValue;
              } catch (err) {
                  pinState = '-1';
                }
              try {
                document.getElementById(digitalPins[i]).innerHTML        = stateOnOff(pinState);
                document.getElementById(digitalPins[i]).style.background = stateOnOffBackground(pinState);
                document.getElementById(digitalPins[i]).style.color      = stateOnOffColor(pinState);
              } catch (err) { }
            }
            // analog pins
            for (var i = 0; i < analogPins.length; i++) {
              try {
                var aPinState = this.responseXML.getElementsByTagName(analogPins[i])[0].childNodes[0].nodeValue;
              } catch (err) {
                  aPinState = '-1';
                }
              try {
                document.getElementById(analogPins[i]).innerHTML = aPinState;
              } catch (err) { }
            }
          } // ports-page

          // Supply page

          if (currentPage == 'supply-page') {
            // Electro
            try {
              document.getElementById('volt').innerHTML = dvoltage;
            } catch (err) { }
            try {
              document.getElementById('p1').innerHTML = power;
            } catch (err) { }

            // Period
            try {
              var period = this.responseXML.getElementsByTagName('period')[0].childNodes[0].nodeValue;
              period -= 100;
            } catch (err) {
                period = '-';
              }
            try {
              var frec = 1 / (period / 1000000);
              document.getElementById('freq').innerHTML = frec.toFixed(2);
            } catch (err) { }

            // Graph
            try {graph('graph-voltage', 3, dvoltage,        '', bufferElectro6, '', 'pwr', 'rgba(4, 169, 174, 1)', 'rgba(204,  9,  51, 1)', 'lightblue');} catch (err) { }
            try {graph('graph-power',   3, power,           '', bufferElectro1, '', 'pwr', 'rgba(4, 169, 174, 1)', 'rgba(213,125,  12, 1)', 'lightblue');} catch (err) { }
            try {graph('graph-freq',    3, frec.toFixed(2), '', bufferElectro5, '', 'pwr', 'rgba(4, 169, 174, 1)', 'rgba( 44, 77, 220, 1)', 'lightblue');} catch (err) { }
          } // supply-page

          // Electro page

          if (currentPage == 'electro-page') {
            // Electro
            for (var i = 0; i < 14; i++) {
              try {
                ui[i] = this.responseXML.getElementsByTagName(UiId[i])[0].childNodes[0].nodeValue;
                ui[i] = Math.round(parseInt(ui[i]), 0);
              } catch (err) {
                  ui[i] = '-';
                }
              try {
                document.getElementById(UiId[i]).innerHTML = ui[i];
              } catch (err) { }

            }
            // Graphs
            try {graph('graph-test',  3, ui[1], '', bufferElectro1, '', 2, 'rgba(4, 169, 174, 1)', 'rgba(204,  9,51,  1)', 'lightblue');} catch (err) { }
            try {graph('graph-cond',  3, ui[4], '', bufferElectro2, '', 2, 'rgba(4, 169, 174, 1)', 'rgba(213,125,12,  1)', 'lightblue');} catch (err) { }
            try {graph('graph-plita', 3, ui[2], '', bufferElectro3, '', 2, 'rgba(4, 169, 174, 1)', 'rgba(166,  0,151, 1)', 'lightblue');} catch (err) { }
            try {graph('graph-str',   3, ui[3], '', bufferElectro4, '', 2, 'rgba(4, 169, 174, 1)', 'rgba( 75,152,190, 1)', 'lightblue');} catch (err) { }
            // Devices
            try {device('device-plita', 3, ui[2], 'Плита',             200, 500,  900, 1500);} catch (err) { }
            try {device('device-str',   3, ui[3], 'Стиральная машина',  60, 300, 1500, 2000);} catch (err) { }
            try {device('device-cond',  3, ui[4], 'Кондиционер',        60, 100,  600, 1500);} catch (err) { }
            // draw bar
            //drawBar(p2, p3, p4, p6, p7, p8, p9, p10, p11, p12, p13);
          } // electro-page

        } // if
      } // if
    } // if
  } // request

  dash_marker1 = parseFloat('0.0');

  request.open('GET', 'request_dash' + currentPage + randomNoCache(), true);
  request.send(null);
  setTimeout('getDashData()', 2000);
  dash_marker3 = parseFloat('0.0');
} // getDashData()

/* end */
