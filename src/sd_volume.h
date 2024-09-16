#ifndef __SD_VOLUME_H
#define __SD_VOLUME_H

#include <pico/stdlib.h>
#include <pico/binary_info.h>
#include <hardware/spi.h>

struct partitionTable {
  /**
     Boot Indicator . Indicates whether the volume is the active
     partition.  Legal values include: 0X00. Do not use for booting.
     0X80 Active partition.
  */
  uint8_t  boot;
  /**
      Head part of Cylinder-head-sector address of the first block in
      the partition. Legal values are 0-255. Only used in old PC BIOS.
  */
  uint8_t  beginHead;
  /**
     Sector part of Cylinder-head-sector address of the first block in
     the partition. Legal values are 1-63. Only used in old PC BIOS.
  */
  unsigned beginSector : 6;
  /** High bits cylinder for first block in partition. */
  unsigned beginCylinderHigh : 2;
  /**
     Combine beginCylinderLow with beginCylinderHigh. Legal values
     are 0-1023.  Only used in old PC BIOS.
  */
  uint8_t  beginCylinderLow;
  /**
     Partition type. See defines that begin with PART_TYPE_ for
     some Microsoft partition types.
  */
  uint8_t  type;
  /**
     head part of cylinder-head-sector address of the last sector in the
     partition.  Legal values are 0-255. Only used in old PC BIOS.
  */
  uint8_t  endHead;
  /**
     Sector part of cylinder-head-sector address of the last sector in
     the partition.  Legal values are 1-63. Only used in old PC BIOS.
  */
  unsigned endSector : 6;
  /** High bits of end cylinder */
  unsigned endCylinderHigh : 2;
  /**
     Combine endCylinderLow with endCylinderHigh. Legal values
     are 0-1023.  Only used in old PC BIOS.
  */
  uint8_t  endCylinderLow;
  /** Logical block address of the first block in the partition. */
  uint32_t firstSector;
  /** Length of the partition, in blocks. */
  uint32_t totalSectors;
} __attribute__((packed));
/** Type name for partitionTable */
typedef struct partitionTable part_t;

struct masterBootRecord {
  /** Code Area for master boot program. */
  uint8_t  codeArea[440];
  /** Optional WindowsNT disk signature. May contain more boot code. */
  uint32_t diskSignature;
  /** Usually zero but may be more boot code. */
  uint16_t usuallyZero;
  /** Partition tables. */
  part_t   part[4];
  /** First MBR signature byte. Must be 0X55 */
  uint8_t  mbrSig0;
  /** Second MBR signature byte. Must be 0XAA */
  uint8_t  mbrSig1;
} __attribute__((packed));

typedef struct masterBootRecord mbr_t;

struct biosParmBlock {
  /**
     Count of bytes per sector. This value may take on only the
     following values: 512, 1024, 2048 or 4096
  */
  uint16_t bytesPerSector;
  /**
     Number of sectors per allocation unit. This value must be a
     power of 2 that is greater than 0. The legal values are
     1, 2, 4, 8, 16, 32, 64, and 128.
  */
  uint8_t  sectorsPerCluster;
  /**
     Number of sectors before the first FAT.
     This value must not be zero.
  */
  uint16_t reservedSectorCount;
  /** The count of FAT data structures on the volume. This field should
      always contain the value 2 for any FAT volume of any type.
  */
  uint8_t  fatCount;
  /**
    For FAT12 and FAT16 volumes, this field contains the count of
    32-byte directory entries in the root directory. For FAT32 volumes,
    this field must be set to 0. For FAT12 and FAT16 volumes, this
    value should always specify a count that when multiplied by 32
    results in a multiple of bytesPerSector.  FAT16 volumes should
    use the value 512.
  */
  uint16_t rootDirEntryCount;
  /**
     This field is the old 16-bit total count of sectors on the volume.
     This count includes the count of all sectors in all four regions
     of the volume. This field can be 0; if it is 0, then totalSectors32
     must be non-zero.  For FAT32 volumes, this field must be 0. For
     FAT12 and FAT16 volumes, this field contains the sector count, and
     totalSectors32 is 0 if the total sector count fits
     (is less than 0x10000).
  */
  uint16_t totalSectors16;
  /**
     This dates back to the old MS-DOS 1.x media determination and is
     no longer usually used for anything.  0xF8 is the standard value
     for fixed (non-removable) media. For removable media, 0xF0 is
     frequently used. Legal values are 0xF0 or 0xF8-0xFF.
  */
  uint8_t  mediaType;
  /**
     Count of sectors occupied by one FAT on FAT12/FAT16 volumes.
     On FAT32 volumes this field must be 0, and sectorsPerFat32
     contains the FAT size count.
  */
  uint16_t sectorsPerFat16;
  /** Sectors per track for interrupt 0x13. Not used otherwise. */
  uint16_t sectorsPerTrtack;
  /** Number of heads for interrupt 0x13.  Not used otherwise. */
  uint16_t headCount;
  /**
     Count of hidden sectors preceding the partition that contains this
     FAT volume. This field is generally only relevant for media
      visible on interrupt 0x13.
  */
  uint32_t hidddenSectors;
  /**
     This field is the new 32-bit total count of sectors on the volume.
     This count includes the count of all sectors in all four regions
     of the volume.  This field can be 0; if it is 0, then
     totalSectors16 must be non-zero.
  */
  uint32_t totalSectors32;
  /**
     Count of sectors occupied by one FAT on FAT32 volumes.
  */
  uint32_t sectorsPerFat32;
  /**
     This field is only defined for FAT32 media and does not exist on
     FAT12 and FAT16 media.
     Bits 0-3 -- Zero-based number of active FAT.
                 Only valid if mirroring is disabled.
     Bits 4-6 -- Reserved.
     Bit 7	-- 0 means the FAT is mirrored at runtime into all FATs.
            -- 1 means only one FAT is active; it is the one referenced in bits 0-3.
     Bits 8-15 	-- Reserved.
  */
  uint16_t fat32Flags;
  /**
     FAT32 version. High byte is major revision number.
     Low byte is minor revision number. Only 0.0 define.
  */
  uint16_t fat32Version;
  /**
     Cluster number of the first cluster of the root directory for FAT32.
     This usually 2 but not required to be 2.
  */
  uint32_t fat32RootCluster;
  /**
     Sector number of FSINFO structure in the reserved area of the
     FAT32 volume. Usually 1.
  */
  uint16_t fat32FSInfo;
  /**
     If non-zero, indicates the sector number in the reserved area
     of the volume of a copy of the boot record. Usually 6.
     No value other than 6 is recommended.
  */
  uint16_t fat32BackBootBlock;
  /**
     Reserved for future expansion. Code that formats FAT32 volumes
     should always set all of the bytes of this field to 0.
  */
  uint8_t  fat32Reserved[12];
} __attribute__((packed));
/** Type name for biosParmBlock */
typedef struct biosParmBlock bpb_t;

struct directoryEntry {
  /**
     Short 8.3 name.
     The first eight bytes contain the file name with blank fill.
     The last three bytes contain the file extension with blank fill.
  */
  uint8_t  name[11];
  /** Entry attributes.

     The upper two bits of the attribute byte are reserved and should
     always be set to 0 when a file is created and never modified or
     looked at after that.  See defines that begin with DIR_ATT_.
  */
  uint8_t  attributes;
  /**
     Reserved for use by Windows NT. Set value to 0 when a file is
     created and never modify or look at it after that.
  */
  uint8_t  reservedNT;
  /**
     The granularity of the seconds part of creationTime is 2 seconds
     so this field is a count of tenths of a second and its valid
     value range is 0-199 inclusive. (WHG note - seems to be hundredths)
  */
  uint8_t  creationTimeTenths;
  /** Time file was created. */
  uint16_t creationTime;
  /** Date file was created. */
  uint16_t creationDate;
  /**
     Last access date. Note that there is no last access time, only
     a date.  This is the date of last read or write. In the case of
     a write, this should be set to the same date as lastWriteDate.
  */
  uint16_t lastAccessDate;
  /**
     High word of this entry's first cluster number (always 0 for a
     FAT12 or FAT16 volume).
  */
  uint16_t firstClusterHigh;
  /** Time of last write. File creation is considered a write. */
  uint16_t lastWriteTime;
  /** Date of last write. File creation is considered a write. */
  uint16_t lastWriteDate;
  /** Low word of this entry's first cluster number. */
  uint16_t firstClusterLow;
  /** 32-bit unsigned holding this file's size in bytes. */
  uint32_t fileSize;
} __attribute__((packed));
//------------------------------------------------------------------------------
// Definitions for directory entries
//
/** Type name for directoryEntry */
typedef struct directoryEntry dir_t;

struct fat32BootSector {
  /** X86 jmp to boot program */
  uint8_t  jmpToBootCode[3];
  /** informational only - don't depend on it */
  char     oemName[8];
  /** BIOS Parameter Block */
  bpb_t    bpb;
  /** for int0x13 use value 0X80 for hard drive */
  uint8_t  driveNumber;
  /** used by Windows NT - should be zero for FAT */
  uint8_t  reserved1;
  /** 0X29 if next three fields are valid */
  uint8_t  bootSignature;
  /** usually generated by combining date and time */
  uint32_t volumeSerialNumber;
  /** should match volume label in root dir */
  char     volumeLabel[11];
  /** informational only - don't depend on it */
  char     fileSystemType[8];
  /** X86 boot code */
  uint8_t  bootCode[420];
  /** must be 0X55 */
  uint8_t  bootSectorSig0;
  /** must be 0XAA */
  uint8_t  bootSectorSig1;
} __attribute__((packed));
//------------------------------------------------------------------------------
// End Of Chain values for FAT entries
/** FAT16 end of chain value used by Microsoft. */
#define FAT16EOC  0XFFFF
/** Minimum value for FAT16 EOC.  Use to test for EOC. */
#define FAT16EOC_MIN  0XFFF8
/** FAT32 end of chain value used by Microsoft. */
#define FAT32EOC  0X0FFFFFFF
/** Minimum value for FAT32 EOC.  Use to test for EOC. */
#define FAT32EOC_MIN  0X0FFFFFF8
/** Mask a for FAT32 entry. Entries are 28 bits. */
#define FAT32MASK  0X0FFFFFFF


/** escape for name[0] = 0XE5 */
#define DIR_NAME_0XE5 0X05
/** name[0] value for entry that is free after being "deleted" */
#define DIR_NAME_DELETED 0XE5
/** name[0] value for entry that is free and no allocated entries follow */
#define DIR_NAME_FREE 0X00
/** file is read-only */
#define DIR_ATT_READ_ONLY 0X01
/** File should hidden in directory listings */
#define DIR_ATT_HIDDEN 0X02
/** Entry is for a system file */
#define DIR_ATT_SYSTEM 0X04
/** Directory entry contains the volume label */
#define DIR_ATT_VOLUME_ID 0X08
/** Entry is for a directory */
#define DIR_ATT_DIRECTORY 0X10
/** Old DOS archive bit for backup support */
#define DIR_ATT_ARCHIVE 0X20
/** Test value for long name entry.  Test is
  (d->attributes & DIR_ATT_LONG_NAME_MASK) == DIR_ATT_LONG_NAME. */
#define DIR_ATT_LONG_NAME 0X0F
/** Test mask for long name entry */
#define DIR_ATT_LONG_NAME_MASK 0X3F
/** defined attribute bits */
#define DIR_ATT_DEFINED_BITS 0X3F

/** ls() flag to print modify date */
#define LS_DATE 1
/** ls() flag to print file size */
#define LS_SIZE 2
/** ls() flag for recursive list of subdirectories */
#define LS_R 4

// use the gnu style oflag in open()
/** open() oflag for reading */
#define O_READ 0X01
/** open() oflag - same as O_READ */
#define O_RDONLY O_READ
/** open() oflag for write */
#define O_WRITE 0X02
/** open() oflag - same as O_WRITE */
#define O_WRONLY O_WRITE
/** open() oflag for reading and writing */
#define O_RDWR (O_READ | O_WRITE)
/** open() oflag mask for access modes */
#define O_ACCMODE (O_READ | O_WRITE)
/** The file offset shall be set to the end of the file prior to each write. */
#define O_APPEND 0X04
/** synchronous writes - call sync() after each write */
#define O_SYNC 0X08
/** create the file if nonexistent */
#define O_CREAT 0X10
/** If O_CREAT and O_EXCL are set, open() shall fail if the file exists */
#define O_EXCL 0X20
/** truncate the file to zero length */
#define O_TRUNC 0X40

// flags for timestamp
/** set the file's last access date */
#define T_ACCESS 1
/** set the file's creation date and time */
#define T_CREATE 2
/** Set the file's write date and time */
#define T_WRITE 4
// values for type_
/** This SdFile has not been opened. */
#define FAT_FILE_TYPE_CLOSED 0
/** SdFile for a file */
#define FAT_FILE_TYPE_NORMAL 1
/** SdFile for a FAT16 root directory */
#define FAT_FILE_TYPE_ROOT16 2
/** SdFile for a FAT32 root directory */
#define FAT_FILE_TYPE_ROOT32 3
/** SdFile for a subdirectory */
#define FAT_FILE_TYPE_SUBDIR 4
/** Test value for directory type */
#define FAT_FILE_TYPE_MIN_DIR FAT_FILE_TYPE_ROOT16

#define F_OFLAG = (O_ACCMODE | O_APPEND | O_SYNC);
    // available bits
#define F_FILE_NON_BLOCKING_WRITE 0X10
    // a new cluster was added to the file
#define F_FILE_CLUSTER_ADDED 0X20
    // use unbuffered SD read
#define F_FILE_UNBUFFERED_READ 0X40
    // sync of directory entry required
#define F_FILE_DIR_DIRTY 0X80

#define isEOC(pVolume, cluster) (cluster >= ((pVolume->fatType_ == 16 )? FAT16EOC_MIN : FAT32EOC_MIN))
/** Type name for fat32BootSector */
typedef struct fat32BootSector fbs_t;

union cache_t {
  /** Used to access cached file data blocks. */
  uint8_t  data[512];
  /** Used to access cached FAT16 entries. */
  uint16_t fat16[256];
  /** Used to access cached FAT32 entries. */
  uint32_t fat32[128];
  /** Used to access cached directory entries. */
  dir_t    dir[16];
  /** Used to access a cached MasterBoot Record. */
  mbr_t    mbr;
  /** Used to access to a cached FAT boot sector. */
  fbs_t    fbs;
};

typedef union cache_t cache;

// value for action argument in cacheRawBlock to indicate read from cache
#define CACHE_FOR_READ 0
// value for action argument in cacheRawBlock to indicate cache dirty
#define CACHE_FOR_WRITE 1

typedef struct __SD_VOLUME_PROT
{
  cache cacheBuffer_;
  uint8_t fatCount_;
  uint8_t blocksPerCluster_;
  uint8_t clusterSizeShift_;
  uint32_t blocksPerFat_;
  uint32_t fatStartBlock_;
  uint16_t rootDirEntryCount_;
  uint32_t rootDirStart_;
  uint32_t dataStartBlock_;
  uint32_t clusterCount_;
  uint8_t fatType_;
  uint8_t cacheDirty_;
  uint32_t cacheBlockNumber_;
  uint32_t cacheMirrorBlock_;
  uint8_t partition_;
} sd_volume;


inline uint8_t DIR_IS_LONG_NAME(const dir_t* dir) {
  return (dir->attributes & DIR_ATT_LONG_NAME_MASK) == DIR_ATT_LONG_NAME;
}
/** Mask for file/subdirectory tests */
#define DIR_ATT_FILE_TYPE_MASK (DIR_ATT_VOLUME_ID | DIR_ATT_DIRECTORY)
/** Directory entry is for a file */
inline uint8_t DIR_IS_FILE(const dir_t* dir) {
  return (dir->attributes & DIR_ATT_FILE_TYPE_MASK) == 0;
}
/** Directory entry is for a subdirectory */
inline uint8_t DIR_IS_SUBDIR(const dir_t* dir) {
  return (dir->attributes & DIR_ATT_FILE_TYPE_MASK) == DIR_ATT_DIRECTORY;
}
/** Directory entry is for a file or subdirectory */
inline uint8_t DIR_IS_FILE_OR_SUBDIR(const dir_t* dir) {
  return (dir->attributes & DIR_ATT_VOLUME_ID) == 0;
}

#ifdef __cplusplus
extern "C" {
#endif
uint8_t sd_volume_init(sd_volume* pvolume);
uint8_t cacheRawBlock(sd_volume* pvolume, uint32_t blockNumber, uint8_t action);
uint8_t cacheMirrorBlockFlush(sd_volume* pvolume, uint8_t blocking);
uint8_t cacheFlush(sd_volume* pvolume, uint8_t blocking);
uint8_t fatType(sd_volume* pvolume);
uint32_t rootDirEntryCount(sd_volume* pvolume);
uint32_t rootDirStart(sd_volume* pvolume);
uint8_t chainSize(sd_volume* pvolume, uint32_t cluster, uint32_t* size);
uint8_t fatGet(sd_volume* pvolume, uint32_t cluster, uint32_t* value);
uint8_t fatPut(sd_volume* pvolume, uint32_t cluster, uint32_t value);
uint8_t fatPutEOC(sd_volume* pvolume, uint32_t cluster);
void cacheSetDirty(sd_volume* pvolume);
#ifdef __cplusplus
}
#endif
#endif