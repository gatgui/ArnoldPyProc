import sys
import glob
import excons
from excons.tools import arnold
from excons.tools import python

env = excons.MakeBaseEnv()

prjs = [
  {"name": "agPyProc",
   "type": "dynamicmodule",
   "ext": arnold.PluginExt(),
   "srcs": ["agPyProc.cpp"],
   "custom": [arnold.Require, python.SoftRequire]
  }
]

excons.DeclareTargets(env, prjs)
