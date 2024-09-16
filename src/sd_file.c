#include <stdlib.h>
#include <string.h>
#include "sd_file.h"
#include "sd_driver.h"

static int16_t _read(sd_file* pfile, void* buf, uint16_t nbyte);
static uint8_t _cacheFlush(sd_file* pfile);

uint8_t openRoot(sd_file* pfile, sd_volume* vol) {
    // error if file is already open
    if (isOpen(pfile)) {
        return false;
    }

    if (fatType(vol) == 16) {
        pfile->type_ = FAT_FILE_TYPE_ROOT16;
        pfile->firstCluster_ = 0;
        pfile->fileSize_ = 32 * rootDirEntryCount(vol);
    } else if (fatType(vol) == 32) {
        pfile->type_ = FAT_FILE_TYPE_ROOT32;
        pfile->firstCluster_ = rootDirStart(vol);
    if (!chainSize(vol, pfile->firstCluster_, &pfile->fileSize_)) {
        return false;
    }
    } else {
        // volume is not initialized or FAT12
        return false;
    }
    pfile->vol_ = vol;
    // read only
    pfile->flags_ = O_READ;

    // set to start of file
    pfile->curCluster_ = 0;
    pfile->curPosition_ = 0;

    // root has no directory entry
    pfile->dirBlock_ = 0;
    pfile->dirIndex_ = 0;
    return true;
}

uint8_t isOpen(sd_file* pfile) {
    return pfile->type_ != FAT_FILE_TYPE_CLOSED;
}

uint8_t fat_open(sd_file* dirFile, uint16_t index, uint8_t oflag) {
  // error if already open
  if (isOpen(dirFile)) {
    return false;
  }

  // don't open existing file if O_CREAT and O_EXCL - user call error
  if ((oflag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
    return false;
  }

  dirFile->vol_ = dirFile->vol_;

  // seek to location of entry
  if (!fat_seekSet(dirFile, 32 * index)) {
    return false;
  }

  // read entry into cache
  dir_t* p = readDirCache(dirFile);
  if (p == NULL) {
    return false;
  }

  // error if empty slot or '.' or '..'
  if (p->name[0] == DIR_NAME_FREE ||
      p->name[0] == DIR_NAME_DELETED || p->name[0] == '.') {
    return false;
  }
  // open cached entry
  return fat_openCachedEntry(dirFile, index & 0XF, oflag);
}

dir_t* readDirCache(sd_file* dirFile) {
    // error if not directory
    if (!isDir(dirFile)) {
    return NULL;
    }

    // index of entry in cache
    uint8_t i = (dirFile->curPosition_ >> 5) & 0XF;

    // use read to locate and cache block
    if (sd_read(dirFile) < 0) {
        return NULL;
    }

    // advance to next entry
    dirFile->curPosition_ += 31;
    
    // return pointer to entry
    return (dirFile->vol_->cacheBuffer_.dir + i);
}

uint8_t fat_seekSet(sd_file* pfile, uint32_t pos) {
  // error if file not open or seek past end of file
  if (!isOpen(pfile) || pos > pfile->fileSize_) {
    return false;
  }

  if (pfile->type_ == FAT_FILE_TYPE_ROOT16) {
    pfile->curPosition_ = pos;
    return true;
  }
  if (pos == 0) {
    // set position to start of file
    pfile->curCluster_ = 0;
    pfile->curPosition_ = 0;
    return true;
  }
  // calculate cluster index for cur and new position
  uint32_t nCur = (pfile->curPosition_ - 1) >> (pfile->vol_->clusterSizeShift_ + 9);
  uint32_t nNew = (pos - 1) >> (pfile->vol_->clusterSizeShift_ + 9);

  if (nNew < nCur || pfile->curPosition_ == 0) {
    // must follow chain from first cluster
    pfile->curCluster_ = pfile->firstCluster_;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (!fatGet(pfile->vol_, pfile->curCluster_, &pfile->curCluster_)) {
      return false;
    }
  }
  pfile->curPosition_ = pos;
  return true;
}

uint8_t fat_openCachedEntry(sd_file* pfile, uint8_t dirIndex, uint8_t oflag) {
    // location of entry in cache
    dir_t* p = pfile->vol_->cacheBuffer_.dir + dirIndex;

    // write or truncate is an error for a directory or read-only file
    if (p->attributes & (DIR_ATT_READ_ONLY | DIR_ATT_DIRECTORY)) {
    if (oflag & (O_WRITE | O_TRUNC)) {
        return false;
    }
    }
    // remember location of directory entry on SD
    pfile->dirIndex_ = dirIndex;
    pfile->dirBlock_ = pfile->vol_->cacheBlockNumber_;

    // copy first cluster number for directory fields
    pfile->firstCluster_ = (uint32_t)p->firstClusterHigh << 16;
    pfile->firstCluster_ |= p->firstClusterLow;

    // make sure it is a normal file or subdirectory
    if (DIR_IS_FILE(p)) {
    pfile->fileSize_ = p->fileSize;
    pfile->type_ = FAT_FILE_TYPE_NORMAL;
    } else if (DIR_IS_SUBDIR(p)) {
    if (!chainSize(pfile->vol_, pfile->firstCluster_, &pfile->fileSize_)) {
        return false;
    }
    pfile->type_ = FAT_FILE_TYPE_SUBDIR;
    } else {
    return false;
    }
    // save open flags for read/write
    pfile->flags_ = oflag & (O_ACCMODE | O_SYNC | O_APPEND);

    // set to start of file
    pfile->curCluster_ = 0;
    pfile->curPosition_ = 0;

    // truncate file to zero length if requested
    if (oflag & O_TRUNC) {
    return truncate(pfile, 0);
    }
    return true;
}

uint8_t isDir(sd_file* pfile) {
    return pfile->type_ >= FAT_FILE_TYPE_MIN_DIR;
}

int16_t sd_read(sd_file* pfile) {
    uint8_t b;
    return _read(pfile, &b, 1) == 1 ? b : -1;
}

// free a cluster chain
uint8_t freeChain(sd_file* pfile, uint32_t cluster) {
  // clear free cluster location
//   pfile->allocSearchStart_ = 2;

  do {
    uint32_t next;
    if (!fatGet(pfile->vol_, cluster, &next)) {
      return false;
    }

    // free cluster
    if (!fatPut(pfile->vol_, cluster, 0)) {
      return false;
    }

    cluster = next;
  } while (!isEOC(pfile->vol_, cluster));

  return true;
}

uint8_t truncate(sd_file* pfile, uint32_t length) {
  // error if not a normal file or read-only
  if (!isFile(pfile) || !(pfile->flags_ & O_WRITE)) {
    return false;
  }

  // error if length is greater than current size
  if (length > pfile->fileSize_) {
    return false;
  }

  // fileSize and length are zero - nothing to do
  if (pfile->fileSize_ == 0) {
    return true;
  }

  // remember position for seek after truncation
  uint32_t newPos = pfile->curPosition_ > length ? length : pfile->curPosition_;

  // position to last cluster in truncated file
  if (!seekSet(pfile, length)) {
    return false;
  }

  if (length == 0) {
    // free all clusters
    if (!freeChain(pfile, pfile->firstCluster_)) {
      return false;
    }
    pfile->firstCluster_ = 0;
  } else {
    uint32_t toFree;
    if (!fatGet(pfile->vol_, pfile->curCluster_, &toFree)) {
      return false;
    }

    if (!isEOC(pfile->vol_, toFree)) {
      // free extra clusters
      if (!freeChain(pfile, toFree)) {
        return false;
      }

      // current cluster is end of chain
      if (!fatPutEOC(pfile->vol_, pfile->curCluster_)) {
        return false;
      }
    }
  }
  pfile->fileSize_ = length;

  // need to update directory entry
  pfile->flags_ |= F_FILE_DIR_DIRTY;

  if (!sync(pfile, false)) {
    return false;
  }

  // set file to correct position
  return seekSet(pfile, newPos);
}

uint8_t isFile(sd_file* pfile) {
    return pfile->type_ == FAT_FILE_TYPE_NORMAL;
}

uint8_t seekSet(sd_file* pfile, uint32_t pos) {
  // error if file not open or seek past end of file
  if (!isOpen(pfile) || pos > pfile->fileSize_) {
    return false;
  }

  if (pfile->type_ == FAT_FILE_TYPE_ROOT16) {
    pfile->curPosition_ = pos;
    return true;
  }
  if (pos == 0) {
    // set position to start of file
    pfile->curCluster_ = 0;
    pfile->curPosition_ = 0;
    return true;
  }
  // calculate cluster index for cur and new position
  uint32_t nCur = (pfile->curPosition_ - 1) >> (pfile->vol_->clusterSizeShift_ + 9);
  uint32_t nNew = (pos - 1) >> (pfile->vol_->clusterSizeShift_ + 9);

  if (nNew < nCur || pfile->curPosition_ == 0) {
    // must follow chain from first cluster
    pfile->curCluster_ = pfile->firstCluster_;
  } else {
    // advance from curPosition
    nNew -= nCur;
  }
  while (nNew--) {
    if (!fatGet(pfile->vol_, pfile->curCluster_, &pfile->curCluster_)) {
      return false;
    }
  }
  pfile->curPosition_ = pos;
  return true;
}

uint8_t sync(sd_file* pfile, uint8_t blocking) {
  // only allow open files and directories
  if (!isOpen(pfile)) {
    return false;
  }

  if (pfile->flags_ & F_FILE_DIR_DIRTY) {
    dir_t* d = cacheDirEntry(pfile, CACHE_FOR_WRITE);
    if (!d) {
      return false;
    }

    // do not set filesize for dir files
    if (!isDir(pfile)) {
      d->fileSize = pfile->fileSize_;
    }

    // update first cluster fields
    d->firstClusterLow = pfile->firstCluster_ & 0XFFFF;
    d->firstClusterHigh = pfile->firstCluster_ >> 16;

    // set modify time if user supplied a callback date/time function
    // if (pfile->dateTime_) {
    //     uint16_t *pdate = (uint16_t*)&d->lastWriteDate;
    //   pfile->dateTime_(pdate, (uint16_t*)&d->lastWriteTime);
    //   d->lastAccessDate = d->lastWriteDate;
    // }
    // clear directory dirty
    pfile->flags_ &= ~F_FILE_DIR_DIRTY;
  }

  if (!blocking) {
    pfile->flags_ &= ~F_FILE_NON_BLOCKING_WRITE;
  }

  return cacheFlush(pfile->vol_, blocking);
}

// void oldToNew(uint16_t* date, uint16_t* time) {
//     uint16_t d;
//     uint16_t t;
//     oldDateTime_(d, t);
//     *date = d;
//     *time = t;
// }

void dateTimeCallback(
    sd_file* pfile,
    void (*dateTime)(uint16_t* date, uint16_t* time)) {  // NOLINT
    pfile->dateTime_ = dateTime;
}

dir_t* cacheDirEntry(sd_file* pfile, uint8_t action) {
  if (!cacheRawBlock(pfile->vol_, pfile->dirBlock_, action)) {
    return NULL;
  }
  return (dir_t*)(pfile->vol_->cacheBuffer_.dir + pfile->dirIndex_);
}


static int16_t _read(sd_file* pfile, void* buf, uint16_t nbyte) {
  uint8_t* dst = (uint8_t*)(buf);

  // error if not open or write only
  if (!isOpen(pfile) || !(pfile->flags_ & O_READ)) {
    return -1;
  }

  // max bytes left in file
  if (nbyte > (pfile->fileSize_ - pfile->curPosition_)) {
    nbyte = pfile->fileSize_ - pfile->curPosition_;
  }

  // amount left to read
  uint16_t toRead = nbyte;
  while (toRead > 0) {
    uint32_t block;  // raw device block number
    uint16_t offset = pfile->curPosition_ & 0X1FF;  // offset in block
    if (pfile->type_ == FAT_FILE_TYPE_ROOT16) {
      block = rootDirStart(pfile->vol_) + (pfile->curPosition_ >> 9);
    } else {
      uint8_t _blockOfCluster = blockOfCluster(pfile, pfile->curPosition_);
      if (offset == 0 && _blockOfCluster == 0) {
        // start of new cluster
        if (pfile->curPosition_ == 0) {
          // use first cluster in file
          pfile->curCluster_ = pfile->firstCluster_;
        } else {
          // get next cluster from FAT
          if (!fatGet(pfile->vol_, pfile->curCluster_, &pfile->curCluster_)) {
            return -1;
          }
        }
      }
      block = clusterStartBlock(pfile, pfile->curCluster_) + _blockOfCluster;
    }
    uint16_t n = toRead;

    // amount to be read from current block
    if (n > (512 - offset)) {
      n = 512 - offset;
    }

    // no buffering needed if n == 512 or user requests no buffering
    if ((unbufferedRead(pfile) || n == 512) &&
        block != pfile->vol_->cacheBlockNumber_) {
      if (!readData(block, offset, n, dst)) {
        return -1;
      }
      dst += n;
    } else {
      // read block to cache and copy data to caller
      if (!cacheRawBlock(pfile->vol_, block, CACHE_FOR_READ)) {
        return -1;
      }
      uint8_t* src = pfile->vol_->cacheBuffer_.data + offset;
      uint8_t* end = src + n;
      while (src != end) {
        *dst++ = *src++;
      }
    }
    pfile->curPosition_ += n;
    toRead -= n;
  }
  return nbyte;
}

uint8_t blockOfCluster(sd_file* pfile, uint32_t position) {
    return (position >> 9) & (pfile->vol_->blocksPerCluster_ - 1);
}

uint32_t clusterStartBlock(sd_file* pfile, uint32_t cluster) {
    return pfile->vol_-> dataStartBlock_ + ((cluster - 2) << pfile->vol_->clusterSizeShift_);
}

void clearUnbufferedRead(sd_file* pfile) {
    pfile->flags_ &= ~F_FILE_UNBUFFERED_READ;
}

uint8_t unbufferedRead(sd_file* pfile) {
    return pfile->flags_ & F_FILE_UNBUFFERED_READ;
}

uint8_t sd_open(sd_file* dirFile, sd_file* pfile, const char* fileName, uint8_t oflag) {
  uint8_t dname[11];
  dir_t* p;

  // error if already open
  if (isOpen(pfile)) {
    return false;
  }

  if (!make83Name(fileName, dname)) {
    return false;
  }
  pfile->vol_ = dirFile->vol_;
  rewind(pfile);

  // bool for empty entry found
  uint8_t emptyFound = false;

  // search for file
  while (dirFile->curPosition_ < dirFile->fileSize_) {
    uint8_t index = 0XF & (dirFile->curPosition_ >> 5);
    p = readDirCache(dirFile);
    if (p == NULL) {
      return false;
    }

    if (p->name[0] == DIR_NAME_FREE || p->name[0] == DIR_NAME_DELETED) {
      // remember first empty slot
      if (!emptyFound) {
        emptyFound = true;
        pfile->dirIndex_ = index;
        pfile->dirBlock_ = dirFile->vol_->cacheBlockNumber_;
      }
      // done if no entries follow
      if (p->name[0] == DIR_NAME_FREE) {
        break;
      }
    } else if (!memcmp(dname, p->name, 11)) {
      // don't open existing file if O_CREAT and O_EXCL
      if ((oflag & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL)) {
        return false;
      }

      // open found file
      return openCachedEntry(pfile, 0XF & index, oflag);
    }
  }
  // only create file if O_CREAT and O_WRITE
  if ((oflag & (O_CREAT | O_WRITE)) != (O_CREAT | O_WRITE)) {
    return false;
  }

  // cache found slot or add cluster if end of file
  if (emptyFound) {
    p = cacheDirEntry(pfile, CACHE_FOR_WRITE);
    if (!p) {
      return false;
    }
  } else {
    if (dirFile->type_ == FAT_FILE_TYPE_ROOT16) {
      return false;
    }

    // add and zero cluster for dirFile - first cluster is in cache for write
    if (!addDirCluster(pfile)) {
      return false;
    }

    // use first entry in cluster
    dirFile->dirIndex_ = 0;
    p = dirFile->vol_->cacheBuffer_.dir;
  }
  // initialize as empty file
  memset(p, 0, sizeof(dir_t));
  memcpy(p->name, dname, 11);

  // set timestamps
//   if (dateTime_) {
//     // call user function
//     dateTime_(&p->creationDate, &p->creationTime);
//   } else {
//     // use default date/time
//     p->creationDate = FAT_DEFAULT_DATE;
//     p->creationTime = FAT_DEFAULT_TIME;
//   }
  p->lastAccessDate = p->creationDate;
  p->lastWriteDate = p->creationDate;
  p->lastWriteTime = p->creationTime;

  // force write of entry to SD
//   if (!SdVolume::cacheFlush()) {
//     return false;
//   }

  // open entry in cache
  return openCachedEntry(dirFile, dirFile->dirIndex_, oflag);
}

uint8_t openCachedEntry(sd_file* dirFile, uint8_t dirIndex, uint8_t oflag) {
  // location of entry in cache
  dir_t* p = dirFile->vol_->cacheBuffer_.dir + dirIndex;

  // write or truncate is an error for a directory or read-only file
  if (p->attributes & (DIR_ATT_READ_ONLY | DIR_ATT_DIRECTORY)) {
    if (oflag & (O_WRITE | O_TRUNC)) {
      return false;
    }
  }
  // remember location of directory entry on SD
  dirFile->dirIndex_ = dirIndex;
  dirFile->dirBlock_ = dirFile->vol_->cacheBlockNumber_;

  // copy first cluster number for directory fields
  dirFile->firstCluster_ = (uint32_t)p->firstClusterHigh << 16;
  dirFile->firstCluster_ |= p->firstClusterLow;

  // make sure it is a normal file or subdirectory
  if (DIR_IS_FILE(p)) {
    dirFile->fileSize_ = p->fileSize;
    dirFile->type_ = FAT_FILE_TYPE_NORMAL;
  } else if (DIR_IS_SUBDIR(p)) {
    if (!chainSize(dirFile->vol_, dirFile->firstCluster_, &dirFile->fileSize_)) {
      return false;
    }
    dirFile->type_ = FAT_FILE_TYPE_SUBDIR;
  } else {
    return false;
  }
  // save open flags for read/write
  dirFile->flags_ = oflag & (O_ACCMODE | O_SYNC | O_APPEND);

  // set to start of file
  dirFile->curCluster_ = 0;
  dirFile->curPosition_ = 0;

  // truncate file to zero length if requested
  if (oflag & O_TRUNC) {
    return truncate(dirFile, 0);
  }
  return true;
}

uint8_t make83Name(const char* str, uint8_t* name) {
  uint8_t c;
  uint8_t n = 7;  // max index for part before dot
  uint8_t i = 0;
  // blank fill name and extension
  while (i < 11) {
    name[i++] = ' ';
  }
  i = 0;
  while ((c = *str++) != '\0') {
    if (c == '.') {
      if (n == 10) {
        return false;  // only one dot allowed
      }
      n = 10;  // max index for full 8.3 name
      i = 8;   // place for extension
    } else {
      // illegal FAT characters
      uint8_t b;
      #if defined(__AVR__)
      PGM_P p = PSTR("|<>^+=?/[];,*\"\\");
      while ((b = pgm_read_byte(p++))) if (b == c) {
          return false;
        }
      #elif defined(__arm__)
      const uint8_t valid[] = "|<>^+=?/[];,*\"\\";
      const uint8_t *p = valid;
      while ((b = *p++)) if (b == c) {
          return false;
        }
      #endif
      // check size and only allow ASCII printable characters
      if (i > n || c < 0X21 || c > 0X7E) {
        return false;
      }
      // only upper case allowed in 8.3 names - convert lower to upper
      name[i++] = c < 'a' || c > 'z' ?  c : c + ('A' - 'a');
    }
  }
  // must have a file name, extension is optional
  return name[0] != ' ';
}

void rewind(sd_file* pfile) {
    pfile->curPosition_ = pfile->curCluster_ = 0;
}

uint8_t addDirCluster(sd_file* pfile) {
  if (!addCluster(pfile)) {
    return false;
  }

  // zero data in cluster insure first cluster is in cache
  uint32_t block = clusterStartBlock(pfile, pfile->curCluster_);
  for (uint8_t i = pfile->vol_->blocksPerCluster_; i != 0; i--) {
    if (!cacheZeroBlock(pfile, block + i - 1)) {
      return false;
    }
  }
  // Increase directory file size by cluster size
  pfile->fileSize_ += 512UL << pfile->vol_->clusterSizeShift_;
  return true;
}

uint8_t addCluster(sd_file* pfile) {
  if (!allocContiguous(pfile, 1, &pfile->curCluster_)) {
    return false;
  }

  // if first cluster of file link to directory entry
  if (pfile->firstCluster_ == 0) {
    pfile->firstCluster_ = pfile->curCluster_;
    pfile->flags_ |= F_FILE_DIR_DIRTY;
  }
  pfile->flags_ |= F_FILE_CLUSTER_ADDED;
  return true;
}

uint8_t allocContiguous(sd_file* pfile, uint32_t count, uint32_t* curCluster) {
  // start of group
  uint32_t bgnCluster;

  // flag to save place to start next search
  uint8_t setStart;

  // set search start cluster
  if (*curCluster) {
    // try to make file contiguous
    bgnCluster = *curCluster + 1;

    // don't save new start location
    setStart = false;
  } else {
    // start at likely place for free cluster
    bgnCluster = pfile->allocSearchStart_;

    // save next search start if one cluster
    setStart = 1 == count;
  }
  // end of group
  uint32_t endCluster = bgnCluster;

  // last cluster of FAT
  uint32_t fatEnd = pfile->vol_->clusterCount_ + 1;

  // search the FAT for free clusters
  for (uint32_t n = 0;; n++, endCluster++) {
    // can't find space checked all clusters
    if (n >= pfile->vol_->clusterCount_) {
      return false;
    }

    // past end - start from beginning of FAT
    if (endCluster > fatEnd) {
      bgnCluster = endCluster = 2;
    }
    uint32_t f;
    if (!fatGet(pfile->vol_, endCluster, &f)) {
      return false;
    }

    if (f != 0) {
      // cluster in use try next cluster as bgnCluster
      bgnCluster = endCluster + 1;
    } else if ((endCluster - bgnCluster + 1) == count) {
      // done - found space
      break;
    }
  }
  // mark end of chain
  if (!fatPutEOC(pfile->vol_, endCluster)) {
    return false;
  }

  // link clusters
  while (endCluster > bgnCluster) {
    if (!fatPut(pfile->vol_, endCluster - 1, endCluster)) {
      return false;
    }
    endCluster--;
  }
  if (*curCluster != 0) {
    // connect chains
    if (!fatPut(pfile->vol_, *curCluster, bgnCluster)) {
      return false;
    }
  }
  // return first cluster number to caller
  *curCluster = bgnCluster;

  // remember possible next free cluster
  if (setStart) {
    pfile->allocSearchStart_ = bgnCluster + 1;
  }

  return true;
}

static uint8_t _cacheFlush(sd_file* pfile) {
    return cacheFlush(pfile->vol_, false);
}

uint8_t cacheZeroBlock(sd_file* pfile, uint32_t blockNumber) {
  if (!_cacheFlush(pfile)) {
    return false;
  }

  // loop take less flash than memset(cacheBuffer_.data, 0, 512);
  for (uint16_t i = 0; i < 512; i++) {
    pfile->vol_->cacheBuffer_.data[i] = 0;
  }
  pfile->vol_->cacheBlockNumber_ = blockNumber;
  cacheSetDirty(pfile->vol_);
  return true;
}

uint8_t sd_close(sd_file* pfile) {
  if (!sync(pfile, false)) {
    return false;
  }
  pfile->type_ = FAT_FILE_TYPE_CLOSED;
  return true;
}
