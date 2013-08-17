#include "PCFTime.h"

void PrintBCD(Print *pPort, uint8_t uValue)
{
  if (uValue < 0x10)
    pPort->print('0');
  pPort->print(uValue, HEX);
}

void PCFTime::InitalizeFrom(uint32_t uTimestamp)
{
  seconds = dec2bcd(uTimestamp % 60);
  uTimestamp = uTimestamp / 60;
  minutes = dec2bcd(uTimestamp % 60);
  uTimestamp = uTimestamp / 60; 
  hours = dec2bcd(uTimestamp % 24);
  uTimestamp = uTimestamp / 24;
  days = dec2bcd((uTimestamp % 31) + 1);
  uTimestamp = uTimestamp / 31;
  months = dec2bcd((uTimestamp % 12) + 1);
  years = dec2bcd(uTimestamp / 12);

  weekdays = 0; // assumed. the RTC data sheet says the meaning can be assigned by user.
}

void PCFTime::Dump(Print *pPort)
{
  PrintBCD(pPort, days);
  pPort->print('-');
  PrintBCD(pPort, months);
  pPort->print("-20");
  PrintBCD(pPort, years);
  pPort->print(' ');
  PrintBCD(pPort, hours);
  pPort->print(':');
  PrintBCD(pPort, minutes);
  pPort->print(':');
  PrintBCD(pPort, seconds);
}

void PCFTime::WriteDateWithHour(Print *pOutput)
{
  PrintBCD(pOutput, years);
  PrintBCD(pOutput, months);
  PrintBCD(pOutput, days);
  PrintBCD(pOutput, hours);
}

uint32_t PCFTime::ToUint32() const
{
  return bcd2dec(seconds)+
    bcd2dec(minutes)*60+
    bcd2dec(hours)*60*60+
    (bcd2dec(days) - 1)*24*60*60+
    (bcd2dec(months) - 1)*31*24*60*60+
    bcd2dec(years)*12*31*24*60*60;
}

uint32_t bcd2dec(uint8_t uBCD)
{
  return (uBCD >> 4) * 10 + (uBCD & 0xF);
}

uint8_t dec2bcd(uint8_t uDecimal)
{
  return ((uDecimal / 10) << 4) | (uDecimal % 10);
}