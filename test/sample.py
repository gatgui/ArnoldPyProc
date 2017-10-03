# Copyright (c) 2016 Gaetan Guidet
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

import arnold

arnold5 = (int(arnold.AiGetVersion()[0]) >= 5)

def Init(procName):
   proc = arnold.AiNodeLookUpByName(procName)
   if not proc:
      print("No such procedural: %s" % procName)
      return (0, None)

   attrs = {}

   it = arnold.AiNodeGetUserParamIterator(proc)
   
   while not arnold.AiUserParamIteratorFinished(it):
      param = arnold.AiUserParamIteratorGetNext(it)
      pname = arnold.AiUserParamGetName(param)
      pcat = arnold.AiUserParamGetCategory(param)
      if pcat == arnold.AI_USERDEF_CONSTANT:
         ptype = arnold.AiUserParamGetType(param)
         pval = None
         if ptype == arnold.AI_TYPE_BOOLEAN:
            pval = arnold.AiNodeGetBool(proc, pname)
         elif ptype == arnold.AI_TYPE_INT:
            pval = arnold.AiNodeGetInt(proc, pname)
         elif ptype == arnold.AI_TYPE_UINT:
            pval = arnold.AiNodeGetUInt(proc, pname)
         elif ptype == arnold.AI_TYPE_FLOAT:
            pval = arnold.AiNodeGetFlt(proc, pname)
         elif ptype == arnold.AI_TYPE_VECTOR:
            pval = arnold.AiNodeGetVec(proc, pname)
         elif ptype == arnold.AI_TYPE_RGB:
            pval = arnold.AiNodeGetRGB(proc, pname)
         elif ptype == arnold.AI_TYPE_RGBA:
            pval = arnold.AiNodeGetRGBA(proc, pname)
         elif ptype == arnold.AI_TYPE_STRING:
            pval = arnold.AiNodeGetStr(proc, pname)
         if arnold5:
            if ptype == arnold.AI_TYPE_VECTOR2:
               pval = arnold.AiNodeGetVec2(proc, pname)
         else:
            if ptype == arnold.AI_TYPE_POINT2:
               pval = arnold.AiNodeGetPnt2(proc, pname)
            elif ptype == arnold.AI_TYPE_POINT:
               pval = arnold.AiNodeGetPnt(proc, pname)
         if pval != None:
            attrs[pname] = (ptype, pval)
         else:
            print("Unsupported type (%d) for parameter \"%s\"" % (ptype, pname))
      else:
         print("Ignore non constant parameter \"%s\"" % pname)
   
   arnold.AiUserParamIteratorFinished(it)

   return (1, attrs)

def NumNodes(user_data):
   ptype, pval = user_data.get("type", (arnold.AI_TYPE_UNDEFINED, ""))
   if pval in ["sphere", "box", "cylinder"]:
      return 1
   else:
      return 0

def GetNode(user_data, i):
   ptype, pval = user_data.get("type")
   n = arnold.AiNode(pval)
   if n:
      name = "sample_%s" % pval
      arnold.AiNodeSetStr(n, "name", name)
      ne = arnold.AiNodeGetNodeEntry(n)
      for k, v in user_data.iteritems():
         if k == "type":
            continue
         ptype, pval = v
         pe = arnold.AiNodeEntryLookUpParameter(ne, k)
         if pe:
            if ptype == arnold.AI_TYPE_BOOLEAN:
               arnold.AiNodeSetBool(n, k, pval)
            elif ptype == arnold.AI_TYPE_INT:
               arnold.AiNodeSetInt(n, k, pval)
            elif ptype == arnold.AI_TYPE_UINT:
               arnold.AiNodeSetUInt(n, k, pval)
            elif ptype == arnold.AI_TYPE_FLOAT:
               arnold.AiNodeSetFlt(n, k, pval)
            elif ptype == arnold.AI_TYPE_VECTOR:
               arnold.AiNodeSetVec(n, k, pval.x, pval.y, pval.z)
            elif ptype == arnold.AI_TYPE_RGB:
               arnold.AiNodeSetRGB(n, k, pval.r, pval.g, pval.b)
            elif ptype == arnold.AI_TYPE_RGBA:
               arnold.AiNodeSetRGBA(n, k, pval.r, pval.g, pval.b, pval.a)
            elif ptype == arnold.AI_TYPE_STRING:
               arnold.AiNodeSetStr(n, k, pval)
            else:
               if arnold5:
                  if ptype == arnold.AI_TYPE_VECTOR2:
                     arnold.AiNodeSetVec2(n, k, pval.x, pval.y)
               else:
                  if ptype == arnold.AI_TYPE_POINT2:
                     arnold.AiNodeSetPnt2(n, k, pval.x, pval.y)
                  elif ptype == arnold.AI_TYPE_POINT:
                     arnold.AiNodeSetPnt(n, k, pval.x, pval.y, pval.z)
      return name
   else:
      return None

def Cleanup(user_data):
   return 1

