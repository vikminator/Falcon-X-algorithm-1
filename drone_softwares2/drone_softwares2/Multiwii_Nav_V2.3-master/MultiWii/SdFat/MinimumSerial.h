#ifndef MinimumSerial_h
#define MinimumSerial_h
/**
 * \class MinimumSerial
 * \brief mini serial class for the %SdFat library.
 */
class MinimumSerial : public Print {
 public:
  void begin(unsigned long);
  int read();
  size_t write(uint8_t b);
  using Print::write;
};
#ifdef UDR0
extern MinimumSerial MiniSerial;
#endif  // UDR0
#endif  // MinimumSerial_h
