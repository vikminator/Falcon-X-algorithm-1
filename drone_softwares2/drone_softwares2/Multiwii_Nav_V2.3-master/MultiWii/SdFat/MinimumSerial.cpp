#include <Arduino.h>
#if defined(UDR0) || defined(DOXYGEN)
#include <MinimumSerial.h>

void MinimumSerial::begin(unsigned long baud) {
  uint16_t baud_setting;
  if (F_CPU != 16000000UL || baud != 57600) {
    UCSR0A = 1 << U2X0;
    baud_setting = (F_CPU / 4 / baud - 1) / 2;
  } else {
    UCSR0A = 0;
    baud_setting = (F_CPU / 8 / baud - 1) / 2;
  }
  
  UBRR0H = baud_setting >> 8;
  UBRR0L = baud_setting;
  // enable transmit and receive
  UCSR0B |= (1 << TXEN0) | (1 << RXEN0) ;
}

int MinimumSerial::read() {
  if (UCSR0A & (1 << RXC0)) return UDR0;
  return -1;
}

size_t MinimumSerial::write(uint8_t b) {
  while (((1 << UDRIE0) & UCSR0B) || !(UCSR0A & (1 << UDRE0))) {}
  UDR0 = b;
  return 1;
}
MinimumSerial MiniSerial;
#endif  //  defined(UDR0) || defined(DOXYGEN)