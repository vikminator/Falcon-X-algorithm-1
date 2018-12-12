#include <SdFat.h>
#ifndef PSTR
#define PSTR(x) x
#define PGM_P const char*
#endif
//------------------------------------------------------------------------------
#if USE_SERIAL_FOR_STD_OUT || !defined(UDR0)
Print* SdFat::stdOut_ = &Serial;
#else  // USE_SERIAL_FOR_STD_OUT
#include <MinimumSerial.h>
Print* SdFat::stdOut_ = &MiniSerial;
#endif  // USE_SERIAL_FOR_STD_OUT
//------------------------------------------------------------------------------
static void pstrPrint(PGM_P str) {
  for (uint8_t c; (c = pgm_read_byte(str)); str++) SdFat::stdOut()->write(c);
}
//------------------------------------------------------------------------------
static void pstrPrintln(PGM_P str) {
  pstrPrint(str);
  SdFat::stdOut()->println();
}
bool SdFat::begin(uint8_t chipSelectPin, uint8_t sckRateID) {
  return card_.init(sckRateID, chipSelectPin) && vol_.init(&card_) && chdir(1);
}
bool SdFat::chdir(bool set_cwd) {
  if (set_cwd) SdBaseFile::cwd_ = &vwd_;
  if (vwd_.isOpen()) vwd_.close();
  return vwd_.openRoot(&vol_);
}
bool SdFat::chdir(const char *path, bool set_cwd) {
  SdBaseFile dir;
  if (path[0] == '/' && path[1] == '\0') return chdir(set_cwd);
  if (!dir.open(&vwd_, path, O_READ)) goto fail;
  if (!dir.isDir()) goto fail;
  vwd_ = dir;
  if (set_cwd) SdBaseFile::cwd_ = &vwd_;
  return true;

 fail:
  return false;
}
void SdFat::chvol() {
  SdBaseFile::cwd_ = &vwd_;
}
void SdFat::errorHalt() {
  errorPrint();
  while (1);
}
void SdFat::errorHalt(char const* msg) {
  errorPrint(msg);
  while (1);
}
void SdFat::errorHalt_P(PGM_P msg) {
  errorPrint_P(msg);
  while (1);
}
void SdFat::errorPrint() {
  if (!card_.errorCode()) return;
  pstrPrint(PSTR("SD errorCode: 0X"));
  stdOut_->print(card_.errorCode(), HEX);
  pstrPrint(PSTR(",0X"));
  stdOut_->println(card_.errorData(), HEX);
}
void SdFat::errorPrint(char const* msg) {
  pstrPrint(PSTR("error: "));
  stdOut_->println(msg);
  errorPrint();
}
void SdFat::errorPrint_P(PGM_P msg) {
  pstrPrint(PSTR("error: "));
  pstrPrintln(msg);
  errorPrint();
}
bool SdFat::exists(const char* name) {
  return vwd_.exists(name);
}
void SdFat::initErrorHalt() {
  initErrorPrint();
  while (1);
}
void SdFat::initErrorHalt(char const *msg) {
  stdOut_->println(msg);
  initErrorHalt();
}
void SdFat::initErrorHalt_P(PGM_P msg) {
  pstrPrintln(msg);
  initErrorHalt();
}
void SdFat::initErrorPrint() {
  if (card_.errorCode()) {
    pstrPrintln(PSTR("Can't access SD card. Do not reformat."));
    if (card_.errorCode() == SD_CARD_ERROR_CMD0) {
      pstrPrintln(PSTR("No card, wrong chip select pin, or SPI problem?"));
    }
    errorPrint();
  } else if (vol_.fatType() == 0) {
    pstrPrintln(PSTR("Invalid format, reformat SD."));
  } else if (!vwd_.isOpen()) {
    pstrPrintln(PSTR("Can't open root directory."));
  } else {
    pstrPrintln(PSTR("No error found."));
  }
}
//------------------------------------------------------------------------------
/**Print message and error details and halt after SdFat::init() fails.
 *
 * \param[in] msg Message to print.
 */
void SdFat::initErrorPrint(char const *msg) {
  stdOut_->println(msg);
  initErrorPrint();
}
void SdFat::initErrorPrint_P(PGM_P msg) {
  pstrPrintln(msg);
  initErrorHalt();
}
void SdFat::ls(uint8_t flags) {
  vwd_.ls(stdOut_, flags);
}
void SdFat::ls(Print* pr, uint8_t flags) {
  vwd_.ls(pr, flags);
}
bool SdFat::mkdir(const char* path, bool pFlag) {
  SdBaseFile sub;
  return sub.mkdir(&vwd_, path, pFlag);
}
bool SdFat::remove(const char* path) {
  return SdBaseFile::remove(&vwd_, path);
}
bool SdFat::rename(const char *oldPath, const char *newPath) {
  SdBaseFile file;
  if (!file.open(oldPath, O_READ)) return false;
  return file.rename(&vwd_, newPath);
}
bool SdFat::rmdir(const char* path) {
  SdBaseFile sub;
  if (!sub.open(path, O_READ)) return false;
  return sub.rmdir();
}
bool SdFat::truncate(const char* path, uint32_t length) {
  SdBaseFile file;
  if (!file.open(path, O_WRITE)) return false;
  return file.truncate(length);
}
