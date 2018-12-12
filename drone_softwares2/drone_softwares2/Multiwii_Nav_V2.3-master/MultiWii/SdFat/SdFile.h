#include <SdBaseFile.h>
#ifndef SdFile_h
#define SdFile_h
//------------------------------------------------------------------------------
/**
 * \class SdFile
 * \brief SdBaseFile with Print.
 */
class SdFile : public SdBaseFile, public Print {
 public:
  SdFile() {}
  SdFile(const char* name, uint8_t oflag);
#if DESTRUCTOR_CLOSES_FILE
  ~SdFile() {}
#endif  // DESTRUCTOR_CLOSES_FILE
  /** \return value of writeError */
  bool getWriteError() {return SdBaseFile::getWriteError();}
  /** Set writeError to zero */
  void clearWriteError() {SdBaseFile::clearWriteError();}
  size_t write(uint8_t b);
  int write(const char* str);
  int write(const void* buf, size_t nbyte);
  void write_P(PGM_P str);
  void writeln_P(PGM_P str);
};
#endif  // SdFile_h
