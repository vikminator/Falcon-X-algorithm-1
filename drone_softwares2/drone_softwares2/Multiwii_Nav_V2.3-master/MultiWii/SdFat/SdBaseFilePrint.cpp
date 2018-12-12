#include <SdFat.h>
//------------------------------------------------------------------------------
/** List directory contents to stdOut.
 *
 * \param[in] flags The inclusive OR of
 *
 * LS_DATE - %Print file modification date
 *
 * LS_SIZE - %Print file size.
 *
 * LS_R - Recursive list of subdirectories.
 */
void SdBaseFile::ls(uint8_t flags) {
  ls(SdFat::stdOut(), flags, 0);
}
//------------------------------------------------------------------------------
/** List directory contents.
 *
 * \param[in] pr Print stream for list.
 *
 * \param[in] flags The inclusive OR of
 *
 * LS_DATE - %Print file modification date
 *
 * LS_SIZE - %Print file size.
 *
 * LS_R - Recursive list of subdirectories.
 *
 * \param[in] indent Amount of space before file name. Used for recursive
 * list to indicate subdirectory level.
 */
void SdBaseFile::ls(Print* pr, uint8_t flags, uint8_t indent) {
  rewind();
  int8_t status;
  while ((status = lsPrintNext(pr, flags, indent))) {
    if (status > 1 && (flags & LS_R)) {
      uint16_t index = curPosition()/32 - 1;
      SdBaseFile s;
      if (s.open(this, index, O_READ)) s.ls(pr, flags, indent + 2);
      seekSet(32 * (index + 1));
    }
  }
}
//------------------------------------------------------------------------------
// saves 32 bytes on stack for ls recursion
// return 0 - EOF, 1 - normal file, or 2 - directory
int8_t SdBaseFile::lsPrintNext(Print *pr, uint8_t flags, uint8_t indent) {
  dir_t dir;
  uint8_t w = 0;

  while (1) {
    if (read(&dir, sizeof(dir)) != sizeof(dir)) return 0;
    if (dir.name[0] == DIR_NAME_FREE) return 0;

    // skip deleted entry and entries for . and  ..
    if (dir.name[0] != DIR_NAME_DELETED && dir.name[0] != '.'
      && DIR_IS_FILE_OR_SUBDIR(&dir)) break;
  }
  // indent for dir level
  for (uint8_t i = 0; i < indent; i++) pr->write(' ');

  // print name
  for (uint8_t i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) {
      pr->write('.');
      w++;
    }
    pr->write(dir.name[i]);
    w++;
  }
  if (DIR_IS_SUBDIR(&dir)) {
    pr->write('/');
    w++;
  }
  if (flags & (LS_DATE | LS_SIZE)) {
    while (w++ < 14) pr->write(' ');
  }
  // print modify date/time if requested
  if (flags & LS_DATE) {
    pr->write(' ');
    printFatDate(pr, dir.lastWriteDate);
    pr->write(' ');
    printFatTime(pr, dir.lastWriteTime);
  }
  // print size if requested
  if (!DIR_IS_SUBDIR(&dir) && (flags & LS_SIZE)) {
    pr->write(' ');
    pr->print(dir.fileSize);
  }
  pr->println();
  return DIR_IS_FILE(&dir) ? 1 : 2;
}
//------------------------------------------------------------------------------
// print uint8_t with width 2
static void print2u(Print* pr, uint8_t v) {
  if (v < 10) pr->write('0');
  pr->print(v, DEC);
}
bool SdBaseFile::printCreateDateTime(Print* pr) {
  dir_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  printFatDate(pr, dir.creationDate);
  pr->write(' ');
  printFatTime(pr, dir.creationTime);
  return true;

 fail:
  return false;
}
void SdBaseFile::printFatDate(uint16_t fatDate) {
  printFatDate(SdFat::stdOut(), fatDate);
}
void SdBaseFile::printFatDate(Print* pr, uint16_t fatDate) {
  pr->print(FAT_YEAR(fatDate));
  pr->write('-');
  print2u(pr, FAT_MONTH(fatDate));
  pr->write('-');
  print2u(pr, FAT_DAY(fatDate));
}
void SdBaseFile::printFatTime(uint16_t fatTime) {
  printFatTime(SdFat::stdOut(), fatTime);
}
void SdBaseFile::printFatTime(Print* pr, uint16_t fatTime) {
  print2u(pr, FAT_HOUR(fatTime));
  pr->write(':');
  print2u(pr, FAT_MINUTE(fatTime));
  pr->write(':');
  print2u(pr, FAT_SECOND(fatTime));
}
//------------------------------------------------------------------------------
/** Template for SdBaseFile::printField() */
template <typename Type>
static int printFieldT(SdBaseFile* file, char sign, Type value, char term) {
  char buf[3*sizeof(Type) + 3];
  char* str = &buf[sizeof(buf)];

  if (term) {
    *--str = term;
    if (term == '\n') {
      *--str = '\r';
    }
  }
  do {
    Type m = value;
    value /= 10;
    *--str = '0' + m - 10*value;
  } while (value);
  if (sign) {
    *--str = sign;
  }
  return file->write(str, &buf[sizeof(buf)] - str);
}
int SdBaseFile::printField(uint16_t value, char term) {
  return printFieldT(this, 0, value, term);
}
int SdBaseFile::printField(int16_t value, char term) {
  char sign = 0;
  if (value < 0) {
    sign = '-';
    value = -value;
  }
  return printFieldT(this, sign, (uint16_t)value, term);
}
int SdBaseFile::printField(uint32_t value, char term) {
  return printFieldT(this, 0, value, term);
}
int SdBaseFile::printField(int32_t value, char term) {
  char sign = 0;
  if (value < 0) {
    sign = '-';
    value = -value;
  }
  return printFieldT(this, sign, (uint32_t)value, term);
}
bool SdBaseFile::printModifyDateTime(Print* pr) {
  dir_t dir;
  if (!dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  printFatDate(pr, dir.lastWriteDate);
  pr->write(' ');
  printFatTime(pr, dir.lastWriteTime);
  return true;

 fail:
  return false;
}
bool SdBaseFile::printName(Print* pr) {
  char name[13];
  if (!getFilename(name)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return pr->print(name) > 0;

 fail:
  return false;
}
bool SdBaseFile::printName() {
  return printName(SdFat::stdOut());
}
