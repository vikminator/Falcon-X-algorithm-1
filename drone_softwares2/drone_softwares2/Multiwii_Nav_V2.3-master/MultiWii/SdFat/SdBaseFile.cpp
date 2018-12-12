#include <SdFat.h>
// macro for debug
#define DBG_FAIL_MACRO  //  Serial.print(__FILE__);Serial.println(__LINE__)
//------------------------------------------------------------------------------
// pointer to cwd directory
SdBaseFile* SdBaseFile::cwd_ = 0;
// callback function for date/time
void (*SdBaseFile::dateTime_)(uint16_t* date, uint16_t* time) = 0;
//------------------------------------------------------------------------------
// add a cluster to a file
bool SdBaseFile::addCluster() {
  if (!vol_->allocContiguous(1, &curCluster_)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // if first cluster of file link to directory entry
  if (firstCluster_ == 0) {
    firstCluster_ = curCluster_;
    flags_ |= F_FILE_DIR_DIRTY;
  }
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// Add a cluster to a directory file and zero the cluster.
// return with first block of cluster in the cache
cache_t* SdBaseFile::addDirCluster() {
  uint32_t block;
  cache_t* pc;
  // max folder size
  if (fileSize_/sizeof(dir_t) >= 0XFFFF) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!addCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  block = vol_->clusterStartBlock(curCluster_);
  pc = vol_->cacheFetch(block, SdVolume::CACHE_RESERVE_FOR_WRITE);
  if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  memset(pc, 0, 512);
  // zero rest of clusters
  for (uint8_t i = 1; i < vol_->blocksPerCluster_; i++) {
    if (!vol_->writeBlock(block + i, pc->data)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // Increase directory file size by cluster size
  fileSize_ += 512UL*vol_->blocksPerCluster_;
  return pc;

 fail:
  return 0;
}
//------------------------------------------------------------------------------
// cache a file's directory entry
// return pointer to cached entry or null for failure
dir_t* SdBaseFile::cacheDirEntry(uint8_t action) {
  cache_t* pc;
  pc = vol_->cacheFetch(dirBlock_, action);
  if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return pc->dir + dirIndex_;

 fail:
  return 0;
}
bool SdBaseFile::close() {
  bool rtn = sync();
  type_ = FAT_FILE_TYPE_CLOSED;
  return rtn;
}
bool SdBaseFile::contiguousRange(uint32_t* bgnBlock, uint32_t* endBlock) {
  // error if no blocks
  if (firstCluster_ == 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  for (uint32_t c = firstCluster_; ; c++) {
    uint32_t next;
    if (!vol_->fatGet(c, &next)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // check for contiguous
    if (next != (c + 1)) {
      // error if not end of chain
      if (!vol_->isEOC(next)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      *bgnBlock = vol_->clusterStartBlock(firstCluster_);
      *endBlock = vol_->clusterStartBlock(c)
                  + vol_->blocksPerCluster_ - 1;
      return true;
    }
  }

 fail:
  return false;
}
bool SdBaseFile::createContiguous(SdBaseFile* dirFile,
        const char* path, uint32_t size) {
  uint32_t count;
  // don't allow zero length file
  if (size == 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (!open(dirFile, path, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // calculate number of clusters needed
  count = ((size - 1) >> (vol_->clusterSizeShift_ + 9)) + 1;

  // allocate clusters
  if (!vol_->allocContiguous(count, &firstCluster_)) {
    remove();
    DBG_FAIL_MACRO;
    goto fail;
  }
  fileSize_ = size;

  // insure sync() will update dir entry
  flags_ |= F_FILE_DIR_DIRTY;

  return sync();

 fail:
  return false;
}
bool SdBaseFile::dirEntry(dir_t* dir) {
  dir_t* p;
  // make sure fields on SD are correct
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read entry
  p = cacheDirEntry(SdVolume::CACHE_FOR_READ);
  if (!p) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy to caller's struct
  memcpy(dir, p, sizeof(dir_t));
  return true;

 fail:
  return false;
}
void SdBaseFile::dirName(const dir_t& dir, char* name) {
  uint8_t j = 0;
  for (uint8_t i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) name[j++] = '.';
    name[j++] = dir.name[i];
  }
  name[j] = 0;
}
bool SdBaseFile::exists(const char* name) {
  SdBaseFile file;
  return file.open(this, name, O_READ);
}
int16_t SdBaseFile::fgets(char* str, int16_t num, char* delim) {
  char ch;
  int16_t n = 0;
  int16_t r = -1;
  while ((n + 1) < num && (r = read(&ch, 1)) == 1) {
    // delete CR
    if (ch == '\r') continue;
    str[n++] = ch;
    if (!delim) {
      if (ch == '\n') break;
    } else {
      if (strchr(delim, ch)) break;
    }
  }
  if (r < 0) {
    // read error
    return -1;
  }
  str[n] = '\0';
  return n;
}
bool SdBaseFile::getFilename(char* name) {
  dir_t* p;
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (isRoot()) {
    name[0] = '/';
    name[1] = '\0';
    return true;
  }
  // cache entry
  p = cacheDirEntry(SdVolume::CACHE_FOR_READ);
  if (!p) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // format name
  dirName(*p, name);
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
void SdBaseFile::getpos(FatPos_t* pos) {
  pos->position = curPosition_;
  pos->cluster = curCluster_;
}
void SdBaseFile::ls(uint8_t flags) {
  ls(SdFat::stdOut(), flags, 0);
}
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
// format directory name field from a 8.3 name string
bool SdBaseFile::make83Name(const char* str, uint8_t* name, const char** ptr) {
  uint8_t c;
  uint8_t n = 7;  // max index for part before dot
  uint8_t i = 0;
  // blank fill name and extension
  while (i < 11) name[i++] = ' ';
  i = 0;
  while (*str != '\0' && *str != '/') {
    c = *str++;
    if (c == '.') {
      if (n == 10) {
        // only one dot allowed
        DBG_FAIL_MACRO;
        goto fail;
      }
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension
    } else {
      // illegal FAT characters
#ifdef __AVR__
      // store chars in flash
      PGM_P p = PSTR("|<>^+=?/[];,*\"\\");
      uint8_t b;
      while ((b = pgm_read_byte(p++))) if (b == c) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#else  // __AVR__
      // store chars in RAM
      if (strchr("|<>^+=?/[];,*\"\\", c)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
#endif  // __AVR__

      // check size and only allow ASCII printable characters
      if (i > n || c < 0X21 || c > 0X7E) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // only upper case allowed in 8.3 names - convert lower to upper
      name[i++] = c < 'a' || c > 'z' ?  c : c + ('A' - 'a');
    }
  }
  *ptr = str;
  // must have a file name, extension is optional
  return name[0] != ' ';

 fail:
  return false;
}
bool SdBaseFile::mkdir(SdBaseFile* parent, const char* path, bool pFlag) {
  uint8_t dname[11];
  SdBaseFile dir1, dir2;
  SdBaseFile* sub = &dir1;
  SdBaseFile* start = parent;

  if (!parent || isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (*path == '/') {
    while (*path == '/') path++;
    if (!parent->isRoot()) {
      if (!dir2.openRoot(parent->vol_)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      parent = &dir2;
    }
  }
  while (1) {
    if (!make83Name(path, dname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    while (*path == '/') path++;
    if (!*path) break;
    if (!sub->open(parent, dname, O_READ)) {
      if (!pFlag || !sub->mkdir(parent, dname)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    if (parent != start) parent->close();
    parent = sub;
    sub = parent != &dir1 ? &dir1 : &dir2;
  }
  return mkdir(parent, dname);

 fail:
  return false;
}
//------------------------------------------------------------------------------
bool SdBaseFile::mkdir(SdBaseFile* parent, const uint8_t dname[11]) {
  uint32_t block;
  dir_t d;
  dir_t* p;
  cache_t* pc;

  if (!parent->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // create a normal file
  if (!open(parent, dname, O_CREAT | O_EXCL | O_RDWR)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // convert file to directory
  flags_ = O_READ;
  type_ = FAT_FILE_TYPE_SUBDIR;

  // allocate and zero first cluster
  if (!addDirCluster()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // force entry to SD
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // cache entry - should already be in cache due to sync() call
  p = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!p) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // change directory entry  attribute
  p->attributes = DIR_ATT_DIRECTORY;

  // make entry for '.'
  memcpy(&d, p, sizeof(d));
  d.name[0] = '.';
  for (uint8_t i = 1; i < 11; i++) d.name[i] = ' ';

  // cache block for '.'  and '..'
  block = vol_->clusterStartBlock(firstCluster_);
  pc = vol_->cacheFetch(block, SdVolume::CACHE_FOR_WRITE);
  if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy '.' to block
  memcpy(&pc->dir[0], &d, sizeof(d));
  // make entry for '..'
  d.name[1] = '.';
  if (parent->isRoot()) {
    d.firstClusterLow = 0;
    d.firstClusterHigh = 0;
  } else {
    d.firstClusterLow = parent->firstCluster_ & 0XFFFF;
    d.firstClusterHigh = parent->firstCluster_ >> 16;
  }
  // copy '..' to block
  memcpy(&pc->dir[1], &d, sizeof(d));
  // write first block
  return vol_->cacheSync();

 fail:
  return false;
}
  bool SdBaseFile::open(const char* path, uint8_t oflag) {
    return open(cwd_, path, oflag);
  }
bool SdBaseFile::open(SdBaseFile* dirFile, const char* path, uint8_t oflag) {
  uint8_t dname[11];
  SdBaseFile dir1, dir2;
  SdBaseFile *parent = dirFile;
  SdBaseFile *sub = &dir1;

  if (!dirFile) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (*path == '/') {
    while (*path == '/') path++;
    if (!dirFile->isRoot()) {
      if (!dir2.openRoot(dirFile->vol_)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      parent = &dir2;
    }
  }
  while (1) {
    if (!make83Name(path, dname, &path)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    while (*path == '/') path++;
    if (!*path) break;
    if (!sub->open(parent, dname, O_READ)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (parent != dirFile) parent->close();
    parent = sub;
    sub = parent != &dir1 ? &dir1 : &dir2;
  }
  return open(parent, dname, oflag);

 fail:
  return false;
}
//------------------------------------------------------------------------------
// open with filename in dname
bool SdBaseFile::open(SdBaseFile* dirFile,
  const uint8_t dname[11], uint8_t oflag) {
  cache_t* pc;
  bool emptyFound = false;
  bool fileFound = false;
  uint8_t index;
  dir_t* p;

  vol_ = dirFile->vol_;

  dirFile->rewind();
  // search for file

  while (dirFile->curPosition_ < dirFile->fileSize_) {
    index = 0XF & (dirFile->curPosition_ >> 5);
    p = dirFile->readDirCache();
    if (!p) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (p->name[0] == DIR_NAME_FREE || p->name[0] == DIR_NAME_DELETED) {
      // remember first empty slot
      if (!emptyFound) {
        dirBlock_ = vol_->cacheBlockNumber();
        dirIndex_ = index;
        emptyFound = true;
      }
      // done if no entries follow
      if (p->name[0] == DIR_NAME_FREE) break;
    } else if (!memcmp(dname, p->name, 11)) {
      fileFound = true;
      break;
    }
  }
  if (fileFound) {
    // don't open existing file if O_EXCL
    if (oflag & O_EXCL) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // don't create unless O_CREAT and O_WRITE
    if (!(oflag & O_CREAT) || !(oflag & O_WRITE)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (emptyFound) {
      index = dirIndex_;
      p = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
      if (!p) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      if (dirFile->type_ == FAT_FILE_TYPE_ROOT_FIXED) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // add and zero cluster for dirFile - first cluster is in cache for write
      pc = dirFile->addDirCluster();
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // use first entry in cluster
      p = pc->dir;
      index = 0;
    }
    // initialize as empty file
    memset(p, 0, sizeof(dir_t));
    memcpy(p->name, dname, 11);

    // set timestamps
    if (dateTime_) {
      // call user date/time function
      dateTime_(&p->creationDate, &p->creationTime);
    } else {
      // use default date/time
      p->creationDate = FAT_DEFAULT_DATE;
      p->creationTime = FAT_DEFAULT_TIME;
    }
    p->lastAccessDate = p->creationDate;
    p->lastWriteDate = p->creationDate;
    p->lastWriteTime = p->creationTime;

    // write entry to SD
    if (!dirFile->vol_->cacheSync()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // open entry in cache
  return openCachedEntry(index, oflag);

 fail:
  return false;
}
bool SdBaseFile::open(SdBaseFile* dirFile, uint16_t index, uint8_t oflag) {
  dir_t* p;

  vol_ = dirFile->vol_;

  // error if already open
  if (isOpen() || !dirFile) {
    DBG_FAIL_MACRO;
    goto fail;
  }

  // don't open existing file if O_EXCL - user call error
  if (oflag & O_EXCL) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to location of entry
  if (!dirFile->seekSet(32 * index)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read entry into cache
  p = dirFile->readDirCache();
  if (!p) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if empty slot or '.' or '..'
  if (p->name[0] == DIR_NAME_FREE ||
      p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // open cached entry
  return openCachedEntry(index & 0XF, oflag);

 fail:
  return false;
}
//------------------------------------------------------------------------------
// open a cached directory entry. Assumes vol_ is initialized
bool SdBaseFile::openCachedEntry(uint8_t dirIndex, uint8_t oflag) {
  // location of entry in cache
  dir_t* p = &vol_->cacheAddress()->dir[dirIndex];

  // write or truncate is an error for a directory or read-only file
  if (p->attributes & (DIR_ATT_READ_ONLY | DIR_ATT_DIRECTORY)) {
    if (oflag & (O_WRITE | O_TRUNC)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // remember location of directory entry on SD
  dirBlock_ = vol_->cacheBlockNumber();
  dirIndex_ = dirIndex;

  // copy first cluster number for directory fields
  firstCluster_ = (uint32_t)p->firstClusterHigh << 16;
  firstCluster_ |= p->firstClusterLow;

  // make sure it is a normal file or subdirectory
  if (DIR_IS_FILE(p)) {
    fileSize_ = p->fileSize;
    type_ = FAT_FILE_TYPE_NORMAL;
  } else if (DIR_IS_SUBDIR(p)) {
    if (!setDirSize()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    type_ = FAT_FILE_TYPE_SUBDIR;
  } else {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save open flags for read/write
  flags_ = oflag & F_OFLAG;

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;
  if ((oflag & O_TRUNC) && !truncate(0)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return oflag & O_AT_END ? seekEnd(0) : true;

 fail:
  type_ = FAT_FILE_TYPE_CLOSED;
  return false;
}
bool SdBaseFile::openNext(SdBaseFile* dirFile, uint8_t oflag) {
  dir_t* p;
  uint8_t index;

  if (!dirFile) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  vol_ = dirFile->vol_;

  while (1) {
    index = 0XF & (dirFile->curPosition_ >> 5);

    // read entry into cache
    p = dirFile->readDirCache();
    if (!p) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if last entry
    if (p->name[0] == DIR_NAME_FREE) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // skip empty slot or '.' or '..'
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') {
      continue;
    }
    // must be file or dir
    if (DIR_IS_FILE_OR_SUBDIR(p)) {
      return openCachedEntry(index, oflag);
    }
  }

 fail:
  return false;
}
bool SdBaseFile::openParent(SdBaseFile* dir) {
  dir_t entry;
  dir_t* p;
  SdBaseFile file;
  uint32_t c;
  uint32_t cluster;
  uint32_t lbn;
  cache_t* pc;
  // error if already open or dir is root or dir is not a directory
  if (isOpen() || !dir || dir->isRoot() || !dir->isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  vol_ = dir->vol_;
  // position to '..'
  if (!dir->seekSet(32)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read '..' entry
  if (dir->read(&entry, sizeof(entry)) != 32) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // verify it is '..'
  if (entry.name[0] != '.' || entry.name[1] != '.') {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // start cluster for '..'
  cluster = entry.firstClusterLow;
  cluster |= (uint32_t)entry.firstClusterHigh << 16;
  if (cluster == 0) return openRoot(vol_);
  // start block for '..'
  lbn = vol_->clusterStartBlock(cluster);
  // first block of parent dir
    pc = vol_->cacheFetch(lbn, SdVolume::CACHE_FOR_READ);
    if (!pc) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  p = &pc->dir[1];
  // verify name for '../..'
  if (p->name[0] != '.' || p->name[1] != '.') {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // '..' is pointer to first cluster of parent. open '../..' to find parent
  if (p->firstClusterHigh == 0 && p->firstClusterLow == 0) {
    if (!file.openRoot(dir->volume())) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    if (!file.openCachedEntry(1, O_READ)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // search for parent in '../..'
  do {
    if (file.readDir(&entry) != 32) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    c = entry.firstClusterLow;
    c |= (uint32_t)entry.firstClusterHigh << 16;
  } while (c != cluster);
  // open parent
  return open(&file, file.curPosition()/32 - 1, O_READ);

 fail:
  return false;
}
bool SdBaseFile::openRoot(SdVolume* vol) {
  // error if file is already open
  if (isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  vol_ = vol;
  if (vol->fatType() == 16 || (FAT12_SUPPORT && vol->fatType() == 12)) {
    type_ = FAT_FILE_TYPE_ROOT_FIXED;
    firstCluster_ = 0;
    fileSize_ = 32 * vol->rootDirEntryCount();
  } else if (vol->fatType() == 32) {
    type_ = FAT_FILE_TYPE_ROOT32;
    firstCluster_ = vol->rootDirStart();
    if (!setDirSize()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } else {
    // volume is not initialized, invalid, or FAT12 without support
    DBG_FAIL_MACRO;
    goto fail;
  }
  // read only
  flags_ = O_READ;

  // set to start of file
  curCluster_ = 0;
  curPosition_ = 0;

  // root has no directory entry
  dirBlock_ = 0;
  dirIndex_ = 0;
  return true;

 fail:
  return false;
}
int SdBaseFile::peek() {
  FatPos_t pos;
  getpos(&pos);
  int c = read();
  if (c >= 0) setpos(&pos);
  return c;
}
void SdBaseFile::printDirName(const dir_t& dir,
  uint8_t width, bool printSlash) {
  printDirName(SdFat::stdOut(), dir, width, printSlash);
}
void SdBaseFile::printDirName(Print* pr, const dir_t& dir,
  uint8_t width, bool printSlash) {
  uint8_t w = 0;
  for (uint8_t i = 0; i < 11; i++) {
    if (dir.name[i] == ' ')continue;
    if (i == 8) {
      pr->write('.');
      w++;
    }
    pr->write(dir.name[i]);
    w++;
  }
  if (DIR_IS_SUBDIR(&dir) && printSlash) {
    pr->write('/');
    w++;
  }
  while (w < width) {
    pr->write(' ');
    w++;
  }
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
int16_t SdBaseFile::read() {
  uint8_t b;
  return read(&b, 1) == 1 ? b : -1;
}
int SdBaseFile::read(void* buf, size_t nbyte) {
  uint8_t blockOfCluster;
  uint8_t* dst = reinterpret_cast<uint8_t*>(buf);
  uint16_t offset;
  size_t toRead;
  uint32_t block;  // raw device block number
  cache_t* pc;

  // error if not open or write only
  if (!isOpen() || !(flags_ & O_READ)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // max bytes left in file
  if (nbyte >= (fileSize_ - curPosition_)) {
    nbyte = fileSize_ - curPosition_;
  }
  // amount left to read
  toRead = nbyte;
  while (toRead > 0) {
    size_t n;
    offset = curPosition_ & 0X1FF;  // offset in block
    blockOfCluster = vol_->blockOfCluster(curPosition_);
    if (type_ == FAT_FILE_TYPE_ROOT_FIXED) {
      block = vol_->rootDirStart() + (curPosition_ >> 9);
    } else {
      if (offset == 0 && blockOfCluster == 0) {
        // start of new cluster
        if (curPosition_ == 0) {
          // use first cluster in file
          curCluster_ = firstCluster_;
        } else {
          // get next cluster from FAT
          if (!vol_->fatGet(curCluster_, &curCluster_)) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        }
      }
      block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;
    }
    if (offset != 0 || toRead < 512 || block == vol_->cacheBlockNumber()) {
      // amount to be read from current block
      n = 512 - offset;
      if (n > toRead) n = toRead;
      // read block to cache and copy data to caller
      pc = vol_->cacheFetch(block, SdVolume::CACHE_FOR_READ);
      if (!pc) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      uint8_t* src = pc->data + offset;
      memcpy(dst, src, n);
    } else if (!USE_MULTI_BLOCK_SD_IO || toRead < 1024) {
      // read single block
      n = 512;
      if (!vol_->readBlock(block, dst)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      uint8_t nb = toRead >> 9;
      if (type_ != FAT_FILE_TYPE_ROOT_FIXED) {
        uint8_t mb = vol_->blocksPerCluster() - blockOfCluster;
        if (mb < nb) nb = mb;
      }
      n = 512*nb;
      if (vol_->cacheBlockNumber() <= block
        && block < (vol_->cacheBlockNumber() + nb)) {
        // flush cache if a block is in the cache
        if (!vol_->cacheSync()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
      if (!vol_->sdCard()->readStart(block)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      for (uint8_t b = 0; b < nb; b++) {
        if (!vol_->sdCard()->readData(dst + b*512)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
      if (!vol_->sdCard()->readStop()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    dst += n;
    curPosition_ += n;
    toRead -= n;
  }
  return nbyte;

 fail:
  return -1;
}
int8_t SdBaseFile::readDir(dir_t* dir) {
  int16_t n;
  // if not a directory file or miss-positioned return an error
  if (!isDir() || (0X1F & curPosition_)) return -1;

  while (1) {
    n = read(dir, sizeof(dir_t));
    if (n != sizeof(dir_t)) return n == 0 ? 0 : -1;
    // last entry if DIR_NAME_FREE
    if (dir->name[0] == DIR_NAME_FREE) return 0;
    // skip empty entries and entry for .  and ..
    if (dir->name[0] == DIR_NAME_DELETED || dir->name[0] == '.') continue;
    // return if normal file or subdirectory
    if (DIR_IS_FILE_OR_SUBDIR(dir)) return n;
  }
}
//------------------------------------------------------------------------------
// Read next directory entry into the cache
// Assumes file is correctly positioned
dir_t* SdBaseFile::readDirCache() {
  uint8_t i;
  // error if not directory
  if (!isDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // index of entry in cache
  i = (curPosition_ >> 5) & 0XF;

  // use read to locate and cache block
  if (read() < 0) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // advance to next entry
  curPosition_ += 31;

  // return pointer to entry
  return vol_->cacheAddress()->dir + i;

 fail:
  return 0;
}
bool SdBaseFile::remove() {
  dir_t* d;
  // free any clusters - will fail if read-only or directory
  if (!truncate(0)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // cache directory entry
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // mark entry deleted
  d->name[0] = DIR_NAME_DELETED;

  // set this file closed
  type_ = FAT_FILE_TYPE_CLOSED;

  // write entry to SD
  return vol_->cacheSync();
  return true;

 fail:
  return false;
}
bool SdBaseFile::remove(SdBaseFile* dirFile, const char* path) {
  SdBaseFile file;
  if (!file.open(dirFile, path, O_WRITE)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  return file.remove();

 fail:
  return false;
}
bool SdBaseFile::rename(SdBaseFile* dirFile, const char* newPath) {
  dir_t entry;
  uint32_t dirCluster = 0;
  SdBaseFile file;
  cache_t* pc;
  dir_t* d;

  // must be an open file or subdirectory
  if (!(isFile() || isSubDir())) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // can't move file
  if (vol_ != dirFile->vol_) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // sync() and cache directory entry
  sync();
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // save directory entry
  memcpy(&entry, d, sizeof(entry));

  // mark entry deleted
  d->name[0] = DIR_NAME_DELETED;

  // make directory entry for new path
  if (isFile()) {
    if (!file.open(dirFile, newPath, O_CREAT | O_EXCL | O_WRITE)) {
      goto restore;
    }
  } else {
    // don't create missing path prefix components
    if (!file.mkdir(dirFile, newPath, false)) {
      goto restore;
    }
    // save cluster containing new dot dot
    dirCluster = file.firstCluster_;
  }
  // change to new directory entry
  dirBlock_ = file.dirBlock_;
  dirIndex_ = file.dirIndex_;

  // mark closed to avoid possible destructor close call
  file.type_ = FAT_FILE_TYPE_CLOSED;

  // cache new directory entry
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy all but name field to new directory entry
  memcpy(&d->attributes, &entry.attributes, sizeof(entry) - sizeof(d->name));

  if (dirCluster) {
    // get new dot dot
    uint32_t block = vol_->clusterStartBlock(dirCluster);
    pc = vol_->cacheFetch(block, SdVolume::CACHE_FOR_READ);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&entry, &pc->dir[1], sizeof(entry));

    // free unused cluster
    if (!vol_->freeChain(dirCluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // store new dot dot
    block = vol_->clusterStartBlock(firstCluster_);
    pc = vol_->cacheFetch(block, SdVolume::CACHE_FOR_WRITE);
    if (!pc) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    memcpy(&pc->dir[1], &entry, sizeof(entry));
  }
  return vol_->cacheSync();

 restore:
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // restore entry
  d->name[0] = entry.name[0];
  vol_->cacheSync();

 fail:
  return false;
}
bool SdBaseFile::rmdir() {
  // must be open subdirectory
  if (!isSubDir()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  rewind();

  // make sure directory is empty
  while (curPosition_ < fileSize_) {
    dir_t* p = readDirCache();
    if (!p) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last used entry
    if (p->name[0] == DIR_NAME_FREE) break;
    // skip empty slot, '.' or '..'
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') continue;
    // error not empty
    if (DIR_IS_FILE_OR_SUBDIR(p)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  // convert empty directory to normal file for remove
  type_ = FAT_FILE_TYPE_NORMAL;
  flags_ |= O_WRITE;
  return remove();

 fail:
  return false;
}
bool SdBaseFile::rmRfStar() {
  uint16_t index;
  SdBaseFile f;
  rewind();
  while (curPosition_ < fileSize_) {
    // remember position
    index = curPosition_/32;

    dir_t* p = readDirCache();
    if (!p) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // done if past last entry
    if (p->name[0] == DIR_NAME_FREE) break;

    // skip empty slot or '.' or '..'
    if (p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') continue;

    // skip if part of long file name or volume label in root
    if (!DIR_IS_FILE_OR_SUBDIR(p)) continue;

    if (!f.open(this, index, O_READ)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (f.isSubDir()) {
      // recursively delete
      if (!f.rmRfStar()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      // ignore read-only
      f.flags_ |= O_WRITE;
      if (!f.remove()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    // position to next entry if required
    if (curPosition_ != (32UL*(index + 1))) {
      if (!seekSet(32UL*(index + 1))) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  // don't try to delete root
  if (!isRoot()) {
    if (!rmdir()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  return true;

 fail:
  return false;
}
SdBaseFile::SdBaseFile(const char* path, uint8_t oflag) {
  type_ = FAT_FILE_TYPE_CLOSED;
  writeError = false;
  open(path, oflag);
}
bool SdBaseFile::seekSet(uint32_t pos) {
  uint32_t nCur;
  uint32_t nNew;
  // error if file not open or seek past end of file
  if (!isOpen() || pos > fileSize_) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (type_ == FAT_FILE_TYPE_ROOT_FIXED) {
    curPosition_ = pos;
    goto done;
  }
  if (pos == 0) {
    // set position to start of file
    curCluster_ = 0;
    curPosition_ = 0;
    goto done;
  }
  // calculate cluster index for cur and new position
  nCur = (curPosition_ - 1) >> (vol_->clusterSizeShift_ + 9);
  nNew = (pos - 1) >> (vol_->clusterSizeShift_ + 9);

  if (nNew < nCur || curPosition_ == 0) {
    // must follow chain from first cluster
    curCluster_ = firstCluster_;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (!vol_->fatGet(curCluster_, &curCluster_)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  curPosition_ = pos;

 done:
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
// set fileSize_ for a directory
bool SdBaseFile::setDirSize() {
  uint16_t s = 0;
  uint32_t cluster = firstCluster_;
  do {
    if (!vol_->fatGet(cluster, &cluster)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    s += vol_->blocksPerCluster();
    // max size if a directory file is 4096 blocks
    if (s >= 4096) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  } while (!vol_->isEOC(cluster));
  fileSize_ = 512L*s;
  return true;

 fail:
  return false;
}
//------------------------------------------------------------------------------
void SdBaseFile::setpos(FatPos_t* pos) {
  curPosition_ = pos->position;
  curCluster_ = pos->cluster;
}
bool SdBaseFile::sync() {
  // only allow open files and directories
  if (!isOpen()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (flags_ & F_FILE_DIR_DIRTY) {
    dir_t* d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
    // check for deleted by another open file object
    if (!d || d->name[0] == DIR_NAME_DELETED) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    // do not set filesize for dir files
    if (!isDir()) d->fileSize = fileSize_;

    // update first cluster fields
    d->firstClusterLow = firstCluster_ & 0XFFFF;
    d->firstClusterHigh = firstCluster_ >> 16;

    // set modify time if user supplied a callback date/time function
    if (dateTime_) {
      dateTime_(&d->lastWriteDate, &d->lastWriteTime);
      d->lastAccessDate = d->lastWriteDate;
    }
    // clear directory dirty
    flags_ &= ~F_FILE_DIR_DIRTY;
  }
  return vol_->cacheSync();

 fail:
  writeError = true;
  return false;
}
bool SdBaseFile::timestamp(SdBaseFile* file) {
  dir_t* d;
  dir_t dir;

  // get timestamps
  if (!file->dirEntry(&dir)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // update directory fields
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // copy timestamps
  d->lastAccessDate = dir.lastAccessDate;
  d->creationDate = dir.creationDate;
  d->creationTime = dir.creationTime;
  d->creationTimeTenths = dir.creationTimeTenths;
  d->lastWriteDate = dir.lastWriteDate;
  d->lastWriteTime = dir.lastWriteTime;

  // write back entry
  return vol_->cacheSync();

 fail:
  return false;
}
bool SdBaseFile::timestamp(uint8_t flags, uint16_t year, uint8_t month,
         uint8_t day, uint8_t hour, uint8_t minute, uint8_t second) {
  uint16_t dirDate;
  uint16_t dirTime;
  dir_t* d;

  if (!isOpen()
    || year < 1980
    || year > 2107
    || month < 1
    || month > 12
    || day < 1
    || day > 31
    || hour > 23
    || minute > 59
    || second > 59) {
      DBG_FAIL_MACRO;
      goto fail;
  }
  // update directory entry
  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  d = cacheDirEntry(SdVolume::CACHE_FOR_WRITE);
  if (!d) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  dirDate = FAT_DATE(year, month, day);
  dirTime = FAT_TIME(hour, minute, second);
  if (flags & T_ACCESS) {
    d->lastAccessDate = dirDate;
  }
  if (flags & T_CREATE) {
    d->creationDate = dirDate;
    d->creationTime = dirTime;
    // seems to be units of 1/100 second not 1/10 as Microsoft states
    d->creationTimeTenths = second & 1 ? 100 : 0;
  }
  if (flags & T_WRITE) {
    d->lastWriteDate = dirDate;
    d->lastWriteTime = dirTime;
  }
  return vol_->cacheSync();

 fail:
  return false;
}
bool SdBaseFile::truncate(uint32_t length) {
  uint32_t newPos;
  // error if not a normal file or read-only
  if (!isFile() || !(flags_ & O_WRITE)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // error if length is greater than current size
  if (length > fileSize_) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // fileSize and length are zero - nothing to do
  if (fileSize_ == 0) return true;

  // remember position for seek after truncation
  newPos = curPosition_ > length ? length : curPosition_;

  // position to last cluster in truncated file
  if (!seekSet(length)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  if (length == 0) {
    // free all clusters
    if (!vol_->freeChain(firstCluster_)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    firstCluster_ = 0;
  } else {
    uint32_t toFree;
    if (!vol_->fatGet(curCluster_, &toFree)) {
      DBG_FAIL_MACRO;
      goto fail;
    }
    if (!vol_->isEOC(toFree)) {
      // free extra clusters
      if (!vol_->freeChain(toFree)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      // current cluster is end of chain
      if (!vol_->fatPutEOC(curCluster_)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
  }
  fileSize_ = length;

  // need to update directory entry
  flags_ |= F_FILE_DIR_DIRTY;

  if (!sync()) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // set file to correct position
  return seekSet(newPos);

 fail:
  return false;
}
int SdBaseFile::write(const void* buf, size_t nbyte) {
  // convert void* to uint8_t*  -  must be before goto statements
  const uint8_t* src = reinterpret_cast<const uint8_t*>(buf);
  cache_t* pc;
  uint8_t cacheOption;
  // number of bytes left to write  -  must be before goto statements
  size_t nToWrite = nbyte;
  size_t n;
  // error if not a normal file or is read-only
  if (!isFile() || !(flags_ & O_WRITE)) {
    DBG_FAIL_MACRO;
    goto fail;
  }
  // seek to end of file if append flag
  if ((flags_ & O_APPEND) && curPosition_ != fileSize_) {
    if (!seekEnd()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  while (nToWrite) {
    uint8_t blockOfCluster = vol_->blockOfCluster(curPosition_);
    uint16_t blockOffset = curPosition_ & 0X1FF;
    if (blockOfCluster == 0 && blockOffset == 0) {
      // start of new cluster
      if (curCluster_ != 0) {
        uint32_t next;
        if (!vol_->fatGet(curCluster_, &next)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
        if (vol_->isEOC(next)) {
          // add cluster if at end of chain
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        } else {
          curCluster_ = next;
        }
      } else {
        if (firstCluster_ == 0) {
          // allocate first cluster of file
          if (!addCluster()) {
            DBG_FAIL_MACRO;
            goto fail;
          }
        } else {
          curCluster_ = firstCluster_;
        }
      }
    }
    // block for data write
    uint32_t block = vol_->clusterStartBlock(curCluster_) + blockOfCluster;

    if (blockOffset != 0 || nToWrite < 512) {
      // partial block - must use cache
      // max space in block
      n = 512 - blockOffset;
      // lesser of space and amount to write
      if (n > nToWrite) n = nToWrite;

      if (blockOffset == 0 && curPosition_ >= fileSize_) {
        // start of new block don't need to read into cache
        cacheOption = SdVolume::CACHE_RESERVE_FOR_WRITE;
      } else {
        // rewrite part of block
        cacheOption = SdVolume::CACHE_FOR_WRITE;
        }
        pc = vol_->cacheFetch(block, cacheOption);
        if (!pc) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      uint8_t* dst = pc->data + blockOffset;
      memcpy(dst, src, n);
      if (512 == (n + blockOffset)) {
        if (!vol_->cacheWriteData()) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
    } else if (!USE_MULTI_BLOCK_SD_IO || nToWrite < 1024) {
      // use single block write command
      n = 512;
      if (vol_->cacheBlockNumber() == block) {
        vol_->cacheInvalidate();
      }
      if (!vol_->writeBlock(block, src)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    } else {
      // use multiple block write command
      uint8_t maxBlocks = vol_->blocksPerCluster() - blockOfCluster;
      uint8_t nBlock = nToWrite >> 9;
      if (nBlock > maxBlocks) nBlock = maxBlocks;

      n = 512*nBlock;
      if (!vol_->sdCard()->writeStart(block, nBlock)) {
        DBG_FAIL_MACRO;
        goto fail;
      }
      for (uint8_t b = 0; b < nBlock; b++) {
        // invalidate cache if block is in cache
        if ((block + b) == vol_->cacheBlockNumber()) {
          vol_->cacheInvalidate();
        }
        if (!vol_->sdCard()->writeData(src + 512*b)) {
          DBG_FAIL_MACRO;
          goto fail;
        }
      }
      if (!vol_->sdCard()->writeStop()) {
        DBG_FAIL_MACRO;
        goto fail;
      }
    }
    curPosition_ += n;
    src += n;
    nToWrite -= n;
  }
  if (curPosition_ > fileSize_) {
    // update fileSize and insure sync will update dir entry
    fileSize_ = curPosition_;
    flags_ |= F_FILE_DIR_DIRTY;
  } else if (dateTime_ && nbyte) {
    // insure sync will update modified date and time
    flags_ |= F_FILE_DIR_DIRTY;
  }

  if (flags_ & O_SYNC) {
    if (!sync()) {
      DBG_FAIL_MACRO;
      goto fail;
    }
  }
  return nbyte;

 fail:
  // return for write error
  writeError = true;
  return -1;
}
//------------------------------------------------------------------------------
// suppress cpplint warnings with NOLINT comment
#if ALLOW_DEPRECATED_FUNCTIONS && !defined(DOXYGEN)
void (*SdBaseFile::oldDateTime_)(uint16_t& date, uint16_t& time) = 0;  // NOLINT
#endif  // ALLOW_DEPRECATED_FUNCTIONS
