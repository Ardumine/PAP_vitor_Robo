#ifndef NeoSWSerial_h
#define NeoSWSerial_h

#include "Arduino.h"

class NeoSWSerial : public Stream
{
  NeoSWSerial( const NeoSWSerial & ); // Not allowed
  NeoSWSerial & operator =( const NeoSWSerial & ); // Not allowed

public:
  NeoSWSerial(uint8_t receivePin, uint8_t transmitPin)
    {
      rxPin = receivePin;
      txPin = transmitPin;
      _isr  = (isr_t) NULL;
    }

          void   begin(uint16_t baudRate=9600);   // initialize, set baudrate, listen
          void   listen();                        // enable RX interrupts
          void   ignore();                        // disable RX interrupts
          void   setBaudRate(uint16_t baudRate);  // 9600 [default], 19200, 38400
  virtual int    available();
  virtual int    read();
  virtual size_t write(uint8_t txChar);
 using Stream::write; // make the base class overloads visible
  virtual int    peek() { return 0; };
  virtual void   flush() {};
          void   end() { ignore(); }

  typedef void (* isr_t)( uint8_t );
  void attachInterrupt( isr_t fn );
  void detachInterrupt() { attachInterrupt( (isr_t) NULL ); };

private:
           uint8_t  rxPin, txPin;
  volatile uint8_t *rxPort;

  uint16_t _baudRate;
  isr_t    _isr;

  static void rxChar( uint8_t rx ); // buffer or dispatch one received character

  bool checkRxTime();

  static void startChar();

public:
  // visible only so the ISRs can call it...
  static void rxISR( uint8_t port_input_register );

  //#define NEOSWSERIAL_EXTERNAL_PCINT // uncomment to use your own PCINT ISRs
};
#endif
