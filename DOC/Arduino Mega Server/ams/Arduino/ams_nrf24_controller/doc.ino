/*
  Modul Documentation
  AMS nRF24 controller
  part of Arduino Mega Server project
*/

/*

Pins connections
================
D0           - RX
D1           - TX
D2     /INT0 - NRF IRQ
D3 /PWM/INT1 - ~BMP180
D4           - 1-WIRE (TEMP)
D5 /PWM      - ~CONT
D6 /PWM      - ~PIR
D7           - NRF CE
D8           - NRF CSN
D9 /PWM      - ~DHT22
D10/PWM/SS   - SPI
D11/PWM/MOSI - SPI
D12    /MISO - SPI
D13    /SCK  - SPI
D14/A0       -
D15/A1       -
D16/A2       -
D17/A3       -
D18/A4 /SDA  - I2C
D19/A5 /SCL  - I2C
   /A6       -
   /A7       -


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

packet type
===========
0 - empty
1 - command
2 - request
3 - data
4 - alarm
5 - answer
6 - echo

*/

