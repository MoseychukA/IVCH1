/*
  Module Documentation
  part of Arduino Mega Server project
*/

/* 
Analog pins
===========
A15 - PM channel b8
A14 - PM channel b7
A13 - PM channel b6
A12 - PM channel b5
A11 - PM channel b4
A10 - PM channel b3
 A9 - PM channel b2
 A8 - PM channel b1
 A7 -
 A6 -
 A5 - PM channel a5
 A4 - PM channel a4
 A3 - PM channel a3
 A2 - PM channel a2
 A1 - PM channel a1
 A0 - PM U

GPIO
====
D53 - 
D52 - NRF24 (SCK)
D51 - NRF24 (MOSI)
D50 - NRF24 (MISO)
.
.
D31 - cont 2
D30 - 
D29 - 
D28 - 
D27 - cont 1
D26 - pir 5
D25 - pir 4
D24 - pir 3
D23 - pir 2
D22 - pir 1

D21 [SCL] - RTC
D20 [SDA] - RTC
D19 [RX1] - NOO MT1132
D18 [TX1] - NOO MT1132
D17 [RX2] - NOO MR1132
D16 [TX2] - NOO MR1132
D15 [RX3] 
D14 [TX3] 

D13 - Ethernet Shield (SCK)
D12 - Ethernet Shield (MISO)
D11 - Ethernet Shield (MOSI)
D10 - Ethernet Shield (SS for Ethernet)
 D9 - NRF24 (CSN)
 D8 - NRF24 (CE)

 D7 - LED G
 D6 - LED R
 D5 - LED B
 D4 - Ethernet Shield (SS for SD card)
 D3 ~ Key
 D2 - 1-Wire DS18B20
 D1 - [TX]
 D0 - [RX]

nRF24 packet structure
======================
01234567890123456789012345678901
.........ppppppppddddddddddddddd
        9       8             15

 buff[0] - packet type
 buff[1] - command
 buff[2] - address
 buff[3] - destination address
 buff[4] - required answer
 buff[5] - packet number
 buff[6] - reserve
 buff[7] - reserve
 buff[8] - reserve
 buff[9] - password 1
buff[10] - password 2
buff[11] - password 3
buff[12] - password 4
buff[13] - password 5
buff[14] - password 6
buff[15] - password 7
buff[16] - password 8
buff[17] - data 1
buff[18] - data 2
buff[19] - data 3
buff[20] - data 4
buff[21] - data 5
buff[22] - data 6
buff[23] - data 7
buff[24] - data 8
buff[25] - data 9
buff[26] - data 10
buff[27] - data 11
buff[28] - data 12
buff[29] - data 13
buff[30] - data 14
buff[31] - data 15

nRF24 packets type
=================
0 - empty
1 - command
2 - request
3 - data
4 - alarm
5 - answer
6 - echo

*/
