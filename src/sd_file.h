#ifndef __SD_FILE_H
#define __SD_FILE_H
#include "sd_volume.h"

// flags for ls()
/** ls() flag to print modify date */
#define LS_DATE  1
/** ls() flag to print file size */
#define LS_SIZE  2
/** ls() flag for recursive list of subdirectories */
#define LS_R  4

// use the gnu style oflag in open()
/** open() oflag for reading */
#define O_READ  0X01
/** open() oflag - same as O_READ */
#define O_RDONLY  O_READ
/** open() oflag for write */
#define O_WRITE  0X02
/** open() oflag - same as O_WRITE */
#define O_WRONLY  O_WRITE
/** open() oflag for reading and writing */
#define O_RDWR  (O_READ | O_WRITE)
/** open() oflag mask for access modes */
#define O_ACCMODE  (O_READ | O_WRITE)
/** The file offset shall be set to the end of the file prior to each write. */
#define O_APPEND  0X04
/** synchronous writes - call sync() after each write */
#define O_SYNC  0X08
/** create the file if nonexistent */
#define O_CREAT  0X10
/** If O_CREAT and O_EXCL are set, open() shall fail if the file exists */
#define O_EXCL  0X20
/** truncate the file to zero length */
#define O_TRUNC  0X40

// flags for timestamp
/** set the file's last access date */
#define T_ACCESS  1
/** set the file's creation date and time */
#define T_CREATE  2
/** Set the file's write date and time */
#define T_WRITE  4
// values for type_
/** This SdFile has not been opened. */
#define FAT_FILE_TYPE_CLOSED  0
/** SdFile for a file */
#define FAT_FILE_TYPE_NORMAL  1
/** SdFile for a FAT16 root directory */
#define FAT_FILE_TYPE_ROOT16  2
/** SdFile for a FAT32 root directory */
#define FAT_FILE_TYPE_ROOT32  3
/** SdFile for a subdirectory */
#define FAT_FILE_TYPE_SUBDIR  4
/** Test value for directory type */
// #define FAT_FILE_TYPE_MIN_DIR  FAT_FILE_TYPE_ROOT16;

typedef struct __SD_FILE_PROT
{
//   cache cacheBuffer_;
  uint8_t flags_;
//   uint8_t blocksPerCluster_;
//   uint8_t clusterSizeShift_;
  uint32_t firstCluster_;
  uint32_t fileSize_;
  sd_volume *vol_;
//   uint16_t rootDirEntryCount_;
//   uint32_t rootDirStart_;
  uint32_t curCluster_;
  uint32_t curPosition_;
  uint8_t type_;
//   uint8_t cacheDirty_;
  uint32_t dirBlock_;
  uint32_t dirIndex_;
  uint32_t allocSearchStart_;
  //------------------------------------------------------------------------------
// callback function for date/time
void (*dateTime_)(uint16_t* date, uint16_t* time);

} sd_file;

#ifdef __cplusplus
extern "C" {
#endif
uint8_t isOpen(sd_file* pfile);
uint8_t openRoot(sd_file* pfile, sd_volume* vol);
dir_t* readDirCache(sd_file* dirFile);
uint8_t fat_seekSet(sd_file* pfile, uint32_t pos);
uint8_t fat_openCachedEntry(sd_file* pfile, uint8_t dirIndex, uint8_t oflag);
uint8_t isDir(sd_file* pfile);
uint8_t truncate(sd_file* pfile, uint32_t length);
uint8_t isFile(sd_file* pfile);
uint8_t seekSet(sd_file* pfile, uint32_t pos);
uint8_t sync(sd_file* pfile, uint8_t blocking);
void dateTimeCallback(
    sd_file* pfile,
    void (*dateTime)(uint16_t* date, uint16_t* time));
dir_t* cacheDirEntry(sd_file* pfile, uint8_t action);
uint8_t blockOfCluster(sd_file* pfile, uint32_t position);
uint32_t clusterStartBlock(sd_file* pfile, uint32_t cluster);
void clearUnbufferedRead(sd_file* pfile);
uint8_t unbufferedRead(sd_file* pfile);
int16_t sd_read(sd_file* pfile);
uint8_t sd_open(sd_file* dirFile, sd_file* pfile, const char* fileName, uint8_t oflag);
uint8_t openCachedEntry(sd_file* dirFile, uint8_t dirIndex, uint8_t oflag);
uint8_t make83Name(const char* str, uint8_t* name);
void rewind(sd_file* pfile);
uint8_t addDirCluster(sd_file* pfile);
uint8_t addCluster(sd_file* pfile);
uint8_t allocContiguous(sd_file* pfile, uint32_t count, uint32_t* curCluster);
uint8_t cacheZeroBlock(sd_file* pfile, uint32_t blockNumber);
uint8_t sd_close(sd_file* pfile);
#ifdef __cplusplus
}
#endif
#endif