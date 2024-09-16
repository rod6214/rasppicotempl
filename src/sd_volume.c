#include "sd_volume.h"
#include "sd_driver.h"

static uint8_t _sd_volume_init(sd_volume* pvolume, uint8_t partition);
static uint8_t _cacheFlush(sd_volume* pvolume);

uint8_t sd_volume_init(sd_volume* pvolume) {
  return _sd_volume_init(pvolume, 1) ? true : _sd_volume_init(pvolume, 0); 
}

static uint8_t _sd_volume_init(sd_volume* pvolume, uint8_t partition) {
    
    pvolume->cacheDirty_ = 0;
    pvolume->cacheBlockNumber_ = 0xFFFFFFFF;
    pvolume->cacheMirrorBlock_ = 0;

    uint32_t volumeStartBlock = 0;
    init_sd_core();

    if (partition > 0) {

      cacheRawBlock(pvolume, volumeStartBlock, CACHE_FOR_READ);

      part_t* p = &pvolume->cacheBuffer_.mbr.part[partition - 1];
      if ((p->boot & 0X7F) != 0  ||
          p->totalSectors < 100 ||
          p->firstSector == 0) {
        // not a valid partition
        return false;
      }
      volumeStartBlock = p->firstSector;
    }


    cacheRawBlock(pvolume, volumeStartBlock, CACHE_FOR_READ);
    
    bpb_t* bpb = &pvolume->cacheBuffer_.fbs.bpb;
    if (bpb->bytesPerSector != 512 ||
        bpb->fatCount == 0 ||
        bpb->reservedSectorCount == 0 ||
        bpb->sectorsPerCluster == 0) {
        // not valid FAT volume
        return FALSE;
    }
    pvolume->fatCount_ = bpb->fatCount;
    pvolume->blocksPerCluster_ = bpb->sectorsPerCluster;

    // determine shift that is same as multiply by blocksPerCluster_
    pvolume->clusterSizeShift_ = 0;
    while (pvolume->blocksPerCluster_ != (1 << pvolume->clusterSizeShift_)) {
        // error if not power of 2
        if (pvolume->clusterSizeShift_++ > 7) {
        return FALSE;
        }
    }
    pvolume->blocksPerFat_ = bpb->sectorsPerFat16 ?
                    bpb->sectorsPerFat16 : bpb->sectorsPerFat32;

    pvolume->fatStartBlock_ = volumeStartBlock + bpb->reservedSectorCount;

    // count for FAT16 zero for FAT32
    pvolume->rootDirEntryCount_ = bpb->rootDirEntryCount;

    // directory start for FAT16 dataStart for FAT32
    pvolume->rootDirStart_ = pvolume->fatStartBlock_ + bpb->fatCount * pvolume->blocksPerFat_;

    // data start for FAT16 and FAT32
    pvolume->dataStartBlock_ = pvolume->rootDirStart_ + ((32 * bpb->rootDirEntryCount + 511) / 512);

    // total blocks for FAT16 or FAT32
    uint32_t totalBlocks = bpb->totalSectors16 ?
                            bpb->totalSectors16 : bpb->totalSectors32;
    // total data blocks
    pvolume->clusterCount_ = totalBlocks - (pvolume->dataStartBlock_ - volumeStartBlock);

    // divide by cluster size to get cluster count
    pvolume->clusterCount_ >>= pvolume->clusterSizeShift_;

    // FAT type is determined by cluster count
    if (pvolume->clusterCount_ < 4085) {
        pvolume->fatType_ = 12;
    } else if (pvolume->clusterCount_ < 65525) {
        pvolume->fatType_ = 16;
    } else {
        pvolume->rootDirStart_ = bpb->fat32RootCluster;
        pvolume->fatType_ = 32;
    }
    return TRUE;
}

uint8_t cacheFlush(sd_volume* pvolume, uint8_t blocking) {
  if (pvolume->cacheDirty_) {
    if (!writeBlock(pvolume->cacheBlockNumber_, pvolume->cacheBuffer_.data, blocking)) {
      return false;
    }

    if (!blocking) {
      return true;
    }

    // mirror FAT tables
    if (!cacheMirrorBlockFlush(pvolume, blocking)) {
      return false;
    }
    pvolume->cacheDirty_ = 0;
  }
  return true;
}

uint8_t fatType(sd_volume* pvolume) {
  return pvolume->fatType_;
}

uint32_t rootDirEntryCount(sd_volume* pvolume) {
  return pvolume->rootDirEntryCount_;
}

uint32_t rootDirStart(sd_volume* pvolume) {
  return pvolume->rootDirStart_;
}

uint8_t chainSize(sd_volume* pvolume, uint32_t cluster, uint32_t* size) {
  uint32_t s = 0;
  do {
    if (!fatGet(pvolume, cluster, &cluster)) {
      return false;
    }
    s += 512UL << pvolume->clusterSizeShift_;
  } while (!isEOC(pvolume, cluster));
  *size = s;
  return true;
}



uint8_t cacheMirrorBlockFlush(sd_volume* pvolume, uint8_t blocking) {
  if (pvolume->cacheMirrorBlock_) {
    if (!writeBlock(pvolume->cacheMirrorBlock_, pvolume->cacheBuffer_.data, blocking)) {
      return false;
    }
    pvolume->cacheMirrorBlock_ = 0;
  }
  return true;
}

uint8_t cacheRawBlock(sd_volume* pvolume, uint32_t blockNumber, uint8_t action) {
  if (pvolume->cacheBlockNumber_ != blockNumber) {
    if (!_cacheFlush(pvolume)) {
      return false;
    }
    if (!readBlock(blockNumber, pvolume->cacheBuffer_.data)) {
      return false;
    }
    pvolume->cacheBlockNumber_ = blockNumber;
  }
  pvolume->cacheDirty_ |= action;
  return true;
}

static uint8_t _cacheFlush(sd_volume* pvolume) {
  cacheFlush(pvolume, false);
}

uint8_t fatGet(sd_volume* pvolume, uint32_t cluster, uint32_t* value) {
  if (cluster > (pvolume->clusterCount_ + 1)) {
    return false;
  }
  uint32_t lba = pvolume->fatStartBlock_;
  lba += pvolume->fatType_ == 16 ? cluster >> 8 : cluster >> 7;
  if (lba != pvolume->cacheBlockNumber_) {
    if (!cacheRawBlock(pvolume, lba, CACHE_FOR_READ)) {
      return false;
    }
  }
  if (pvolume->fatType_ == 16) {
    *value = pvolume->cacheBuffer_.fat16[cluster & 0XFF];
  } else {
    *value = pvolume->cacheBuffer_.fat32[cluster & 0X7F] & FAT32MASK;
  }
  return true;
}

// Store a FAT entry
uint8_t fatPut(sd_volume* pvolume, uint32_t cluster, uint32_t value) {
  // error if reserved cluster
  if (cluster < 2) {
    return false;
  }

  // error if not in FAT
  if (cluster > (pvolume->clusterCount_ + 1)) {
    return false;
  }

  // calculate block address for entry
  uint32_t lba = pvolume->fatStartBlock_;
  lba += pvolume->fatType_ == 16 ? cluster >> 8 : cluster >> 7;

  if (lba != pvolume->cacheBlockNumber_) {
    if (!cacheRawBlock(pvolume, lba, CACHE_FOR_READ)) {
      return false;
    }
  }
  // store entry
  if (pvolume->fatType_ == 16) {
    pvolume->cacheBuffer_.fat16[cluster & 0XFF] = value;
  } else {
    pvolume->cacheBuffer_.fat32[cluster & 0X7F] = value;
  }
  cacheSetDirty(pvolume);

  // mirror second FAT
  if (pvolume->fatCount_ > 1) {
    pvolume->cacheMirrorBlock_ = lba + pvolume->blocksPerFat_;
  }
  return true;
}

uint8_t fatPutEOC(sd_volume* pvolume, uint32_t cluster) {
  return fatPut(pvolume, cluster, 0x0FFFFFFF);
}

void cacheSetDirty(sd_volume* pvolume) {
  pvolume->cacheDirty_ |= CACHE_FOR_WRITE;
}
