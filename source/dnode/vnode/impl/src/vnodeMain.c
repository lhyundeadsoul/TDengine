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

#include "vnodeDef.h"

SVnode *vnodeOpen(const char *path, const SVnodeOptions *pVnodeOptions) {
  SVnode *pVnode = NULL;
  /* TODO */
  return pVnode;
}

void vnodeCloee(SVnode *pVnode) { /* TODO */
}

void vnodeDestroy(const char *path) { taosRemoveDir(path); }