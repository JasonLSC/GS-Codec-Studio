#!/usr/bin/python3
#
# cmakebuild_update.py
#

import pyhhi.build.app.cmbldup
import pyhhi.build.common.util


app = pyhhi.build.app.cmbldup.CMakeBuildUpdateApp()
pyhhi.build.common.util.exec_main_default_try(app)
