//
// Created by taylor-santos on 6/21/2022 at 15:51.
//

#pragma once

#if defined(KERNEL_PROGRAM)
#    if !defined(cl_float3)
#        define cl_float3 float3
#    endif
#    if !defined(cl_float)
#        define cl_float float
#    endif
#    if !defined(cl_uchar3)
#        define cl_uchar3 uchar3
#    endif
#else
#    define CL_TARGET_OPENCL_VERSION 200
#    include <CL/cl.h>
#endif

#pragma pack(push, 1)

typedef struct Sphere {
    cl_float3 center;
    cl_float  radius;
} Sphere;

typedef struct Box {
    cl_float3 min;
    cl_float3 max;
} Box;

typedef enum Type {
    SPHERE,
    BOX,
} Type;

typedef struct Object {
    Type type;
    union {
        Sphere sphere;
        Box    box;
    };
    cl_uchar3 color;
    cl_float  emission;
} Object;

#pragma pack(pop)
