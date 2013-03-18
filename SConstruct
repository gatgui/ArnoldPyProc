import os
import sys
from distutils import sysconfig

arnoldBase = ARGUMENTS.get("arnold-prefix", None)
debug = int(ARGUMENTS.get("debug", "0"))
mscver = ARGUMENTS.get("mscver", "9.0")

if not arnoldBase:
  print("Please specify arnold prefix using arnold-prefix=")
  sys.exit(-1)

if not os.path.isdir(arnoldBase):
  print("Invalid arnold prefix \"%s\": Not a directory" % arnoldBase)
  sys.exit(-1)

arnoldInc = arnoldBase + "/include"
if not os.path.isdir(arnoldInc):
  print("Invalid arnold prefix \"%s\": No include directory" % arnoldBase)
  sys.exit(-1)

if sys.platform == "win32":
  arnoldLib = arnoldBase + "/lib"
else:
  arnoldLib = arnoldBase + "/bin"
if not os.path.isdir(arnoldLib):
  print("Invalid arnold prefix \"%s\": No lib directory" % arnoldBase)
  sys.exit(-1)


if sys.platform == "win32":
  env = Environment(MSVC_VERSION=mscver)
  target = "agPyProc.dll"
  env.Append(LIBPATH=[sysconfig.PREFIX + "\\libs"])
  env.Append(LIBS=["python%s" % sysconfig.get_python_version().replace(".", "")])
  ccflags = " /MD /GR /EHsc "
  if debug:
    env.Append(CCFLAGS=ccflags + "/Od /Zi")
    env.Append(LINKFLAGS=" /debug /incremental:yes /opt:noref /opt:noicf")
  else:
    env.Append(CCFLAGS=ccflags + "/O2")
    env.Append(LINKFLAGS=" /release /incremental:no /opt:ref")

elif sys.platform == "darwin":
  env = Environment()
  target = "agPyProc.dylib"
  env["SHLIBPREFIX"] = ""
  env.Append(LINKFLAGS=" -F%s -framework %s" %
                        (sysconfig.get_config_var("PYTHONFRAMEWORKPREFIX"),
                         sysconfig.get_config_var("PYTHONFRAMEWORK")))
  ccflags = " -Wall "
  if debug:
    env.Append(CCFLAGS=ccflags + "-O0 -ggdb")
  else:
    env.Append(CCFLAGS=ccflags + "-O2")

else:
  env = Environment()
  target = "agPyProc.so"
  env["SHLIBPREFIX"] = ""
  env.Append(LIBS=["python%s" % sysconfig.get_python_version()])
  ccflags = " -Wall "
  if debug:
    env.Append(CCFLAGS=ccflags + "-O0 -ggdb")
  else:
    env.Append(CCFLAGS=ccflags + "-O2")


env.Append(CPPPATH=[arnoldInc, sysconfig.get_python_inc()])
env.Append(LIBPATH=[arnoldLib])
env.Append(LIBS=["ai"])

agPyProc = env.SharedLibrary(target, [env.SharedObject("agPyProc.cpp")])
