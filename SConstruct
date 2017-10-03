import sys
import glob
import excons
from excons.tools import arnold
from excons.tools import python

env = excons.MakeBaseEnv()

cppflags = ""
if sys.platform != "win32":
    cppflags += " -Wno-unused-parameter"

verprefix = "/a%spy%s" % (arnold.Version(compat=True), python.Version())

prjs = [
  {"name": "pyproc",
   "prefix": "arnold" + verprefix,
   "type": "dynamicmodule",
   "ext": arnold.PluginExt(),
   "cppflags": cppflags,
   "srcs": ["src/main.cpp"],
   "custom": [arnold.Require, python.SoftRequire]
  }
]

excons.DeclareTargets(env, prjs)

excons.EcosystemDist(env, "pyproc.env", {"pyproc": verprefix})

Default(["pyproc"])

