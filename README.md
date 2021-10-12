# ESP1588
IEEE-1588 Precision Time Protocol (PTP) client for ESP8266/ESP32 (Arduino Core)

Provides a globally synchronized millis() function, initially designed to facilitate a coordinated light show based on arrays of ESP8266/ESP32 light fixtures,
but could be used for anything that needs accurate time (+/- 1ms in my implementation).

Mostly complete, includes Best Master Clock algorithm.
Delay request-response is not implemented, but feel free to implement it if you need it. 
