#include <SdFile.h>
/**  Create a file object and open it in the current working directory.
 *
 * \param[in] path A path with a valid 8.3 DOS name for a file to be opened.
 *
 * \param[in] oflag Values for \a oflag are constructed by a bitwise-inclusive
 * OR of open flags. see SdBaseFile::open(SdBaseFile*, const char*, uint8_t).
 */
SdFile::SdFile(const char* path, uint8_t oflag) : SdBaseFile(path, oflag) {
}
//-int SdFile::write(const void* buf, size_t nbyte) {
  return SdBaseFile::write(buf, nbyte);
}
size_t SdFile::write(uint8_t b) {
  return SdBaseFile::write(&b, 1) == 1 ? 1 : 0;
}
//------------------------------------------------------------------------------
/** Write a string to a file. Used by the Arduino Print class.
 * \param[in] str Pointer to the string.
 * Use getWriteError to check for errors.
 * \return count of characters written for success or -1 for failure.
 */
int SdFile::write(const char* str) {
  return SdBaseFile::write(str, strlen(str));
}
//------------------------------------------------------------------------------
/** Write a PROGMEM string to a file.
 * \param[in] str Pointer to the PROGMEM string.
 * Use getWriteError to check for errors.
 */
void SdFile::write_P(PGM_P str) {
  for (uint8_t c; (c = pgm_read_byte(str)); str++) write(c);
}
//------------------------------------------------------------------------------
/** Write a PROGMEM string followed by CR/LF to a file.
 * \param[in] str Pointer to the PROGMEM string.
 * Use getWriteError to check for errors.
 */
void SdFile::writeln_P(PGM_P str) {
  write_P(str);
  write_P(PSTR("\r\n"));
}
