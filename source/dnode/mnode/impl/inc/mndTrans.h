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

#ifndef _TD_MND_TRANS_H_
#define _TD_MND_TRANS_H_

#include "mndInt.h"

#ifdef __cplusplus
extern "C" {
#endif

int32_t mndInitTrans(SMnode *pMnode);
void    mndCleanupTrans(SMnode *pMnode);

STrans *mndTransCreate(SMnode *pMnode, ETrnPolicy policy, void *rpcHandle);
void    mndTransDrop(STrans *pTrans);
int32_t mndTransAppendRedolog(STrans *pTrans, SSdbRaw *pRaw);
int32_t mndTransAppendUndolog(STrans *pTrans, SSdbRaw *pRaw);
int32_t mndTransAppendCommitlog(STrans *pTrans, SSdbRaw *pRaw);
int32_t mndTransAppendRedoAction(STrans *pTrans, SEpSet *, void *pMsg);
int32_t mndTransAppendUndoAction(STrans *pTrans, SEpSet *, void *pMsg);

int32_t mndTransPrepare(STrans *pTrans, int32_t (*syncfp)(SSdbRaw *pRaw, void *pData));
int32_t mndTransApply(SMnode *pMnode, SSdbRaw *pRaw, void *pData, int32_t code);
int32_t mndTransExecute(SSdb *pSdb, int32_t tranId);

SSdbRaw *mndTransActionEncode(STrans *pTrans);
SSdbRow *mndTransActionDecode(SSdbRaw *pRaw);

#ifdef __cplusplus
}
#endif

#endif /*_TD_MND_TRANS_H_*/