#include "rtc.h"
#include "SPI.h"

#define RTC_00_Control1 0x00
#define RTC_01_Control2 0x01
#define RTC_02_Sec 0x02
#define RTC_03_Min 0x03
#define RTC_04_Hour 0x04
#define RTC_05_Day 0x05
#define RTC_06_Weekday 0x06
#define RTC_07_Month 0x07
#define RTC_08_Year 0x08
#define RTC_09_AlarmMin 0x09
#define RTC_0A_AlarmHour 0x0A
#define RTC_0B_AlarmDay 0x0B
#define RTC_0C_AlarmWeekday 0x0C
#define RTC_0D_Offset 0x0D
#define RTC_0E_TimerClkOut 0x0E
#define RTC_0F_TimerCountDown 0x0F

#define RTC_TIMER_CLKOUT_NONE 0x70
#define RTC_TIMER_TE_ENABLE 0x08
#define RTC_TIMER_TE_DISABLE 0x00
#define RTC_TIMER_SOURCE_4kHz 0x00
#define RTC_TIMER_SOURCE_64Hz 0x01
#define RTC_TIMER_SOURCE_1Hz 0x10
#define RTC_TIMER_SOURCE_60thHz 0x11

static rtc *s_pInterruptHandler;




/* Constructor. 
Sets the chip-select and interrupt pin used for the real time clock. 
The interrupt is determined automatically. If the interrupt pin is equal to 
rtc::NO_INTERRUPT, the interrupt sare not attached. */
rtc::rtc(uint8_t CSPin, uint8_t uInterruptPin, uint8_t uInterruptChannel) : 
  c_uChipSelectPin(CSPin), c_uInterruptPin(uInterruptPin), c_uInterruptChannel(uInterruptChannel)
{
  m_pDiagnostic = NULL;
  m_uTimerPeriod = RTC_TIMER_OFF;
}

bool rtc::Setup(CDiagnosticChannel *pDiagnostic)
{
  SPI.begin();
  SPI.setDataMode(SPI_MODE0); 
  SPI.setBitOrder(MSBFIRST);
  pinMode(c_uChipSelectPin, OUTPUT);

  // The seconds register contains a flag which indicates whether the 
  // clock is valid. If it is not valid, reset the device & restore time to zero. 
  uint8_t uSeconds;

  uSeconds = SPIRead(RTC_02_Sec);
  if (uSeconds & 0x80)
  {
    // Clock integrity is not guaranteed. Oscillator has stopped or has been 
    // interrupted. Start with a software reset to restore to a known state. 
    SPIWrite(RTC_00_Control1, 0b01011000);

    // Reset time to zero. 
    MelvynTime emptyTime;
    emptyTime.InitalizeFrom(0);
    SetTime(&emptyTime);
  }

  // Hook up interrupts. 
  if (c_uInterruptPin != NO_INTERRUPT)
  {
    pinMode(c_uInterruptPin, INPUT_PULLUP); // RTC interrupt is open drain, active low. 
    uint8_t CurrentInterruptState = SREG; // Save interrupt state. 
    cli();  // disable interrupts. 
    s_pInterruptHandler = this;
    attachInterrupt(c_uInterruptChannel, InterruptHandler, LOW);
    SPIWrite(RTC_01_Control2, 0x01); // Interrupt from countdown timer. 
    SREG = CurrentInterruptState;
  }

  m_pDiagnostic = pDiagnostic;

  //
  // Check if the RTC is present & functional. 
  //
  return Verify(pDiagnostic);
}

/*
The clock can be set to signal an interrupt periodically. Check that is operational
*/
bool rtc::Verify(CDiagnosticChannel *pDiagnostic)
{
  uint8_t uValue;

  if (c_uInterruptPin == NO_INTERRUPT)
  {
    m_bFault = false; 
  }
  else
  {
    // Enable the interrupt timer, check that the SPI write took & see if we get an interrupt.
    HadTick(); // Clear any previous tick.
    SetTimerPeriod(1);
    uValue = SPIRead(RTC_0E_TimerClkOut);

    m_bFault = false; // assume the best...
    if (uValue == (RTC_TIMER_CLKOUT_NONE | RTC_TIMER_SOURCE_64Hz | RTC_TIMER_TE_ENABLE | 0x01))
    {
      delay(20);
      StopTimer();
      if (!HadTick())
      {
        pDiagnostic->PrintLine(F("RTC: no interrupt detected"));
        m_bFault = true;
      }

    }
    else
    {
      pDiagnostic->PrintValue(F("RTC: error reading timer configuration. Got: "), uValue, true);
      m_bFault = true;
    }
  }
  return !m_bFault;
}

void rtc::ReportInitialization(CDiagnosticChannel *pChannel)
{
  Table(*pChannel, F("Real Time Clock"), m_bFault ? "Fault" : "OK");
}

void rtc::SPIWrite(uint8_t uRegister, uint8_t uValue)
{
  uint8_t CurrentInterruptState = SREG; // save interrupt state. 
  cli(); // disable interrupts. 

  digitalWrite(c_uChipSelectPin, HIGH);
  SPI.transfer((uRegister & ~0x80)|0x10); // Send the address with the write mask on
  SPI.transfer(uValue); // New value follows
  digitalWrite(c_uChipSelectPin, LOW);

  SREG = CurrentInterruptState; // restore interrupt state. 
}

uint8_t rtc::SPIRead(uint8_t uRegister)
{
  uint8_t uValue;
  uint8_t CurrentInterruptState = SREG; // save interrupt state. 
  cli(); // disable interrupts. 

  digitalWrite(c_uChipSelectPin, HIGH);
  SPI.transfer(uRegister | 0x90); // Send the address with the write mask off
  uValue = SPI.transfer(0); // The written value is ignored, reg value is read
  digitalWrite(c_uChipSelectPin, LOW);

  SREG = CurrentInterruptState; // restore interrupt state. 

  return uValue;
}

void rtc::SPIBurstWrite(uint8_t uFirstRegister, const uint8_t *puData, uint8_t uLength)
{
  uint8_t CurrentInterruptState = SREG; // save interrupt state. 
  cli(); // disable interrupts. 

  digitalWrite(c_uChipSelectPin, HIGH);
  SPI.transfer((uFirstRegister &~ 0x80)|0x10); // Send the start address with the write mask on
  while (uLength--)
    SPI.transfer(*puData++);
  digitalWrite(c_uChipSelectPin, LOW);

  SREG = CurrentInterruptState; // restore interrupt state. 
}

void rtc::SPIBurstRead(uint8_t uFirstRegister, uint8_t *puData, uint8_t uLength)
{
  uint8_t CurrentInterruptState = SREG; // save interrupt state. 
  cli(); // disable interrupts. 

  digitalWrite(c_uChipSelectPin, HIGH);
  SPI.transfer(uFirstRegister | 0x90); // Send the start address with the write mask off
  while (uLength--)
    *puData++ = SPI.transfer(0);
  digitalWrite(c_uChipSelectPin, LOW);

  SREG = CurrentInterruptState; // restore interrupt state. 
}

void rtc::SetTime(MelvynTime* melvyntime)
{
  SPIBurstWrite(RTC_02_Sec,(uint8_t*)melvyntime,sizeof(MelvynTime));
}

void rtc::GetTime(MelvynTime* melvyntime) const
{
  const_cast<rtc *>(this)->SPIBurstRead(RTC_02_Sec,(uint8_t*)melvyntime,sizeof(MelvynTime));
}

/* Get the time in seconds from the real time clock*/
uint32_t rtc::GetTime() const
{
  MelvynTime currentTime;
  memset(&currentTime,0,sizeof(currentTime));
  GetTime(&currentTime);

  return currentTime.ToUint32();
}

unsigned rtc::ProcessCommand(CMessageBuffer *pMessage)
{
  switch (pMessage->GetMessage()->Command)
  {

  case MAKE_COMMAND('ST'):
    //Update the RTC with the time sent by Megunolink
    if (pMessage->GetMessageDataSize() != sizeof(MelvynTime))
      return CMessageResponseHeader::ERR_DATA_SIZE;
    SetTime(((MelvynTime*)pMessage->GetMessageData()));
    return CMessageResponseHeader::OK;

  case MAKE_COMMAND('PT'): // Print the current time. 
    if (m_pDiagnostic != NULL)
    {
      MelvynTime Now;
      GetTime(&Now);
      Now.Dump(m_pDiagnostic);
      m_pDiagnostic->NewLine();
      return CMessageResponseHeader::OK;
    }
    else
      return CMessageResponseHeader::ERR_GENERAL;
  }
}

void rtc::SendToTable(String name, uint8_t datetime)
{
  Serial.print("{TABLE|SET|");
  Serial.print(name);
  Serial.print('|');
  Serial.print(datetime);
  Serial.print("}");
}

/* 
* Sets timeout period for RTC timer & enables the timer. 
* uTicks: Number of ticks on a 64Hz source before an interrupt signal is raised. 
* If RTC_TIMER_OFF, the timer is stopped. 
* Returns true if the timer is updated & false if there is a fault. 
*/
bool rtc::SetTimerPeriod(uint8_t uTicks)
{
  uint8_t uValue;
  uint8_t CurIntReg = SREG;

  if (m_bFault)
    return false;

  if (uTicks == RTC_TIMER_OFF)
    StopTimer();
  else
  {
    cli();

    // First disable the timer so we can update without the risk of it generating an interrupt. 
    uValue = RTC_TIMER_CLKOUT_NONE | RTC_TIMER_SOURCE_64Hz | RTC_TIMER_TE_DISABLE;
    SPIWrite(RTC_0E_TimerClkOut, uValue);

    // Update the timer tick counter
    SPIWrite(RTC_0F_TimerCountDown, uTicks);
    m_uInterruptPeriod = ((uint32_t)uTicks * 1000) / 64;

    // Enable the timer again. 
    uValue = RTC_TIMER_CLKOUT_NONE | RTC_TIMER_SOURCE_64Hz | RTC_TIMER_TE_ENABLE;
    SPIWrite(RTC_0E_TimerClkOut, uValue);

    SREG = CurIntReg;
  }

  m_uTimerPeriod = uTicks;

  return true; // todo: Maybe a more rigourous test here. 
}

void rtc::StopTimer()
{
  uint8_t uValue;
  uValue = RTC_TIMER_CLKOUT_NONE | RTC_TIMER_SOURCE_64Hz;
  SPIWrite(RTC_0E_TimerClkOut, uValue);
  m_uTimerPeriod = RTC_TIMER_OFF;
}

uint32_t rtc::GetMilliSeconds() const
{
  uint8_t CurrentInterruptState = SREG;  // Save interrupts. 
  uint32_t uResult;

  cli(); // Disable interrupts

  uResult = m_uMillisCounter;

  SREG = CurrentInterruptState; // Restore interrupts. 

  return uResult;
}

// The RTC chip provides an interrupt signal after each period specified by SetTimerPeriod. 
// We count them. 
void rtc::InterruptHandler()
{
  // Update current time. 
  s_pInterruptHandler->m_uMillisCounter += s_pInterruptHandler->m_uInterruptPeriod;

  // Clear timer interrupt flag. 
  s_pInterruptHandler->SPIWrite(RTC_01_Control2, 0x01);

  //rtc tick (so in the main we run some code once per wakeup period)
  s_pInterruptHandler->m_RTCTick = true;

}

//Replies if a period has occured (1 tick)
boolean rtc::HadTick()
{
  boolean tempRTCTick;
  tempRTCTick = m_RTCTick;
  m_RTCTick = false;
  return tempRTCTick;
}

