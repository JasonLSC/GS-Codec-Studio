#!/usr/bin/python3
#
# cmake_docutil.py
#

import pyhhi.build.app.cmkdocapp
import pyhhi.build.common.util


app = pyhhi.build.app.cmkdocapp.CMakeDocUtilApp()
pyhhi.build.common.util.exec_main_default_try(app)
