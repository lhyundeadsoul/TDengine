###################################################################
#           Copyright (c) 2016 by TAOS Technologies, Inc.
#                     All rights reserved.
#
#  This file is proprietary and confidential to TAOS Technologies.
#  No part of this file may be reproduced, stored, transmitted,
#  disclosed or used in any form or by any means other than as
#  expressly provided by the written permission from Jianhui Tao
#
###################################################################

# -*- coding: utf-8 -*-

from util.log import *
from util.cases import *
from util.sql import *

class TDTestCase:
    def init(self, conn, logSql):
        tdLog.debug("start to execute %s" % __file__)
        tdSql.init(conn.cursor(), logSql)

    def run(self):
        # TD-5903 show db.vgroups xxx. xxx is invalid content, but still returns results.
        # PR: 

        tdSql.execute("create database if not exists test_5903")
        tdSql.execute("show test_5903.vgroups")
        tdSql.error("show test_5903.vgroups xxx")
        #res = tdSql.query('show test_5903.vgroups', True)
        #expected_res = [(datetime.datetime(2020, 9, 13, 20, 26, 40), 'aaa', True, 127, 32767, 2147483647, 9223372036854775807, 11.12345027923584, 22.123456789, 'aaa', 'aaa')]
        #tdSql.checkEqual(res, expected_res)

    def stop(self):
        tdSql.close()
        tdLog.success("%s successfully executed" % __file__)


tdCases.addWindows(__file__, TDTestCase())
tdCases.addLinux(__file__, TDTestCase())
