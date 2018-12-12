#ifndef bufstream_h
#define bufstream_h
/**
 * \file
 * \brief \ref ibufstream and \ref obufstream classes
 */
#include <iostream.h>
//==============================================================================
/**
 * \class ibufstream
 * \brief parse a char string
 */
class ibufstream : public istream {
 public:
  /** Constructor */
  ibufstream() : buf_(0), len_(0) {}
  /** Constructor
   * \param[in] str pointer to string to be parsed
   * Warning: The string will not be copied so must stay in scope.
   */
  explicit ibufstream(const char* str) {
  init(str);
  }
  /** Initialize an ibufstream
   * \param[in] str pointer to string to be parsed
   * Warning: The string will not be copied so must stay in scope.
   */
  void init(const char* str) {
    buf_ = str;
    len_ = strlen(buf_);
    pos_ = 0;
    clear();
  }

 protected:
  /// @cond SHOW_PROTECTED
  int16_t getch() {
    if (pos_ < len_) return buf_[pos_++];
    setstate(eofbit);
    return -1;
  }
  void getpos(FatPos_t *pos) {
    pos->position = pos_;
  }
  bool seekoff(off_type off, seekdir way) {return false;}
  bool seekpos(pos_type pos) {
    if (pos < len_) {
      pos_ = pos;
      return true;
    }
    return false;
  }
  void setpos(FatPos_t *pos) {
    pos_ = pos->position;
  }
  pos_type tellpos() {
    return pos_;
  }
  /// @endcond
 private:
  const char* buf_;
  size_t len_;
  size_t pos_;
};
//==============================================================================
/**
 * \class obufstream
 * \brief format a char string
 */
class obufstream : public ostream {
 public:
  /** constructor */
  obufstream() : in_(0) {}
  /** Constructor
   * \param[in] buf buffer for formatted string
   * \param[in] size buffer size
   */
  obufstream(char *buf, size_t size) {
    init(buf, size);
  }
  /** Initialize an obufstream
   * \param[in] buf buffer for formatted string
   * \param[in] size buffer size
   */
  void init(char *buf, size_t size) {
    buf_ = buf;
    buf[0] = '\0';
    size_ = size;
    in_ = 0;
  }
  /** \return a pointer to the buffer */
  char* buf() {return buf_;}
  /** \return the length of the formatted string */
  size_t length() {return in_;}

 protected:
  /// @cond SHOW_PROTECTED
  void putch(char c) {
    if (in_ >= (size_ - 1)) {
      setstate(badbit);
      return;
    }
    buf_[in_++] = c;
    buf_[in_]= '\0';
  }
  void putstr(const char *str) {
    while (*str) putch(*str++);
  }
  bool seekoff(off_type off, seekdir way) {return false;}
  bool seekpos(pos_type pos) {
    if (pos > in_) return false;
    in_ = pos;
    buf_[in_] = '\0';
    return true;
  }
  bool sync() {return true;}

  pos_type tellpos() {
    return in_;
  }
  /// @endcond
 private:
  char *buf_;
  size_t size_;
  size_t in_;
};
#endif  // bufstream_h
