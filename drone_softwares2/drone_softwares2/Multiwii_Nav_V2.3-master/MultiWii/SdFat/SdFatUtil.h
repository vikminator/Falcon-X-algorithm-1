#ifndef SdFatUtil_h
#define SdFatUtil_h
/**
 * \file
 * \brief Useful utility functions.
 */
#include <SdFat.h>
/** Store and print a string in flash memory.*/
#define PgmPrint(x) SerialPrint_P(PSTR(x))
/** Store and print a string in flash memory followed by a CR/LF.*/
#define PgmPrintln(x) SerialPrintln_P(PSTR(x))

namespace SdFatUtil {
  int FreeRam();
  void print_P(Print* pr, PGM_P str);
  void println_P(Print* pr, PGM_P str);
  void SerialPrint_P(PGM_P str);
  void SerialPrintln_P(PGM_P str);
}
using namespace SdFatUtil;  // NOLINT
#endif  // #define SdFatUtil_h
