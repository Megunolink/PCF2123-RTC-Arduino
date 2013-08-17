#pragma once
#ifdef _MSC_VER
#include "../../BuildTools/ArduinoIntellisense.h"
#endif

#include <Arduino.h>
#include "PCFTime.h"


// Define timeout periods for the RTC timer. This assumes it is configured
// with a 64 Hz clock. 
#define RTC_TIMER_OFF 0
#define RTC_TIMER_125ms 8
#define RTC_TIMER_250ms 16
#define RTC_TIMER_375ms 24
#define RTC_TIMER_500ms 32
#define RTC_TIMER_625ms 40
#define RTC_TIMER_875ms 56
#define RTC_TIMER_1000ms 64
#define RTC_TIMER_1500ms 96
#define RTC_TIMER_2000ms 128
#define RTC_TIMER_2500ms 160
#define RTC_TIMER_3000ms 192

class rtc
{
private:
  // Chip enable is active high. 
  const uint8_t c_uChipSelectPin, c_uInterruptPin, c_uInterruptChannel;
  volatile uint32_t m_uMillisCounter; // Current millis count. Based off RTC timer. Granularity is m_uInterruptPeriod. 
  uint32_t m_uInterruptPeriod; // Number of milliseconds between each tick of the RTC timer.
  volatile boolean m_RTCTick;
  uint8_t m_uTimerPeriod;

  bool m_bFault; // True if a fault is detected. 

public:
  enum EConstatns { NO_INTERRUPT = 0xff };

  rtc(uint8_t CSPin, uint8_t uInterruptPin, uint8_t uInterruptChannel);
  bool Setup( CDiagnosticChannel *pDiagnostic );
  void ReportInitialization(CDiagnosticChannel *pChannel);
  bool HasFault() const { return m_bFault; }

  void SetTime(PCFTime* pcftime);
  void GetTime(PCFTime* pcftime) const;
  uint32_t GetTime() const;
  uint32_t GetMilliSeconds() const;

  void SendToTable(String name, uint8_t datetime);
  unsigned ProcessCommand(CMessageBuffer *pMessage);

  bool SetTimerPeriod(uint8_t uTicks);
  uint8_t GetTimerPeriod() const { return m_uTimerPeriod; }
  void StopTimer();
  boolean HadTick();

private:
  void SPIWrite(uint8_t uRegister, uint8_t uValue);
  uint8_t SPIRead(uint8_t uRegister);
  void SPIBurstWrite(uint8_t uFirstRegister, const uint8_t *puData, uint8_t uLength);
  void SPIBurstRead(uint8_t uFirstRegister, uint8_t *puData, uint8_t uLength);
  uint32_t bcd2dec(uint8_t dec) const;

  bool Verify(CDiagnosticChannel *pDiagnostic);

  static void InterruptHandler();
};

