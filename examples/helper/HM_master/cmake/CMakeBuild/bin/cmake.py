#!/usr/bin/python3
#
# cmake.py
#

import pyhhi.build.common.util
import pyhhi.build.app.cmk


app = pyhhi.build.app.cmk.CMakeLauncherApp()
pyhhi.build.common.util.exec_main_default_try(app)
