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

#ifndef TDENGINE_TSCLIENT_H
#define TDENGINE_TSCLIENT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "os.h"

#include "qAggMain.h"
#include "taos.h"
#include "taosdef.h"
#include "taosmsg.h"
#include "tarray.h"
#include "tcache.h"
#include "tglobal.h"
#include "tref.h"
#include "tutil.h"

#include "qExecutor.h"
#include "qSqlparser.h"
#include "qTsbuf.h"
#include "qUtil.h"
#include "tcmdtype.h"

// forward declaration
struct SSqlInfo;

typedef void (*__async_cb_func_t)(void *param, TAOS_RES *tres, int32_t numOfRows);
// #define __DEV_BRANCH__
#define __5221_BRANCH__

typedef struct SNewVgroupInfo {
  int32_t    vgId;
  int8_t     inUse;
  int8_t     numOfEps;
  SEpAddrMsg ep[TSDB_MAX_REPLICA];
} SNewVgroupInfo;

typedef struct CChildTableMeta {
  int32_t        vgId;
  STableId       id;
  uint8_t        tableType;
  char           sTableName[TSDB_TABLE_FNAME_LEN];  // TODO: refactor super table name, not full name
  uint64_t       suid;                              // super table id
} CChildTableMeta;

typedef struct SColumnIndex {
  int16_t tableIndex;
  int16_t columnIndex;
} SColumnIndex;

typedef struct SColumn {
  uint64_t     tableUid;
  int32_t      columnIndex;
  SColumnInfo  info;
} SColumn;

typedef struct SInternalField {
  TAOS_FIELD      field;
  bool            visible;
  SExprInfo      *pExpr;
} SInternalField;

typedef struct SParamInfo {
  int32_t  idx;
  uint8_t  type;
  uint8_t  timePrec;
  int16_t  bytes;
  uint32_t offset;
} SParamInfo;

typedef struct SBoundColumn {
  int32_t offset;   // all column offset value
  int32_t toffset;  // first part offset for SDataRow TODO: get offset from STSchema on future
  uint8_t valStat;  // denote if current column bound or not(0 means has val, 1 means no val)
} SBoundColumn;
typedef enum {
  VAL_STAT_YES = 0x0,  // 0 means has val
  VAL_STAT_NO = 0x01,  // 1 means no val
} EValStat;
typedef struct {
  uint16_t schemaColIdx;
  uint16_t boundIdx;
  uint16_t finalIdx;
} SBoundIdxInfo;

typedef enum _COL_ORDER_STATUS {
  ORDER_STATUS_UNKNOWN = 0,
  ORDER_STATUS_ORDERED = 1,
  ORDER_STATUS_DISORDERED = 2,
} EOrderStatus;

typedef struct SParsedDataColInfo {
  int16_t        numOfCols;
  int16_t        numOfBound;
  uint16_t       flen;            // TODO: get from STSchema
  uint16_t       allNullLen;      // TODO: get from STSchema
  uint16_t       extendedVarLen;
  int32_t *      boundedColumns;  // bound column idx according to schema
  SBoundColumn * cols;
  SBoundIdxInfo *colIdxInfo;
  int8_t         orderStatus;  // bound columns
} SParsedDataColInfo;

#define IS_DATA_COL_ORDERED(spd) ((spd->orderStatus) == (int8_t)ORDER_STATUS_ORDERED)

typedef struct {
  int32_t dataLen;  // len of SDataRow
  int32_t kvLen;    // len of SKVRow
} SMemRowInfo;
typedef struct {
  uint8_t      memRowType;
  uint16_t     nBoundCols;
  TDRowTLenT   dataRowInitLen;
  TDRowTLenT   kvRowInitLen;
  SArray *     colInfo;  // SColInfo
  SMemRowInfo *rowInfo;
} SMemRowBuilder;

int  initMemRowBuilder(SMemRowBuilder *pBuilder, uint32_t nRows, uint32_t nCols, uint32_t nBoundCols,
                       int32_t allNullLen);
void destroyMemRowBuilder(SMemRowBuilder *pBuilder);

/**
 * @brief
 *
 * @param memRowType
 * @param spd
 * @param idx   the absolute bound index of columns
 * @return FORCE_INLINE
 */
static FORCE_INLINE void tscGetMemRowAppendInfo(SSchema *pSchema, uint8_t memRowType, SParsedDataColInfo *spd,
                                                int32_t idx, int32_t *toffset, int16_t *colId) {
  int32_t schemaIdx = 0;
  if (IS_DATA_COL_ORDERED(spd)) {
    schemaIdx = spd->boundedColumns[idx];
    if (isDataRowT(memRowType)) {
      *toffset = (spd->cols + schemaIdx)->toffset;  // the offset of firstPart
    } else {
      *toffset = idx * sizeof(SColIdx);  // the offset of SColIdx
    }
  } else {
    ASSERT(idx == (spd->colIdxInfo + idx)->boundIdx);
    schemaIdx = (spd->colIdxInfo + idx)->schemaColIdx;
    if (isDataRowT(memRowType)) {
      *toffset = (spd->cols + schemaIdx)->toffset;
    } else {
      *toffset = ((spd->colIdxInfo + idx)->finalIdx) * sizeof(SColIdx);
    }
  }
  *colId = pSchema[schemaIdx].colId;
}

/**
 * @brief Applicable to consume by multi-columns
 *
 * @param row
 * @param value
 * @param isCopyVarData In some scenario, the varVal is copied to row directly before calling tdAppend***ColVal()
 * @param colId
 * @param colType
 * @param idx index in SSchema
 * @param pBuilder
 * @param spd
 * @return FORCE_INLINE
 */
static FORCE_INLINE void tscAppendMemRowColVal(SMemRow row, const void *value, bool isCopyVarData, int16_t colId,
                                               int8_t colType, int32_t toffset, SMemRowBuilder *pBuilder,
                                               int32_t rowNum) {
  tdAppendMemRowColVal(row, value, isCopyVarData, colId, colType, toffset);
  // TODO: When nBoundCols/nCols > 0.5,
  SMemRowInfo *pRowInfo = pBuilder->rowInfo + rowNum;
  tdGetColAppendDeltaLen(value, colType, &pRowInfo->dataLen, &pRowInfo->kvLen);
}

// Applicable to consume by one row
static FORCE_INLINE void tscAppendMemRowColValEx(SMemRow row, const void *value, bool isCopyVarData, int16_t colId,
                                                 int8_t colType, int32_t toffset, int32_t *dataLen, int32_t *kvLen) {
  tdAppendMemRowColVal(row, value, isCopyVarData, colId, colType, toffset);
  // TODO: When nBoundCols/nCols > 0.5,
  tdGetColAppendDeltaLen(value, colType, dataLen, kvLen);
}

static FORCE_INLINE void tscAppendDataRowColValEx(SDataRow row, const void *value, bool isCopyVarData, int16_t colId,
                                                  int8_t colType, int32_t toffset, int32_t *dataLen, int32_t *kvLen) {
  tdAppendDataColVal(row, value, isCopyVarData, colType, toffset);
  // TODO: When nBoundCols/nCols > 0.5,
  tdGetColAppendDeltaLen(value, colType, dataLen, kvLen);
}
static FORCE_INLINE void tscAppendKvRowColValEx(SKVRow row, const void *value, bool isCopyVarData, int16_t colId,
                                                int8_t colType, int32_t toffset, int32_t *dataLen, int32_t *kvLen) {
  tdAppendKvColVal(row, value, isCopyVarData, colId, colType, toffset);
  // TODO: When nBoundCols/nCols > 0.5,
  tdGetColAppendDeltaLen(value, colType, dataLen, kvLen);
}
typedef void (*FPAppendColVal)(SKVRow row, const void *value, bool isCopyVarData, int16_t colId, int8_t colType,
                               int32_t toffset, int32_t *dataLen, int32_t *kvLen);

int tranferRowKVToData(SMemRowBuilder *pBuilder, SMemRow rowKV, SMemRow rowData, TDRowTLenT destLen);

typedef struct STableDataBlocks {
  SName       tableName;
  int8_t      tsSource;     // where does the UNIX timestamp come from, server or client
  bool        ordered;      // if current rows are ordered or not
  bool        cloned;       // for the tables belongs to one stable
  int64_t     vgId;         // virtual group id
  int64_t     prevTS;       // previous timestamp, recorded to decide if the records array is ts ascending
  int32_t     numOfTables;  // number of tables in current submit block
  int32_t     rowSize;      // row size for current table
  uint32_t    nAllocSize;
  uint32_t    headerSize;  // header for table info (uid, tid, submit metadata)
  uint32_t    size;
  STableMeta *pTableMeta;  // the tableMeta of current table, the table meta will be used during submit, keep a ref to
                           // avoid to be removed from cache
  char *pData;

  SParsedDataColInfo boundColumnInfo;

  // for parameter ('?') binding
  uint32_t       numOfAllocedParams;
  uint32_t       numOfParams;
  SParamInfo *   params;
  SMemRowBuilder rowBuilder;
} STableDataBlocks;

typedef struct {
  STableMeta   *pTableMeta;
  SArray       *vgroupIdList;
//  SVgroupsInfo *pVgroupsInfo;
} STableMetaVgroupInfo;

typedef struct SInsertStatementParam {
  SName      **pTableNameList;          // all involved tableMeta list of current insert sql statement.
  int32_t      numOfTables;             // number of tables in table name list
  SHashObj    *pTableBlockHashList;     // data block for each table
  SArray      *pDataBlocks;             // SArray<STableDataBlocks*>. Merged submit block for each vgroup
  int8_t       schemaAttached;          // denote if submit block is built with table schema or not
  STagData     tagData;                 // NOTE: pTagData->data is used as a variant length array

  int32_t      batchSize;               // for parameter ('?') binding and batch processing
  int32_t      numOfParams;

  char         msg[512];                // error message
  uint32_t     insertType;              // insert data from [file|sql statement| bound statement]
  uint64_t     objectId;                // sql object id
  char        *sql;                     // current sql statement position
} SInsertStatementParam;

// TODO extract sql parser supporter
typedef struct {
  int     command;
  uint8_t msgType;
  SInsertStatementParam insertParam;
  char    reserve1[3];        // fix bus error on arm32
  int32_t count;   // todo remove it
  bool    subCmd;

  char         reserve2[3];        // fix bus error on arm32
  int16_t      numOfCols;
  char         reserve3[2];        // fix bus error on arm32
  uint32_t     allocSize;
  char *       payload;
  int32_t      payloadLen;

  SHashObj    *pTableMetaMap;  // local buffer to keep the queried table meta, before validating the AST
  SQueryInfo  *pQueryInfo;
  SQueryInfo  *active;         // current active query info
  int32_t      batchSize;      // for parameter ('?') binding and batch processing
  int32_t      resColumnId;
} SSqlCmd;

typedef struct SResRec {
  int numOfRows;
  int numOfTotal;
} SResRec;

typedef struct {
  int32_t        numOfRows;                  // num of results in current retrieval
  int64_t        numOfRowsGroup;             // num of results of current group
  int64_t        numOfTotal;                 // num of total results
  int64_t        numOfClauseTotal;           // num of total result in current subclause
  char *         pRsp;
  int32_t        rspType;
  int32_t        rspLen;
  uint64_t       qId;
  int64_t        useconds;
  int64_t        offset;  // offset value from vnode during projection query of stable
  int32_t        row;
  int16_t        numOfCols;
  int16_t        precision;
  bool           completed;
  int32_t        code;
  int32_t        numOfGroups;
  SResRec *      pGroupRec;
  char *         data;
  TAOS_ROW       tsrow;
  TAOS_ROW       urow;
  int32_t*       length;  // length for each field for current row
  char **        buffer;  // Buffer used to put multibytes encoded using unicode (wchar_t)
  SColumnIndex*  pColumnIndex;

  TAOS_FIELD*           final;
  SArithmeticSupport   *pArithSup;   // support the arithmetic expression calculation on agg functions
  struct SGlobalMerger *pMerger;
} SSqlRes;

typedef struct {
  char         key[512]; 
  void         *pDnodeConn; 
} SRpcObj;

typedef struct STscObj {
  void *             signature;
  void *             pTimer;
  char               user[TSDB_USER_LEN];
  char               pass[TSDB_KEY_LEN];
  char               acctId[TSDB_ACCT_ID_LEN];
  char               db[TSDB_ACCT_ID_LEN + TSDB_DB_NAME_LEN];
  char               sversion[TSDB_VERSION_LEN];
  char               writeAuth : 1;
  char               superAuth : 1;
  uint32_t           connId;
  uint64_t           rid;      // ref ID returned by taosAddRef
  int64_t            hbrid;
  struct SSqlObj *   sqlList;
  struct SSqlStream *streamList;
  SRpcObj           *pRpcObj;
  SRpcCorEpSet      *tscCorMgmtEpSet;
  pthread_mutex_t    mutex;
  int32_t            numOfObj; // number of sqlObj from this tscObj
} STscObj;

typedef struct SSubqueryState {
  pthread_mutex_t mutex;
  int8_t  *states;
  int32_t  numOfSub;            // the number of total sub-queries
  uint64_t numOfRetrievedRows;  // total number of points in this query
} SSubqueryState;

typedef struct SSqlObj {
  void            *signature;
  int64_t          owner;        // owner of sql object, by which it is executed
  STscObj         *pTscObj;
  int64_t          rpcRid;
  __async_cb_func_t  fp;
  __async_cb_func_t  fetchFp;
  void            *param;
  int64_t          stime;
  uint32_t         queryId;
  void *           pStream;
  void *           pSubscription;
  char *           sqlstr;
  void *           pBuf;  // table meta buffer
  char             parseRetry;
  char             retry;
  char             maxRetry;
  SRpcEpSet        epSet;
  char             listed;
  tsem_t           rspSem;
  SSqlCmd          cmd;
  SSqlRes          res;
  bool             isBind;
  
  SSubqueryState   subState;
  struct SSqlObj **pSubs;

  int64_t          metaRid;
  int64_t          svgroupRid;

  int64_t          squeryLock;
  int32_t          retryReason;  // previous error code
  struct SSqlObj  *prev, *next;
  int64_t          self;
} SSqlObj;

typedef struct SSqlStream {
  SSqlObj *pSql;
  void *  cqhandle;  // stream belong to SCQContext handle
  const char* dstTable;
  uint32_t streamId;
  char     listed;
  bool     isProject;
  int16_t  precision;
  int64_t  num;  // number of computing count

  /*
   * keep the number of current result in computing,
   * the value will be set to 0 before set timer for next computing
   */
  int64_t numOfRes;

  int64_t useconds;  // total  elapsed time
  int64_t ctime;     // stream created time
  int64_t stime;     // stream next executed time
  int64_t etime;     // stream end query time, when time is larger then etime, the stream will be closed
  int64_t ltime;     // stream last row time in stream table
  SInterval interval;
  void *  pTimer;

  void (*fp)();
  void *param;

  void (*callback)(void *);  // Callback function when stream is stopped from client level
  struct SSqlStream *prev, *next;
} SSqlStream;

void tscSetStreamDestTable(SSqlStream* pStream, const char* dstTable);

int  tscAcquireRpc(const char *key, const char *user, const char *secret,void **pRpcObj);
void tscReleaseRpc(void *param);
void tscInitMsgsFp();

int tsParseSql(SSqlObj *pSql, bool initial);

void tscProcessMsgFromServer(SRpcMsg *rpcMsg, SRpcEpSet *pEpSet);
int  tscBuildAndSendRequest(SSqlObj *pSql, SQueryInfo* pQueryInfo);

int  tscRenewTableMeta(SSqlObj *pSql, int32_t tableIndex);
void tscAsyncResultOnError(SSqlObj *pSql);

void tscQueueAsyncError(void(*fp), void *param, int32_t code);

int tscProcessLocalCmd(SSqlObj *pSql);
int tscCfgDynamicOptions(char *msg);

int32_t tscTansformFuncForSTableQuery(SQueryInfo *pQueryInfo);
void    tscRestoreFuncForSTableQuery(SQueryInfo *pQueryInfo);

int32_t tscCreateResPointerInfo(SSqlRes *pRes, SQueryInfo *pQueryInfo);
void tscSetResRawPtr(SSqlRes* pRes, SQueryInfo* pQueryInfo);
void tscSetResRawPtrRv(SSqlRes* pRes, SQueryInfo* pQueryInfo, SSDataBlock* pBlock, bool convertNchar);

void handleDownstreamOperator(SSqlObj** pSqlList, int32_t numOfUpstream, SQueryInfo* px, SSqlObj* pParent);
void destroyTableNameList(SInsertStatementParam* pInsertParam);

void tscResetSqlCmd(SSqlCmd *pCmd, bool removeMeta);

/**
 * free query result of the sql object
 * @param pObj
 */
void tscFreeSqlResult(SSqlObj *pSql);

void* tscCleanupTableMetaMap(SHashObj* pTableMetaMap);

/**
 * free sql object, release allocated resource
 * @param pObj
 */
void tscFreeSqlObj(SSqlObj *pSql);
void tscFreeSubobj(SSqlObj* pSql);

void tscFreeRegisteredSqlObj(void *pSql);

void tscCloseTscObj(void *pObj);

// todo move to taos? or create a new file: taos_internal.h
TAOS *taos_connect_a(char *ip, char *user, char *pass, char *db, uint16_t port, void (*fp)(void *, TAOS_RES *, int),
                     void *param, TAOS **taos);
TAOS_RES* taos_query_h(TAOS* taos, const char *sqlstr, int64_t* res);
TAOS_RES * taos_query_ra(TAOS *taos, const char *sqlstr, __async_cb_func_t fp, void *param);

void waitForQueryRsp(void *param, TAOS_RES *tres, int code);

void doAsyncQuery(STscObj *pObj, SSqlObj *pSql, __async_cb_func_t fp, void *param, const char *sqlstr, size_t sqlLen);

void tscImportDataFromFile(SSqlObj *pSql);
struct SGlobalMerger* tscInitResObjForLocalQuery(int32_t numOfRes, int32_t rowLen, uint64_t id);
bool tscIsUpdateQuery(SSqlObj* pSql);
char* tscGetSqlStr(SSqlObj* pSql);
bool tscIsQueryWithLimit(SSqlObj* pSql);

bool tscHasReachLimitation(SQueryInfo *pQueryInfo, SSqlRes *pRes);
void tscSetBoundColumnInfo(SParsedDataColInfo *pColInfo, SSchema *pSchema, int32_t numOfCols);

char *tscGetErrorMsgPayload(SSqlCmd *pCmd);

int32_t tscInvalidOperationMsg(char *msg, const char *additionalInfo, const char *sql);
int32_t tscSQLSyntaxErrMsg(char* msg, const char* additionalInfo,  const char* sql);

int32_t tscValidateSqlInfo(SSqlObj *pSql, struct SSqlInfo *pInfo);

int32_t tsSetBlockInfo(SSubmitBlk *pBlocks, const STableMeta *pTableMeta, int32_t numOfRows);
extern int32_t    sentinel;
extern SHashObj  *tscVgroupMap;
extern SHashObj  *tscTableMetaMap;
extern SCacheObj *tscVgroupListBuf;

extern int   tscObjRef;
extern void *tscTmr;
extern void *tscQhandle;
extern int   tscKeepConn[];
extern int   tscRefId;
extern int   tscNumOfObj;     // number of existed sqlObj in current process.

extern int (*tscBuildMsg[TSDB_SQL_MAX])(SSqlObj *pSql, SSqlInfo *pInfo);
 
void tscBuildVgroupTableInfo(SSqlObj* pSql, STableMetaInfo* pTableMetaInfo, SArray* tables);
int16_t getNewResColId(SSqlCmd* pCmd);

int32_t schemaIdxCompar(const void *lhs, const void *rhs);
int32_t boundIdxCompar(const void *lhs, const void *rhs);
FORCE_INLINE int32_t getExtendedRowSize(STableDataBlocks *pBlock) {
#ifdef __5221_BRANCH__
  ASSERT(pBlock->rowSize == pBlock->pTableMeta->tableInfo.rowSize);
#endif
  return pBlock->pTableMeta->tableInfo.rowSize + TD_MEM_ROW_DATA_HEAD_SIZE + pBlock->boundColumnInfo.extendedVarLen;
}
FORCE_INLINE void checkAndConvertMemRow(SMemRow row, int32_t dataLen, int32_t kvLen) {
  if (isDataRow(row)) {
    if (kvLen < (dataLen * KVRatioConvert)) {
      memRowSetConvert(row);
    }
  } else if (kvLen > dataLen) {
    memRowSetConvert(row);
  }
}

FORCE_INLINE void initSMemRow(SMemRow row, uint8_t memRowType, STableDataBlocks *pBlock, int16_t nCols) {
  memRowSetType(row, memRowType);
  if (isDataRowT(memRowType)) {
    dataRowSetVersion(memRowDataBody(row), pBlock->pTableMeta->sversion);
    dataRowSetLen(memRowDataBody(row), (TDRowLenT)(TD_DATA_ROW_HEAD_SIZE + pBlock->boundColumnInfo.flen));
  } else {
    memRowSetKvVersion(row, pBlock->pTableMeta->sversion);
    kvRowSetNCols(memRowKvBody(row), pBlock->numOfParams);
    kvRowSetLen(memRowKvBody(row), (TDRowLenT)(TD_KV_ROW_HEAD_SIZE + sizeof(SColIdx) * pBlock->numOfParams));
  }
}
/**
 * TODO: Move to tdataformat.h and refactor when STSchema available.
 *    - fetch flen and toffset from STSChema and remove param spd
 */
static FORCE_INLINE void convertToSDataRow(SMemRow dest, SMemRow src, SSchema *pSchema, int nCols,
                                           SParsedDataColInfo *spd) {
  ASSERT(isKvRow(src));

  SDataRow dataRow = memRowDataBody(dest);
  SKVRow   kvRow = memRowKvBody(src);

  memRowSetType(dest, SMEM_ROW_DATA);
  dataRowSetVersion(dataRow, memRowKvVersion(src));
  dataRowSetLen(dataRow, (TDRowLenT)(TD_DATA_ROW_HEAD_SIZE + spd->flen));

  int32_t kvIdx = 0;
  for (int i = 0; i < nCols; ++i) {
    SSchema *schema = pSchema + i;
    char *   val = tdGetKVRowValOfColEx(kvRow, schema->colId, &kvIdx);
    tdAppendDataColVal(dataRow, val != NULL ? val : getNullValue(schema->type), true, schema->type,
                       (spd->cols + i)->toffset);
  }
}

// TODO: Move to tdataformat.h and refactor when STSchema available.
static FORCE_INLINE void convertToSKVRow(SMemRow dest, SMemRow src, SSchema *pSchema, int nCols, int nBoundCols,
                                         SParsedDataColInfo *spd) {
  ASSERT(isDataRow(src));

  SDataRow dataRow = memRowKvBody(src);
  SKVRow   kvRow = memRowDataBody(dest);

  memRowSetType(dest, SMEM_ROW_KV);
  memRowSetKvVersion(kvRow, dataRowVersion(dataRow));
  kvRowSetNCols(kvRow, nBoundCols);
  kvRowSetLen(kvRow, (TDRowLenT)(TD_KV_ROW_HEAD_SIZE + sizeof(SColIdx) * nBoundCols));

  int32_t toffset = 0, kvOffset = 0;
  for (int i = 0; i < nCols; ++i) {
    SSchema *schema = pSchema + i;
    if ((spd->cols + i)->valStat == VAL_STAT_YES) {
      toffset = (spd->cols + i)->toffset;
      char *val = tdGetRowDataOfCol(dataRow, schema->type, toffset + TD_DATA_ROW_HEAD_SIZE);
      tdAppendKvColVal(kvRow, val, true, schema->colId, schema->type, kvOffset);
      kvOffset += sizeof(SColIdx);
    }
  }
}

// TODO: Move to tdataformat.h and refactor when STSchema available.
static FORCE_INLINE void convertSMemRow(SMemRow dest, SMemRow src, STableDataBlocks *pBlock) {
  STableMeta *        pTableMeta = pBlock->pTableMeta;
  STableComInfo       tinfo = tscGetTableInfo(pTableMeta);
  SSchema *           pSchema = tscGetTableSchema(pTableMeta);
  SParsedDataColInfo *spd = &pBlock->boundColumnInfo;

  ASSERT(dest != src);

  if (isDataRow(src)) {
    // TODO: Can we use pBlock -> numOfParam directly?
    ASSERT(spd->numOfBound > 0);
    convertToSKVRow(dest, src, pSchema, tinfo.numOfColumns, spd->numOfBound, spd);
  } else {
    convertToSDataRow(dest, src, pSchema, tinfo.numOfColumns, spd);
  }
}

#ifdef __cplusplus
}
#endif

#endif
