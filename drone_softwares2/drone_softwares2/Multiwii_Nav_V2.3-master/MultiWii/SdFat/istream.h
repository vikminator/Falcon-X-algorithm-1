#ifndef istream_h
#define istream_h

#include <ios.h>


class istream : public virtual ios {
 public:
  istream() {}
  
  istream& operator>>(istream& (*pf)(istream& str)) {
    return pf(*this);
  }
  
  istream& operator>>(ios_base& (*pf)(ios_base& str)) {
    pf(*this);
    return *this;
  }
  
  istream& operator>>(ios& (*pf)(ios& str)) {
    pf(*this);
    return *this;
  }
 
  istream& operator>>(char *str) {
    getStr(str);
    return *this;
  }
 
  istream& operator>>(char& ch) {
    getChar(&ch);
    return *this;
  }
 
  istream& operator>>(signed char *str) {
    getStr(reinterpret_cast<char*>(str));
    return *this;
  }

  istream& operator>>(signed char& ch) {
    getChar(reinterpret_cast<char*>(&ch));
    return *this;
  }

  istream& operator>>(unsigned char *str) {
    getStr(reinterpret_cast<char*>(str));
    return *this;
  }
 
  istream& operator>>(unsigned char& ch) {
    getChar(reinterpret_cast<char*>(&ch));
    return *this;
  }

  istream& operator>>(bool& arg) {
    getBool(&arg);
    return *this;
  }

  istream &operator>>(short& arg) {  // NOLINT
    getNumber(&arg);
    return *this;
  }
 
  istream &operator>>(unsigned int& arg) {
    getNumber(&arg);
    return *this;
  }
 
  istream &operator>>(long& arg) {  // NOLINT
    getNumber(&arg);
    return *this;
  }

  istream &operator>>(unsigned long& arg) {  // NOLINT
    getNumber(&arg);
    return *this;
  }
  
  istream &operator>> (double& arg) {
    getDouble(&arg);
    return *this;
  }
 
  istream &operator>> (float& arg) {
    double v;
    getDouble(&v);
    arg = v;
    return *this;
  }
 
  istream& operator>> (void*& arg) {
    uint32_t val;
    getNumber(&val);
    arg = reinterpret_cast<void*>(val);
    return *this;
  }
 
  streamsize gcount() const {return gcount_;}
  int get();
  istream& get(char& ch);
  istream& get(char *str, streamsize n, char delim = '\n');
  istream& getline(char *str, streamsize count, char delim = '\n');
  istream& ignore(streamsize n = 1, int delim= -1);
  int peek();

  pos_type tellg() {return tellpos();}
 
  istream& seekg(pos_type pos) {
    if (!seekpos(pos)) setstate(failbit);
    return *this;
  }
 
  istream& seekg(off_type off, seekdir way) {
    if (!seekoff(off, way)) setstate(failbit);
    return *this;
  }
  void skipWhite();

 protected:
  /// @cond SHOW_PROTECTED
   /**
   * Internal - do not use
   * \return
   */
  virtual int16_t getch() = 0;
  /**
   * Internal - do not use
   * \param[out] pos
   * \return
   */
  int16_t getch(FatPos_t* pos) {
    getpos(pos);
    return getch();
  }
  /**
   * Internal - do not use
   * \param[out] pos
   */
  virtual void getpos(FatPos_t* pos) = 0;
  /**
   * Internal - do not use
   * \param[in] pos
   */
  virtual bool seekoff(off_type off, seekdir way) = 0;
  virtual bool seekpos(pos_type pos) = 0;
  virtual void setpos(FatPos_t* pos) = 0;
  virtual pos_type tellpos() = 0;

  /// @endcond
 private:
  size_t gcount_;
  void getBool(bool *b);
  void getChar(char* ch);
  bool getDouble(double* value);
  template <typename T>  void getNumber(T* value);
  bool getNumber(uint32_t posMax, uint32_t negMax, uint32_t* num);
  void getStr(char *str);
  int16_t readSkip();
};
//------------------------------------------------------------------------------
template <typename T>
void istream::getNumber(T* value) {
  uint32_t tmp;
  if ((T)-1 < 0) {
    // number is signed, max positive value
    uint32_t const m = ((uint32_t)-1) >> (33 - sizeof(T) * 8);
    // max absolute value of negative number is m + 1.
    if (getNumber(m, m + 1, &tmp)) {
      *value = (T)tmp;
    }
  } else {
    // max unsigned value for T
    uint32_t const m = (T)-1;
    if (getNumber(m, m, &tmp)) {
      *value = (T)tmp;
    }
  }
}
#endif  // istream_h
