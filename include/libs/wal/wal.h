/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef _TD_WAL_H_
#define _TD_WAL_H_

#include "os.h"
#include "tdef.h"
#include "tlog.h"
#include "tarray.h"
#ifdef __cplusplus
extern "C" {
#endif

extern int32_t wDebugFlag;

#define wFatal(...) { if (wDebugFlag & DEBUG_FATAL) { taosPrintLog("WAL FATAL ", 255, __VA_ARGS__); }}
#define wError(...) { if (wDebugFlag & DEBUG_ERROR) { taosPrintLog("WAL ERROR ", 255, __VA_ARGS__); }}
#define wWarn(...)  { if (wDebugFlag & DEBUG_WARN)  { taosPrintLog("WAL WARN ", 255, __VA_ARGS__); }}
#define wInfo(...)  { if (wDebugFlag & DEBUG_INFO)  { taosPrintLog("WAL ", 255, __VA_ARGS__); }}
#define wDebug(...) { if (wDebugFlag & DEBUG_DEBUG) { taosPrintLog("WAL ", wDebugFlag, __VA_ARGS__); }}
#define wTrace(...) { if (wDebugFlag & DEBUG_TRACE) { taosPrintLog("WAL ", wDebugFlag, __VA_ARGS__); }}

typedef enum {
  TAOS_WAL_NOLOG = 0,
  TAOS_WAL_WRITE = 1,
  TAOS_WAL_FSYNC = 2
} EWalType;

typedef struct {
  int8_t   sver;
  uint8_t  msgType;
  int8_t   reserved[2];
  int32_t  len;
  int64_t  version;
  uint32_t signature;
  uint32_t cksumHead;
  uint32_t cksumBody;
  //char     cont[];
} SWalHead;

typedef struct {
  int32_t  vgId;
  int32_t  fsyncPeriod;  // millisecond
  EWalType walLevel;     // wal level
} SWalCfg;

#define WAL_PREFIX       "wal"
#define WAL_PREFIX_LEN   3
#define WAL_NOSUFFIX_LEN 20
#define WAL_SUFFIX_AT    (WAL_NOSUFFIX_LEN+1)
#define WAL_LOG_SUFFIX   "log"
#define WAL_INDEX_SUFFIX "idx"
#define WAL_REFRESH_MS   1000
#define WAL_MAX_SIZE     (TSDB_MAX_WAL_SIZE + sizeof(SWalHead) + 16)
#define WAL_SIGNATURE    ((uint32_t)(0xFAFBFDFEUL))
#define WAL_PATH_LEN     (TSDB_FILENAME_LEN + 12)
#define WAL_FILE_LEN     (WAL_PATH_LEN + 32)
//#define WAL_FILE_NUM     1 // 3
#define WAL_FILESET_MAX  128

#define WAL_IDX_ENTRY_SIZE     (sizeof(int64_t)*2)
#define WAL_CUR_POS_READ_ONLY  1
#define WAL_CUR_FILE_READ_ONLY 2

typedef struct SWal {
  // cfg
  int32_t  vgId;
  int32_t  fsyncPeriod;  // millisecond
  EWalType level;
  //reference
  int64_t refId;
  //current tfd
  int64_t curLogTfd;
  int64_t curIdxTfd;
  //current version
  int64_t curVersion;
  int64_t curOffset;
  //current file version
  int64_t curFileFirstVersion;
  int64_t curFileLastVersion;
  //wal fileset version
  int64_t firstVersion;
  int64_t snapshotVersion;
  int64_t lastVersion;
  //fsync status
  int32_t fsyncSeq;
  //ctl
  int32_t curStatus;
  pthread_mutex_t mutex;
  //path
  char path[WAL_PATH_LEN];
  //file set
  SArray* fileSet;
  //reusable write head
  SWalHead head;
} SWal;  // WAL HANDLE

typedef int32_t (*FWalWrite)(void *ahandle, void *pHead);

// module initialization
int32_t walInit();
void    walCleanUp();

// handle open and ctl
SWal   *walOpen(const char *path, SWalCfg *pCfg);
int32_t walAlter(SWal *, SWalCfg *pCfg);
void    walClose(SWal *);

// write
//int64_t  walWriteWithMsgType(SWal*, int8_t msgType, void* body, int32_t bodyLen);
int64_t walWrite(SWal *, int64_t index, uint8_t msgType, void *body, int32_t bodyLen);
//int64_t walWriteBatch(SWal *, void **bodies, int32_t *bodyLen, int32_t batchSize);

// apis for lifecycle management
void    walFsync(SWal *, bool force);
int32_t walCommit(SWal *, int64_t ver);
// truncate after
int32_t walRollback(SWal *, int64_t ver);
// notify that previous logs can be pruned safely
int32_t walTakeSnapshot(SWal *, int64_t ver);
//int32_t  walDataCorrupted(SWal*);

// read
int32_t walRead(SWal *, SWalHead **, int64_t ver);
int32_t walReadWithFp(SWal *, FWalWrite writeFp, int64_t verStart, int32_t readNum);

// lifecycle check
int64_t walGetFirstVer(SWal *);
int64_t walGetSnapshotVer(SWal *);
int64_t walGetLastVer(SWal *);

#ifdef __cplusplus
}
#endif

#endif  // _TD_WAL_H_