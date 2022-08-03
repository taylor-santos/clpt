//
// Created by taylor-santos on 6/21/2022 at 15:51.
//

#pragma once

#define MAX_KD_DEPTH 6

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

#if !defined(KERNEL_PROGRAM)
#    define DEFAULT(...) = {__VA_ARGS__}
#else
#    define DEFAULT(...)
#endif

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
    cl_float3 albedo   DEFAULT(1, 1, 1);
    cl_float3 emission DEFAULT(0, 0, 0);

    float IOR DEFAULT(1.0f);

    float specular_chance    DEFAULT(0);
    float specular_roughness DEFAULT(0);
    cl_float3 specular_color DEFAULT(1, 1, 1);

    float refraction_chance         DEFAULT(0);
    float refraction_roughness      DEFAULT(0);
    cl_float3 refraction_absorption DEFAULT(0, 0, 0);
} Object;

typedef struct Light {
    cl_uint index;
} Light;

typedef struct KDTreeNode {
    union {
        cl_float split;           // If interior node, split location
        cl_uint  one_prim;        // If leaf with one primitive, index of that primitive
        cl_uint  prim_ids_offset; // If leaf with multiple primitives, offset into ids array
    };
    union {
        cl_uint flags;
        cl_uint nprims;
        cl_uint above_child;
    };
    cl_float3 lower_bound;
    cl_float3 upper_bound;
} KDTreeNode;

#pragma pack(pop)
