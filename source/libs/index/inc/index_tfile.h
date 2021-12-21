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
#ifndef __INDEX_TFILE_H__
#define __INDEX_TFILE_H__

#include "index.h"
#include "indexInt.h"
#include "tlockfree.h"
#include "tskiplist.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct IndexTFile {
  T_REF_DECLARE() 
} IndexTFile;



IndexTFile *indexTFileCreate();


int indexTFileSearch(void *tfile, SIndexTermQuery *query, SArray *result);

#ifdef __cplusplus
}


#endif



#endif