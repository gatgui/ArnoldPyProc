import sys
import glob
import excons
from excons.tools import arnold
from excons.tools import python

env = excons.MakeBaseEnv()

prjs = [
  {"name": "pyproc",
   "prefix": "arnold",
   "type": "dynamicmodule",
   "ext": arnold.PluginExt(),
   "srcs": ["src/main.cpp"],
   "custom": [arnold.Require, python.SoftRequire]
  }
]

excons.DeclareTargets(env, prjs)

excons.EcosystemDist(env, "pyproc.env", {"pyproc": ""})

Default(["pyproc"])

