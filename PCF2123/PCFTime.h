/* ***********************************************************************
Relay configuration.
Persisted in EEPROM. 
*********************************************************************** */
#pragma once
#ifdef _MSC_VER
#include "ArduinoIntellisense.h"
#endif

#include <Arduino.h>
#include <Memory.h>

struct PCFTime
{
  // Current time stored in BCD format. 
  uint8_t seconds;
  uint8_t minutes;
  uint8_t hours;
  uint8_t days;
  uint8_t weekdays;
  uint8_t months;
  uint8_t years;

  void InitalizeFrom(uint32_t uTimestamp);
  void Dump(Print *pOutput);
  void WriteDateWithHour(Print *pOutput);
  uint32_t ToUint32() const;

} PACKED;


uint32_t bcd2dec(uint8_t dec);
uint8_t dec2bcd(uint8_t uDecimal);