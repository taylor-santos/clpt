#define KERNEL_PROGRAM
#define HAVE_DOUBLE

#include "tinymt64.clh"
#include "cl_struct.h"

#undef KERNEL_PROGRAM

float3
mul(float4 *M, float3 X) {
    return (float3){
               dot(M[0].xyz, X) + M[0].w,
               dot(M[1].xyz, X) + M[1].w,
               dot(M[2].xyz, X) + M[2].w,
           } /
           (dot(M[3].xyz, X) + M[3].w);
}

bool
ray_sphere_intersect(float3 origin, float3 dir, float3 center, float radius, float *t) {
    float3 m = origin - center;
    float  b = dot(m, dir);
    float  c = dot(m, m) - radius * radius;
    if (c > 0.0f && b > 0.0f) return false;
    float d = b * b - c;

    if (d < 0.0f) return false;

    *t = -b - sqrt(d);
    // if (*t < 0.0f) *t = 0.0f;
    return true;
}

bool
ray_box_intersect(
    float3 origin,
    float3 inv_dir,
    float3 box_min,
    float3 box_max,
    float *t,
    int   *norm) {
    // https://tavianator.com/cgit/dimension.git/tree/libdimension/bvh/bvh.c#n196
    float tx1 = (box_min.x - origin.x) * inv_dir.x;
    float tx2 = (box_max.x - origin.x) * inv_dir.x;

    int   n    = 0;
    float tmin = tx1;
    if (tx2 < tmin) {
        tmin = tx2;
        n    = 1;
    }
    float tmax = max(tx1, tx2);

    float ty1 = (box_min.y - origin.y) * inv_dir.y;
    float ty2 = (box_max.y - origin.y) * inv_dir.y;

    if (ty1 <= ty2 && tmin < ty1) {
        tmin = ty1;
        n    = 2;
    } else if (ty2 < ty1 && tmin < ty2) {
        tmin = ty2;
        n    = 3;
    }
    tmax = min(tmax, max(ty1, ty2));

    float tz1 = (box_min.z - origin.z) * inv_dir.z;
    float tz2 = (box_max.z - origin.z) * inv_dir.z;

    if (tz1 <= tz2 && tmin < tz1) {
        tmin = tz1;
        n    = 4;
    } else if (tz2 < tz1 && tmin < tz2) {
        tmin = tz2;
        n    = 5;
    }
    tmax = min(tmax, max(tz1, tz2));

    *t    = tmin;
    *norm = n;

    return tmax >= max(0.0, tmin);
}

float3
snell(float3 s, float3 n, float n1, float n2) {
    float ns = dot(n, s);
    return (n1 * s - n * (n1 * ns + sqrt(n2 * n2 + n1 * n1 * (ns * ns - 1)))) / n2;
}

/*
float3
random_hemisphere(float3 n, tinymt64wp_t *rand, float angle) {
    // A random point on a unit sphere has a uniformly distributed z coordinate
    float ct = cos(angle);
    float z  = ct + (1 - ct) * tinymt64_double01(rand);
    float p  = tinymt64_double01(rand) * 2 * M_PI;

    float3 v = {
        n.x * z + (sqrt(1 - pow(z, 2)) *
                   ((pow(n.y, 2) + n.z + pow(n.z, 2)) * cos(p) - n.x * n.y * sin(p))) /
                      (1 + n.z),
        (-((n.y * n.z * sqrt((-1 + pow(n.y, 2) + pow(n.z, 2)) / (-1 + pow(z, 2))) *
            (-1 + pow(z, 2)) * (n.x * cos(p) + n.y * sin(p))) /
           sign(n.x)) +
         n.x * (-(n.y * (-1 + pow(n.z, 2)) * z) +
                n.x * sqrt(1 - pow(z, 2)) * (-(n.y * cos(p)) + n.x * sin(p)))) /
            (n.x - n.x * pow(n.z, 2)),
        n.z * z - sqrt(1 - pow(z, 2)) * (n.x * cos(p) + n.y * sin(p))};
    return v;
}
 */
float3
random_hemisphere(float3 n, tinymt64wp_t *rand) {
    float  z = -1 + tinymt64_double01(rand) * 2;
    float  p = tinymt64_double01(rand) * 2 * M_PI;
    float  r = sqrt(1 - z * z);
    float3 d = {r * cos(p), r * sin(p), z};
    if (dot(d, n) < 0) d = -d;
    return d;
}

typedef struct {
    float3 pos;
    float3 dir;
} refract_result;

refract_result
sphere_refract(float3 c, float r, float3 n, float3 d, float n1_sqr, float n2_sqr) {
    d        = snell(d, n, sqrt(n1_sqr), sqrt(n2_sqr));
    float  t = -2 * r * dot(n, d) / dot(d, d);
    float3 p = c + n * r + d * t;
    //    n = normalize(p - c);
    //    d = snell(d, -n, sqrt(n2_sqr), sqrt(n1_sqr));
    return (refract_result){p, d};
}

float
FresnelReflectAmount(float n1, float n2, float3 normal, float3 incident, float f0, float f90) {
    // Schlick approximation
    float r0 = (n1 - n2) / (n1 + n2);
    r0 *= r0;
    float cosX = -dot(normal, incident);
    if (n1 > n2) {
        float n     = n1 / n2;
        float sinT2 = n * n * (1.0 - cosX * cosX);
        // Total internal reflection
        if (sinT2 > 1.0) return f90;
        cosX = sqrt(1.0 - sinT2);
    }
    float x   = 1.0 - cosX;
    float ret = r0 + (1.0 - r0) * x * x * x * x * x;

    // adjust reflect multiplier for object reflectivity
    return mix(f0, f90, ret);
}

float3
trace_path(
    float3                       pos,
    float3                       dir,
    int                          depth,
    int                          last_hit_index,
    tinymt64wp_t                *rand,
    __global __read_only Object *objects,
    __read_only uint             object_count,
    bool                         debug) {
    if (debug) printf("depth %d\n", depth);
    if (depth <= 0) {
        return 0;
    }

    bool  did_hit = false;
    float min_t;
    int   hit_index;
    int   hit_side;

    for (int i = 0; i < object_count; i++) {
        if (i == last_hit_index) {
            continue; // Don't re-test the surface we're reflecting off of
        }

        float t;
        bool  hit;
        int   side = -1;

        switch (objects[i].type) {
            case SPHERE:
                hit = ray_sphere_intersect(
                    pos,
                    dir,
                    objects[i].sphere.center,
                    objects[i].sphere.radius,
                    &t);
                break;
            case BOX:
                hit = ray_box_intersect(
                    pos,
                    1 / dir,
                    objects[i].box.min,
                    objects[i].box.max,
                    &t,
                    &side);
                break;
            default: printf("Unknown object type %d at index %d\n", objects[i].type, i); return 0;
        }

        if (hit && (!did_hit || t < min_t)) {
            did_hit   = true;
            min_t     = t;
            hit_index = i;
            hit_side  = side;
        }
    }
    if (!did_hit) {
        if (debug) printf("No object hit at depth %d\n", depth);
        return 0;
    }

    if (debug) printf("Hit %d %d\n", hit_index, objects[hit_index].type);

    float3 hit_color = convert_float3(objects[hit_index].color) / 255.0;

    float3 emission = 0;
    if (objects[hit_index].emission > 0) {
        emission = objects[hit_index].emission * hit_color;
    }

    pos += dir * min_t;

    float3 n;
    switch (objects[hit_index].type) {
        case SPHERE: n = normalize(pos - objects[hit_index].sphere.center); break;
        case BOX:
            switch (hit_side) {
                case 0: n = (float3)(-1, 0, 0); break;
                case 1: n = (float3)(1, 0, 0); break;
                case 2: n = (float3)(0, -1, 0); break;
                case 3: n = (float3)(0, 1, 0); break;
                case 4: n = (float3)(0, 0, -1); break;
                case 5: n = (float3)(0, 0, 1); break;
            }
            break;
    }
    dir              = random_hemisphere(n, rand);
    float  p         = 1 / (2 * M_PI);
    float  cos_theta = dot(dir, n);
    float3 BRDF      = hit_color / M_PI;
    return emission +
           BRDF * cos_theta / p *
               trace_path(pos, dir, depth - 1, hit_index, rand, objects, object_count, debug);
    /*
    bool did_hit = false;
    int hit_index;
    int hit_side;
    float min_t;

    for (int i = 0; i < object_count; i++) {
        if (debug) printf("Checking %d %d\n", i, objects[i].type);
        if (last_hit_index != -1 && i == last_hit_index) {
            continue; // Don't re-test the surface we're reflecting off of
        }
        float t;
        bool hit;
        int side;

        switch (objects[i].type) {
            case SPHERE:
                hit = ray_sphere_intersect(
                    pos,
                    dir,
                    objects[i].sphere.center,
                    objects[i].sphere.radius,
                    &t);
                break;
            case BOX:
                hit = ray_box_intersect(
                    pos,
                    1 / dir,
                    objects[i].box.min,
                    objects[i].box.max,
                    &t,
                    &side);
                break;
        }
        if (hit && (!did_hit || t < min_t)) {
            min_t   = t;
            did_hit = true;
            hit_index = i;
            hit_side = side;
        }
    }
    if (did_hit) {
        pos += dir * min_t;
        float3 n;
        switch (objects[hit_index].type) {
            case SPHERE:
                n = normalize(pos - objects[hit_index].sphere.center);
                break;
            case BOX:
                switch(hit_side) {
                    case 0: n = (float3)(-1,  0,  0); break;
                    case 1: n = (float3)( 1,  0,  0); break;
                    case 2: n = (float3)( 0, -1,  0); break;
                    case 3: n = (float3)( 0,  1,  0); break;
                    case 4: n = (float3)( 0,  0, -1); break;
                    case 5: n = (float3)( 0,  0,  1); break;
                }
                break;
        }
        dir = random_hemisphere(n, rand, M_PI);
        float p = 1 / (2*M_PI);
        float cos_theta = dot(dir, n);
        float3 hit_color = convert_float3(objects[hit_index].color) / 255.0;
        float3 BRDF = hit_color / M_PI;

        return trace_path(pos, dir, hit_index, depth-1, rand, objects, object_count, debug);
//        return hit_color * objects[hit_index].emission + (BRDF * incoming * cos_theta / p);
        // dir -= 2 * dot(dir, n) * n;
    }
    return 0;
     */
}

void kernel
my_kernel(
    __write_only image2d_t       output,
    __global __read_only float4 *camera,
    __global __read_only ulong  *seed,
    __read_only int              input,
    __read_only ulong            noise,
    __global __read_only Object *objects,
    __read_only uint             object_count) {
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int2 res   = get_image_dim(output);

    bool debug = false; // all(coord == res / 2);

    /*
    if (debug) printf("START\n");

    if (debug) {
        printf("Show[");
        for (int i = 0; i < object_count; i++) {
            if (i) printf(",");
            printf("Graphics3D[");
            switch(objects[i].type) {
                case SPHERE:
                    printf("Sphere[{");
                    printf("%v3f", objects[i].sphere.center);
                    printf("},%f]", objects[i].sphere.radius);
                    break;
                case BOX:
                    printf("Cuboid[{");
                    printf("%v3f", objects[i].box.min);
                    printf("},{");
                    printf("%v3f", objects[i].box.max);
                    printf("}]");
                    break;
            }
            printf("]");
        }
    }
     */

    tinymt64wp_t tiny = {0};
    tinymt64_init(&tiny, seed[coord.x + coord.y * res.x] + noise);

    float3 color   = 0;
    int    samples = 0;
    do {
        float3 pos, dir;
        {
            float2 sample = (float2){
                (float)coord.x + tinymt64_double01(&tiny),
                (float)coord.y + tinymt64_double01(&tiny),
            };
            float2 rat = 2 * (sample / convert_float2(res)) - 1;
            float3 fcp = mul(camera, (float3)(rat, 1.0f));

            pos = (float3){camera[0].z, camera[1].z, camera[2].z} / camera[3].z;
            dir = normalize((fcp - pos).xyz);
        }

        /*
        double hue = tinymt64_double01(&tiny);

        float3 ray_color = 1;

        if (debug) {
            printf(",Graphics3D[{Hue[%f],Line[{{", hue);
            printf("%v3f", pos);
            printf("}");
        }

        bool did_hit;
        int last_hit_index = -1;
        do {
            did_hit = false;

            float min_t;
            int   hit_index;
            int   hit_side;

            for (int k = 0; k < object_count; k++) {
                if (last_hit_index != -1 && k == last_hit_index) {
                   continue; // Don't re-test the surface we're reflecting off of
                }
                float t;
                bool  hit = false;
                int side;
                switch (objects[k].type) {
                    case SPHERE:
                        hit = ray_sphere_intersect(
                            pos,
                            dir,
                            objects[k].sphere.center,
                            objects[k].sphere.radius,
                            &t);
                        break;
                    case BOX:
                        hit = ray_box_intersect(
                            pos,
                            1 / dir,
                            objects[k].box.min,
                            objects[k].box.max,
                            &t,
                            &side);
                        break;
                }
                if (hit && (!did_hit || t < min_t)) {
                    min_t   = t;
                    did_hit = true;
                    hit_index = k;
                    hit_side = side;
                }
            }
            if (did_hit) {
                last_hit_index = hit_index;
                pos += dir * min_t;
                if (debug) {
                    printf(",{");
                    printf("%v3f", pos);
                    printf("}");
                }
                float3 n;
                switch (objects[hit_index].type) {
                    case SPHERE:
                        n = normalize(pos - objects[hit_index].sphere.center);
                        break;
                    case BOX:
                        switch(hit_side) {
                            case 0: n = (float3)(-1,  0,  0); break;
                            case 1: n = (float3)( 1,  0,  0); break;
                            case 2: n = (float3)( 0, -1,  0); break;
                            case 3: n = (float3)( 0,  1,  0); break;
                            case 4: n = (float3)( 0,  0, -1); break;
                            case 5: n = (float3)( 0,  0,  1); break;
                        }
                        break;
                }
                dir -= 2 * dot(dir, n) * n;
                pos += dir *0.0001;

                float3 hit_color = convert_float3(objects[hit_index].color) / 255.0;
                ray_color *= hit_color;
            }
        } while (did_hit);
        if (debug) {
            printf("}]}],");
            printf("Graphics3D[{Hue[%f],HalfLine[{", hue);
            printf("%v3f", pos);
            printf("},{");
            printf("%v3f", dir);
            printf("}]}]");
        }
         */
        /*
        if (ray_sphere_intersect(pos, dir, center, r, &t)) {
            if (debug) {
                printf(",\n");
                printf(" Graphics3D[{\n");
                printf("  Hue[%f],\n", hue);
                printf("  Line[{\n");
                printf("   {%f, %f, %f},\n", pos.x, pos.y, pos.z);
                float3 new_pos = pos + dir*t;
                printf("   {%f, %f, %f}\n", new_pos.x, new_pos.y, new_pos.z);
                printf("  }]\n");
                printf(" }]\n");
            }

            pos += dir * t;
            float3 n = normalize(pos - center);
            float n1 = 1, n2 = 1 + (input/10.0f);
            bool in = false;
            int iter = 0;

            do {
                float3 rand_n = n;
                float prob = FresnelReflectAmount(n1, n2, rand_n, dir, 0, 1);
                bool is_reflection = tinymt64_double01(&tiny) <= prob;
                if (is_reflection) {
                    dir -= 2 * dot(dir, rand_n) * rand_n;
                } else {
                    dir = snell(dir, rand_n, n1, n2);
                    n = -n;
                    {
                        float tmp = n1;
                        n1 = n2;
                        n2 = tmp;
                    }
                    in = !in;
                }

                if (in) {
                    float t = 2 * r * dot(n, dir) / dot(dir, dir);

                    if (debug) {
                        printf(",\n");
                        printf(" Graphics3D[{\n");
                        printf("  Hue[%f],\n", hue);
                        printf("  Line[{\n");
                        printf("   {%f, %f, %f},\n", pos.x, pos.y, pos.z);
                    }

                    pos += dir * t;
                    n = normalize(center - pos);

                    if (debug) {
                        printf("   {%f, %f, %f}\n", pos.x, pos.y, pos.z);
                        printf("  }]\n");
                        printf(" }]\n");
                    }
                } else {
                    if (debug) {
                        printf(",\n");
                        printf(" Graphics3D[{\n");
                        printf("  Hue[%f],\n", hue);
                        printf("  HalfLine[\n");
                        printf("   {%f, %f, %f},\n", pos.x, pos.y, pos.z);
                        printf("   {%f, %f, %f}\n", dir.x, dir.y, dir.z);
                        printf("  ]\n");
                        printf(" }]\n");
                    }
                }

                iter++;
            } while(in && iter < 10);
        }
         */
        /*
        float a = atan2(dir.z, dir.x) + M_PI;
        float b = atan2(dir.y, sqrt(dir.x * dir.x + dir.z * dir.z)) + M_PI;
        if (fmod(a, (float)M_PI / 100.0f) < 0.001f || fmod(b, (float)M_PI / 100.0f) < 0.001f) {
            color += 0;
        } else {
            color += ray_color;
        }
         */
        color += trace_path(pos, dir, 4, -1, &tiny, objects, object_count, debug);
        samples++;
    } while (samples < input);
    color /= samples;
    if (debug) {
        //        printf("]\n");
        color = 1 - color;
    }
    write_imagef(output, coord, (float4)(color, 1.0f));
}
