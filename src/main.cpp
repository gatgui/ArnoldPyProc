/*
Copyright (c) 2016 Gaetan Guidet

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <Python.h>
#include <ai.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>

// ---

class PythonInterpreter
{
public:
  
  static PythonInterpreter& Begin()
  {
    if (!msInstance)
    {
      new PythonInterpreter();
    }
    return *msInstance;
  }
  
  static void End()
  {
    if (msInstance)
    {
      delete msInstance;
      msInstance = 0;
    }
  }
  
private:
  
  static void PrintPath(const char *p)
  {
#ifdef _WIN32
    int sep = ';';
#else
    int sep = ':';
#endif
    
    if (!p)
    {
      return;
    }
    
    std::string tmp = p;
    
    size_t p0 = 0;
    size_t p1 = tmp.find(sep, p0);
    
    while (p1 != std::string::npos)
    {
      std::string path = tmp.substr(p0, p1-p0);
      if (path.length() > 0)
      {
        AiMsgInfo("[pyproc]   %s", path.c_str());
      }
      p0 = p1 + 1;
      p1 = tmp.find(sep, p0);
    }
    
    std::string path = tmp.substr(p0);
    if (path.length() > 0)
    {
      AiMsgInfo("[pyproc]   %s", path.c_str());
    }
  }
  
private:
  
  PythonInterpreter()
    : mMainState(0)
    , mRestoreState(0)
    , mRunning(false)
  {
    char *pyproc_debug = getenv("PYPROC_DEBUG");
    int debug = 0;
    
    if (pyproc_debug && sscanf(pyproc_debug, "%d", &debug) == 1 && debug != 0)
    {
#ifdef _WIN32
      char *libpath = getenv("PATH");
#else
#  ifdef __APPLE__
      // This is not DYLD_LIBRARY_PATH, on OSX Python is used as a framework
      char *libpath = 0;
#  else
      char *libpath = getenv("LD_LIBRARY_PATH");
#  endif
#endif
      
      if (libpath)
      {
        AiMsgInfo("[pyproc] LIBPATH:");
        PrintPath(libpath);
      }
      
      char *pypath = getenv("PYTHONPATH");
      if (pypath)
      {
        AiMsgInfo("[pyproc] PYTHONPATH:");
        PrintPath(pypath);
      }
    }
    
    if (Py_IsInitialized() != 0)
    {
      AiMsgInfo("[pyproc] Python already initialized");
      
      if (PyEval_ThreadsInitialized() == 0)
      {
        AiMsgInfo("[pyproc] Initialize python threads");
        
        PyEval_InitThreads();
        
        PyThreadState_Swap(PyGILState_GetThisThreadState());
        PyEval_SaveThread();
      }
      else
      {
        if (_PyThreadState_Current)
        {
          mRestoreState = _PyThreadState_Current;
          PyEval_SaveThread();
        }
      }
      
      PyGILState_STATE gil = PyGILState_Ensure();
      
      setup();
      
      PyGILState_Release(gil);
    }
    else
    {
      AiMsgInfo("[pyproc] Initializing python");
      
      Py_SetProgramName((char*)"pyproc");
      
      Py_Initialize();
      
      PyEval_InitThreads();
      
      setup();
      
      mMainState = PyEval_SaveThread();
    }
    
    mRunning = true;

    msInstance = this;
  }
  
  ~PythonInterpreter()
  {
    if (mRunning)
    {
      if (mMainState)
      {
        AiMsgInfo("[pyproc] Finalize python");
        
        PyEval_RestoreThread(mMainState);
        
        Py_Finalize();
        
        mMainState = 0;
      }
      else if (mRestoreState)
      {
        PyEval_RestoreThread(mRestoreState);
        
        mRestoreState = 0;
      }
      
      mRunning = false;
    }
    
    msInstance = NULL;
  }
  
  void setup()
  {
    // This code was originally using sys.prefix
    // But on windows, sys.prefix doesn't necessarily matches the directory of 
    //   the actually used python DLL
    // What we want here is to be sure to use the binary modules shipped with
    //   the python DLL in use
    static const char *scr = "import sys, os\n\
sys.dont_write_bytecode = True\n\
if sys.platform == \"win32\":\n\
  dlls = os.path.join(os.path.split(os.path.dirname(os.__file__))[0], \"DLLs\")\n\
  if dlls in sys.path:\n\
    sys.path.remove(dlls)\n\
  sys.path.insert(0, dlls)\n";
       
    PyRun_SimpleString(scr);
  }
  
public:
  
  bool isRunning() const
  {
    return mRunning;
  }
  
private:
  
  PyThreadState *mMainState;
  PyThreadState *mRestoreState;
  bool mRunning;
  
  static PythonInterpreter *msInstance;
};

PythonInterpreter* PythonInterpreter::msInstance = NULL;

// ---

class PythonDso
{
public:
  
  PythonDso(AtNode *node)
    : mProcName("")
    , mScript("")
    , mModule(0)
    , mUserData(0)
    , mVerbose(false)
  {
    if (AiNodeLookUpUserParameter(node, "verbose") != NULL)
    {
      mVerbose = AiNodeGetBool(node, "verbose");
    }
    
    mProcName = AiNodeGetStr(node, "name");
    
    std::string script = AiNodeGetStr(node, "data");
    
    struct stat st;
    
    if ((stat(script.c_str(), &st) != 0) || ((st.st_mode & S_IFREG) == 0))
    {
      if (mVerbose)
      {
        AiMsgInfo("[pyproc] Search python procedural in options.procedural_searchpath...");
      }
      
      // look in procedural search path
      AtNode *opts = AiUniverseGetOptions();
      
      if (!opts)
      {
        AiMsgWarning("[pyproc] No 'options' node");
      }
      else
      {
        std::string procpath = AiNodeGetStr(opts, "procedural_searchpath");
        
        if (!findInPath(procpath, script, mScript))
        {
          AiMsgWarning("[pyproc] Python procedural '%s' not found in path", script.c_str());
          mScript = "";
        }
      }
    }
    else
    {
      mScript = script;
    }
    
    if (mScript.length() > 0)
    {
      size_t p0, p1;
      
#ifdef _WIN32
      char dirSepFrom = '/';
      char dirSepTo = '\\';
#else
      char dirSepFrom = '\\';
      char dirSepTo = '/';
#endif
      
      p0 = 0;
      p1 = mScript.find(dirSepFrom, p0);
      
      while (p1 != std::string::npos)
      {
        mScript[p1] = dirSepTo;
        p0 = p1 + 1;
        p1 = mScript.find(dirSepFrom, p0);
      }
      
      if (mVerbose)
      {
        AiMsgInfo("[pyproc] Resolved script path \"%s\"", mScript.c_str());
      }
    }
  }
  
  ~PythonDso()
  {
  }
  
  bool valid() const
  {
    return (mScript.length() > 0);
  }
  
  int init()
  {
    PyGILState_STATE gil = PyGILState_Ensure();
    
    int rv = 0;
    
    // Derive python module name
    std::string modname = "pyproc_";
    
    size_t p0 = mScript.find_last_of("\\/");
    
    if (p0 != std::string::npos)
    {
      modname += mScript.substr(p0+1);
    }
    else
    {
      modname += mScript;
    }
    
    p0 = modname.find('.');
    
    if (p0 != std::string::npos)
    {
      modname = modname.substr(0, p0);
    }
    
    PyObject *pyimp = PyImport_ImportModule("imp");
    
    if (pyimp == NULL)
    {
      AiMsgError("[pyproc] Could not import imp module");
      PyErr_Print();
      PyErr_Clear();
    }
    else
    {
      PyObject *pyload = PyObject_GetAttrString(pyimp, "load_source");
      
      if (pyload == NULL)
      {
        AiMsgError("[pyproc] No \"load_source\" function in imp module");
        PyErr_Print();
        PyErr_Clear();
        Py_DECREF(pyimp);
      }
      else
      {
        if (mVerbose)
        {
          AiMsgInfo("[pyproc] Loading procedural module");
        }
        
        mModule = PyObject_CallFunction(pyload, (char*)"ss", modname.c_str(), mScript.c_str());
        
        if (mModule == NULL)
        {
          AiMsgError("[pyproc] Failed to import procedural python module");
          PyErr_Print();
          PyErr_Clear();
        }
        else
        {
          PyObject *func = PyObject_GetAttrString(mModule, "Init");
    
          if (func)
          {
            PyObject *pyrv = PyObject_CallFunction(func, (char*)"s", mProcName.c_str());
            
            if (pyrv)
            {
              if (PyTuple_Check(pyrv) && PyTuple_Size(pyrv) == 2)
              {
                mUserData = PyTuple_GetItem(pyrv, 1);
                
                Py_INCREF(mUserData);
                
                rv = PyInt_AsLong(PyTuple_GetItem(pyrv, 0));
                
                if (rv == -1 && PyErr_Occurred() != NULL)
                {
                  AiMsgError("[pyproc] Invalid return value for \"Init\" function in module \"%s\"", mScript.c_str());
                  PyErr_Print();
                  PyErr_Clear();
                  
                  rv = 0;
                }
              }
              else
              {
                AiMsgError("[pyproc] Invalid return value for \"Init\" function in module \"%s\"", mScript.c_str());
              }
              
              Py_DECREF(pyrv);
            }
            else
            {
              AiMsgError("[pyproc] \"Init\" function failed in module \"%s\"", mScript.c_str());
              PyErr_Print();
              PyErr_Clear();
            }
            
            Py_DECREF(func);
          }
          else
          {
            AiMsgError("[pyproc] No \"Init\" function in module \"%s\"", mScript.c_str());
            PyErr_Clear();
          }
        }
        
        Py_DECREF(pyload);
      }
      
      Py_DECREF(pyimp);
    }
    
    PyGILState_Release(gil);
    
    return rv;
  }
  
  int numNodes()
  {
    PyGILState_STATE gil = PyGILState_Ensure();
    
    int rv = 0;
    
    PyObject *func = PyObject_GetAttrString(mModule, "NumNodes");
    
    if (func)
    {
      PyObject *pyrv = PyObject_CallFunction(func, (char*)"O", mUserData);
      
      if (pyrv)
      {
        rv = PyInt_AsLong(pyrv);
        
        if (rv == -1 && PyErr_Occurred() != NULL)
        {
          AiMsgError("[pyproc] Invalid return value for \"NumNodes\" function in module \"%s\"", mScript.c_str());
          PyErr_Print();
          PyErr_Clear();
          rv = 0;
        }
        
        Py_DECREF(pyrv);
      }
      else
      {
        AiMsgError("[pyproc] \"NumNodes\" function failed in module \"%s\"", mScript.c_str());
        PyErr_Print();
        PyErr_Clear();
      }
      
      Py_DECREF(func);
    }
    else
    {
      AiMsgError("[pyproc] No \"NumNodes\" function in module \"%s\"", mScript.c_str());
      PyErr_Clear();
    }
    
    PyGILState_Release(gil);
    
    return rv;
  }
  
  AtNode* getNode(int i)
  {
    PyGILState_STATE gil = PyGILState_Ensure();
    
    AtNode *rv = 0;
    
    PyObject *func = PyObject_GetAttrString(mModule, "GetNode");
    
    if (func)
    {
      PyObject *pyrv = PyObject_CallFunction(func, (char*)"Oi", mUserData, i);
      
      if (pyrv)
      {
        if (!PyString_Check(pyrv))
        {
          AiMsgError("[pyproc] Invalid return value for \"GetNode\" function in module \"%s\"", mScript.c_str());
        }
        
        const char *nodeName = PyString_AsString(pyrv);
        
        rv = AiNodeLookUpByName(nodeName);
        
        if (rv == NULL)
        {
          AiMsgError("[pyproc] Invalid node name \"%s\" return by \"GetNode\" function in modulde \"%s\"", nodeName, mScript.c_str());
        }
        
        Py_DECREF(pyrv);
      }
      else
      {
        AiMsgError("[pyproc] \"GetNode\" function failed in module \"%s\"", mScript.c_str());
        PyErr_Print();
        PyErr_Clear();
      }
      
      Py_DECREF(func);
    }
    else
    {
      AiMsgError("[pyproc] No \"GetNode\" function in module \"%s\"", mScript.c_str());
      PyErr_Clear();
    }
    
    PyGILState_Release(gil);
    
    return rv;
  }
  
  int cleanup()
  {
    PyGILState_STATE gil = PyGILState_Ensure();
    
    int rv = 0;
    
    PyObject *func = PyObject_GetAttrString(mModule, "Cleanup");
    
    if (func)
    {
      PyObject *pyrv = PyObject_CallFunction(func, (char*)"O", mUserData);
      
      if (pyrv)
      {
        rv = PyInt_AsLong(pyrv);
        
        if (rv == -1 && PyErr_Occurred() != NULL)
        {
          AiMsgError("[pyproc] Invalid return value for \"Cleanup\" function in module \"%s\"", mScript.c_str());
          PyErr_Print();
          PyErr_Clear();
          rv = 0;
        }
        
        Py_DECREF(pyrv);
      }
      else
      {
        AiMsgError("[pyproc] \"Cleanup\" function failed in module \"%s\"", mScript.c_str());
        PyErr_Print();
        PyErr_Clear();
      }
      
      Py_DECREF(func);
    }
    else
    {
      AiMsgError("[pyproc] No \"Cleanup\" function in module \"%s\"", mScript.c_str());
      PyErr_Clear();
    }
    
    Py_DECREF(mUserData);
    Py_DECREF(mModule);
    
    mUserData = 0;
    mModule = 0;
    
    PyGILState_Release(gil);
    
    return rv;
  }
  
private:
  
  bool findInPath(const std::string &procpath, const std::string &script, std::string &path)
  {
#ifdef _WIN32
    char sep = ';';
#else
    char sep = ':';
#endif
    
    struct stat st;
    bool found = false;

    size_t p0 = 0;
    size_t p1 = procpath.find(sep, p0);

    while (!found && p1 != std::string::npos)
    {
      path = procpath.substr(p0, p1-p0);
      
      if (path.length() > 0)
      {
        if (path[0] == '[' && path[path.length()-1] == ']')
        {
          char *env = getenv(path.substr(1, path.length()-2).c_str());
          if (env) found = findInPath(env, script, path);
        }
        else
        {
          path += "/" + script;
          found = ((stat(path.c_str(), &st) == 0) && ((st.st_mode & S_IFREG) != 0));
        }
      }
      
      p0 = p1 + 1;
      p1 = procpath.find(sep, p0);
    }
    
    if (!found)
    {
      path = procpath.substr(p0);
      
      if (path.length() > 0)
      {
        if (path[0] == '[' && path[path.length()-1] == ']')
        {
          char *env = getenv(path.substr(1, path.length()-2).c_str());
          if (env) found = findInPath(env, script, path);
        }
        else
        {
          path += "/" + script;
          found = ((stat(path.c_str(), &st) == 0) && ((st.st_mode & S_IFREG) != 0));
        }
      }
    }
    
    return found;
  }

private:
  
  std::string mProcName;
  std::string mScript;
  PyObject *mModule;
  PyObject *mUserData;
  bool mVerbose;
};


// ---

int PyDSOInit(AtNode *node, void **user_ptr)
{
  if (!Py_IsInitialized())
  {
    AiMsgWarning("[pyproc] Init: Python not initialized");
    return 0;
  }
  
  PythonDso *dso = new PythonDso(node);
  
  if (dso->valid())
  {
    *user_ptr = (void*)dso;
    
    return dso->init();
  }
  else
  {
    return 0;
  }
}

int PyDSONumNodes(void *user_ptr)
{
  if (!Py_IsInitialized())
  {
    AiMsgWarning("[pyproc] NumNodes: Python not initialized");
    return 0;
  }
  
  PythonDso *dso = (PythonDso*) user_ptr;
  
  return dso->numNodes();
}

AtNode* PyDSOGetNode(void *user_ptr, int i)
{
  if (!Py_IsInitialized())
  {
    AiMsgWarning("[pyproc] GetNode: Python not initialized");
    return 0;
  }
  
  PythonDso *dso = (PythonDso*) user_ptr;
  
  return dso->getNode(i);
}

int PyDSOCleanup(void *user_ptr)
{
  if (!Py_IsInitialized())
  {
    AiMsgWarning("[pyproc] Cleanup: Python not initialized");
    return 0;
  }
  
  PythonDso *dso = (PythonDso*) user_ptr;
  
  int rv = dso->cleanup();
  
  delete dso;
  
  return rv;
}

proc_loader
{
  vtable->Init = PyDSOInit;
  vtable->Cleanup = PyDSOCleanup;
  vtable->NumNodes = PyDSONumNodes;
  vtable->GetNode = PyDSOGetNode;
  strcpy(vtable->version, AI_VERSION);
  return true;
}

// ---

#ifdef _WIN32

#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID reserved)
{
  switch (reason)
  {
  case DLL_PROCESS_ATTACH:
    PythonInterpreter::Begin();
    break;
    
  case DLL_PROCESS_DETACH:
    PythonInterpreter::End();
    
  default:
    break;
  }
  
  return TRUE;
}

#else

__attribute__((constructor)) void _PyProcLoad(void)
{
  PythonInterpreter::Begin();
}

__attribute__((destructor)) void _PyProcUnload(void)
{
  PythonInterpreter::End();
}

#endif

