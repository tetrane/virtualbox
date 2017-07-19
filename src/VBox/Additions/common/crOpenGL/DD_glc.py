from __future__ import print_function
print("""
/** @file
 * VBox OpenGL chromium functions header
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
""")
# Copyright (c) 2001, Stanford University
# All rights reserved.
#
# See the file LICENSE.txt for information on redistributing this software.

import sys

import apiutil

apiutil.CopyrightC()

print("""
/* DO NOT EDIT - THIS FILE GENERATED BY THE DD_gl.py SCRIPT */

#include "chromium.h"
#include "cr_string.h"
#include "cr_version.h"
#include "stub.h"
#include "dri_drv.h"
#include "cr_gl.h"
""")

commoncall_special = [
   "ArrayElement",
   "Begin",
   "CallList",
   "CallLists",
   "Color3f",
   "Color3fv",
   "Color4f",
   "Color4fv",
   "EdgeFlag",
   "End",
   "EvalCoord1f",
   "EvalCoord1fv",
   "EvalCoord2f",
   "EvalCoord2fv",
   "EvalPoint1",
   "EvalPoint2",
   "FogCoordfEXT",
   "FogCoordfvEXT",
   "Indexf",
   "Indexfv",
   "Materialfv",
   "MultiTexCoord1fARB",
   "MultiTexCoord1fvARB",
   "MultiTexCoord2fARB",
   "MultiTexCoord2fvARB",
   "MultiTexCoord3fARB",
   "MultiTexCoord3fvARB",
   "MultiTexCoord4fARB",
   "MultiTexCoord4fvARB",
   "Normal3f",
   "Normal3fv",
   "SecondaryColor3fEXT",
   "SecondaryColor3fvEXT",
   "TexCoord1f",
   "TexCoord1fv",
   "TexCoord2f",
   "TexCoord2fv",
   "TexCoord3f",
   "TexCoord3fv",
   "TexCoord4f",
   "TexCoord4fv",
   "Vertex2f",
   "Vertex2fv",
   "Vertex3f",
   "Vertex3fv",
   "Vertex4f",
   "Vertex4fv",
   "VertexAttrib1fNV",
   "VertexAttrib1fvNV",
   "VertexAttrib2fNV",
   "VertexAttrib2fvNV",
   "VertexAttrib3fNV",
   "VertexAttrib3fvNV",
   "VertexAttrib4fNV",
   "VertexAttrib4fvNV",
   "VertexAttrib1fARB",
   "VertexAttrib1fvARB",
   "VertexAttrib2fARB",
   "VertexAttrib2fvARB",
   "VertexAttrib3fARB",
   "VertexAttrib3fvARB",
   "VertexAttrib4fARB",
   "VertexAttrib4fvARB",
   "EvalMesh1",
   "EvalMesh2",
   "Rectf",
   "DrawArrays",
   "DrawElements",
   "DrawRangeElements"
]

keys = apiutil.GetDispatchedFunctions(sys.argv[1]+"/APIspec.txt")

for func_name in keys:
    if "Chromium" == apiutil.Category(func_name):
        continue
    if func_name == "BoundsInfoCR":
        continue

    return_type = apiutil.ReturnType(func_name)
    params = apiutil.Parameters(func_name)

    if func_name in commoncall_special:
        print("%s vboxDD_gl%s(%s)" % (return_type, func_name, apiutil.MakeDeclarationString(params) ))
    else:
        if apiutil.MakeDeclarationString(params)=="void":
            print("%s vboxDD_gl%s(GLcontext *ctx)" % (return_type, func_name ))
        else:
            print("%s vboxDD_gl%s(GLcontext *ctx, %s)" % (return_type, func_name, apiutil.MakeDeclarationString(params) ))
    print("{")

    if return_type != "void":
        print("\treturn ", end=' ')

    print("\tcr_gl%s(%s);" % (func_name, apiutil.MakeCallString(params)))
    print("}")
    print("")

