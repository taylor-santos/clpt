#define KERNEL_PROGRAM
#define HAVE_DOUBLE

#include "tinymt64.clh"
#include "cl_struct.h"

#undef KERNEL_PROGRAM

#define MAX_DEPTH 0

#define DEBUG(...) /*                   \
                   do {                 \
                   if (ctx->debug) {    \
                   printf(__VA_ARGS__); \
                   }                    \
                   } while (0) */

typedef union array_vec {
    float3 v;
    float  a[3];
} av;

float3
mul4x4(float4 *M, float3 X) {
    return (float3){
               dot(M[0].xyz, X) + M[0].w,
               dot(M[1].xyz, X) + M[1].w,
               dot(M[2].xyz, X) + M[2].w,
           } /
           (dot(M[3].xyz, X) + M[3].w);
}

float3
mul3x3(float3 *M, float3 X) {
    return (float3){
        dot(M[0], X),
        dot(M[1], X),
        dot(M[2], X),
    };
}

float3
left_mul3x3(float3 X, float3 *M) {
    return M[0] * X.x + M[1] * X.y + M[2] * X.z;
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
    if (*t < 0.0f) *t = b - sqrt(d);
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

bool
AABB_bounds(
    float3 origin,
    float3 inv_dir,
    float3 box_min,
    float3 box_max,
    float *t_min,
    float *t_max) {
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

    *t_min = tmin;
    *t_max = tmax;

    return tmax >= max(0.0, tmin);
}

float3
snell(float3 s, float3 n, float n1, float n2) {
    float ns = dot(n, s);
    return (n1 * s - n * (n1 * ns + sqrt(n2 * n2 + n1 * n1 * (ns * ns - 1)))) / n2;
}

float3
random_hemisphere(float3 n, tinymt64wp_t *rand, float angle) {
    float ct = cos(angle);
    float z  = ct + (1 - ct) * tinymt64_double01(rand);
    float p  = tinymt64_double01(rand) * 2 * M_PI;
    float r  = sqrt(1 - z * z);

    float3 v = {r * cos(p), r * sin(p), z};

    // Rotate v such that {0, 0, 1} points to n
    return (float3){
               n.y * n.y * v.x - n.x * n.y * v.y + (1 + n.z) * (n.z * v.x + n.x * v.z),
               -n.x * n.y * v.x + (1 - n.y * n.y + n.z) * v.y + n.y * (1 + n.z) * v.z,
               (1 + n.z) * (-n.x * v.x - n.y * v.y + n.z * v.z),
           } /
           (1 + n.z);

    // A random point on a unit sphere has a uniformly distributed z coordinate
    /*
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
     */
}

/*
float3
random_hemisphere(float3 n, tinymt64wp_t *rand) {
    float  z = -1 + tinymt64_double01(rand) * 2;
    float  p = tinymt64_double01(rand) * 2 * M_PI;
    float  r = sqrt(1 - z * z);
    float3 d = {r * cos(p), r * sin(p), z};
    if (dot(d, n) < 0) d = -d;
    return d;
}
 */

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
fresnel_reflect_amount(float n1, float n2, float3 normal, float3 incident, float f0, float f90) {
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

bool
intersect_object(
    float3                       origin,
    float3                       dir,
    float                       *t,
    int                         *side,
    __global __read_only Object *object) {
    switch (object->type) {
        case SPHERE:
            return ray_sphere_intersect(
                origin,
                dir,
                object->sphere.center,
                object->sphere.radius,
                t);
        case BOX:
            return ray_box_intersect(origin, 1 / dir, object->box.min, object->box.max, t, side);
    }
}

typedef struct HitInfo {
    float  dist;
    float3 point;
    float3 normal;
    int    object_index;

    __global __read_only Object *object;
} HitInfo;

typedef struct Context {
    __global __read_only Object     *objects;
    __read_only uint                 object_count;
    __global __read_only Light      *lights;
    __read_only uint                 light_count;
    __read_only float3               lower_bound;
    __read_only float3               upper_bound;
    __global __read_only KDTreeNode *nodes;
    __global __read_only cl_uint    *prim_ids;
    tinymt64wp_t                     rand;
    bool                             debug;
    int                              intersection_tests;
} Context;

float
split_pos(__global __read_only KDTreeNode *node) {
    return node->split;
}

uint
n_primitives(__global __read_only KDTreeNode *node) {
    return node->nprims >> 2;
}

uint
split_axis(__global __read_only KDTreeNode *node) {
    return node->flags & 3;
}

bool
is_leaf(__global __read_only KDTreeNode *node) {
    return (node->flags & 3) == 3;
}

uint
above_child(__global __read_only KDTreeNode *node) {
    return node->above_child >> 2;
}

typedef struct KDTask {
    __global __read_only KDTreeNode *node;
    float                            t_min;
    float                            t_max;
} KDTask;

bool
trace_kd(
    float3   origin,
    float3   dir,
    int      ignore,
    Context *ctx,
    float   *hit_t,
    int     *hit_index,
    int     *hit_side) {
    // https://pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Kd-Tree_Accelerator
    float3 inv_dir = 1 / dir;
    float  t_min, t_max;
    if (!AABB_bounds(origin, inv_dir, ctx->lower_bound, ctx->upper_bound, &t_min, &t_max)) {
        return false;
    }

    KDTask                           tasks[MAX_KD_DEPTH];
    int                              task_id = 0;
    __global __read_only KDTreeNode *node    = &ctx->nodes[0];

    bool did_hit = false;

    while (node != NULL) {
        if (did_hit && *hit_t < t_min) break;

        if (!is_leaf(node)) {
            uint  axis    = split_axis(node);
            float split   = split_pos(node);
            float t_plane = (split - (av){origin}.a[axis]) * (av){inv_dir}.a[axis];

            __global __read_only KDTreeNode *first_child, *second_child;

            bool below_first = ((av){origin}.a[axis] < split) ||
                               ((av){origin}.a[axis] == split && (av){dir}.a[axis] <= 0);

            if (below_first) {
                first_child  = node + 1;
                second_child = &ctx->nodes[above_child(node)];
            } else {
                first_child  = &ctx->nodes[above_child(node)];
                second_child = node + 1;
            }

            if (t_plane > t_max || t_plane <= 0) {
                node = first_child;
            } else if (t_plane < t_min) {
                node = second_child;
            } else {
                tasks[task_id].node  = second_child;
                tasks[task_id].t_min = t_plane;
                tasks[task_id].t_max = t_max;
                task_id++;

                node  = first_child;
                t_max = t_plane;
            }
        } else { // Leaf node
            float hue;
            if (ctx->debug) hue = tinymt64_double01(&ctx->rand);
            DEBUG(",Graphics3D[{Hue[%f],Opacity[0.2],Cuboid[{", hue);
            DEBUG("%v3f", node->lower_bound);
            DEBUG("},{");
            DEBUG("%v3f", node->upper_bound);
            DEBUG("}]}]");

            DEBUG(",Graphics3D[{Hue[%f]", hue);

            __global __read_only cl_uint *prim_ids;

            int n_prim = n_primitives(node);
            if (n_prim == 1) {
                prim_ids = &node->one_prim;
            } else {
                prim_ids = &ctx->prim_ids[node->prim_ids_offset];
            }
            ctx->intersection_tests += n_prim;
            for (int i = 0; i < n_prim; i++) {
                uint id = prim_ids[i];
                if (id == ignore) continue;
                __global __read_only Object *object;
                if (id < ctx->object_count) {
                    object = &ctx->objects[id];
                } else {
                    object = &ctx->lights[id - ctx->object_count].object;
                }

                switch (object->type) {
                    case SPHERE:
                        DEBUG(",Sphere[{");
                        DEBUG("%v3f", object->sphere.center);
                        DEBUG("},%f]", object->sphere.radius);
                        break;
                    case BOX:
                        DEBUG(",Cuboid[{");
                        DEBUG("%v3f", object->box.min);
                        DEBUG("},{");
                        DEBUG("%v3f", object->box.max);
                        DEBUG("}]");
                        break;
                }

                float t;
                int   side;
                bool  hit = intersect_object(origin, dir, &t, &side, object);
                if (hit && (!did_hit || t < *hit_t)) {
                    did_hit    = true;
                    *hit_t     = t;
                    *hit_index = id;
                    *hit_side  = side;
                }
            }

            DEBUG("}]");

            if (task_id > 0) {
                task_id--;
                node  = tasks[task_id].node;
                t_min = tasks[task_id].t_min;
                t_max = tasks[task_id].t_max;
            } else {
                break;
            }
        }
    }

    return did_hit;
}

bool
intersect(float3 origin, float3 dir, int ignore, Context *ctx, HitInfo *out_info) {
    // https://pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Kd-Tree_Accelerator

    float t;
    int   index;
    int   side;

    bool did_hit = trace_kd(origin, dir, ignore, ctx, &t, &index, &side);

    if (did_hit) {
        __global Object *object = index < ctx->object_count
                                      ? &ctx->objects[index]
                                      : &ctx->lights[index - ctx->object_count].object;

        out_info->dist         = t;
        out_info->object_index = index;
        out_info->point        = origin + dir * t;
        out_info->object       = object;

        switch (object->type) {
            case SPHERE:
                out_info->normal = normalize(out_info->point - object->sphere.center);
                break;
            case BOX:
                switch (side) {
                    case 0: out_info->normal = (float3)(-1, 0, 0); break;
                    case 1: out_info->normal = (float3)(1, 0, 0); break;
                    case 2: out_info->normal = (float3)(0, -1, 0); break;
                    case 3: out_info->normal = (float3)(0, 1, 0); break;
                    case 4: out_info->normal = (float3)(0, 0, -1); break;
                    case 5: out_info->normal = (float3)(0, 0, 1); break;
                }
                out_info->normal =
                    dot(out_info->normal, dir) < 0 ? out_info->normal : -out_info->normal;
                break;
        }
        return true;
    }
    return false;
}

/*
bool
intersect(float3 pos, float3 dir, HitInfo *info, int ignore, Context *ctx) {
    bool  did_hit = false;
    float min_t;
    int   hit_index;
    int   hit_side;

    for (int i = 0; i < ctx->object_count; i++) {
        if (i == ignore) continue; // Don't re-test the surface we're reflecting off of

        float t;
        bool  hit;
        int   side;

        hit = intersect_object(pos, dir, &t, &side, &ctx->objects[i]);

        if (hit && (!did_hit || t < min_t)) {
            did_hit   = true;
            min_t     = t;
            hit_index = i;
            hit_side  = side;
        }
    }
    for (int i = 0; i < ctx->light_count; i++) {
        if (i == ignore - ctx->object_count) continue;

        float t;
        bool  hit;
        int   side;

        hit = intersect_object(pos, dir, &t, &side, &ctx->lights[i].object);

        if (hit && (!did_hit || t < min_t)) {
            did_hit   = true;
            min_t     = t;
            hit_index = ctx->object_count + i;
            hit_side  = side;
        }
    }

    if (!info || !did_hit) return did_hit;

    __global Object *object = hit_index < ctx->object_count
                                  ? &ctx->objects[hit_index]
                                  : &ctx->lights[hit_index - ctx->object_count].object;

    info->dist         = min_t;
    info->object_index = hit_index;
    info->point        = pos + dir * min_t;
    info->object       = object;

    switch (object->type) {
        case SPHERE: info->normal = normalize(info->point - object->sphere.center); break;
        case BOX:
            switch (hit_side) {
                case 0: info->normal = (float3)(-1, 0, 0); break;
                case 1: info->normal = (float3)(1, 0, 0); break;
                case 2: info->normal = (float3)(0, -1, 0); break;
                case 3: info->normal = (float3)(0, 1, 0); break;
                case 4: info->normal = (float3)(0, 0, -1); break;
                case 5: info->normal = (float3)(0, 0, 1); break;
            }
            info->normal = dot(info->normal, dir) < 0 ? info->normal : -info->normal;
            break;
    }

    return true;
}
*/

int
occlusion_check(float3 origin, float3 dir, int ignore, Context *ctx) {
    float t;
    int   index;
    int   side;

    bool did_hit = trace_kd(origin, dir, ignore, ctx, &t, &index, &side);

    return did_hit ? index : -1;
}

float3
random_in_sphere(tinymt64wp_t *rand) {
    float z = tinymt64_double01(rand) * 2 - 1;
    float p = tinymt64_double01(rand) * 2 * M_PI;
    float r = sqrt(1 - z * z);
    float d = pow((float)tinymt64_double01(rand), 1.0f / 3.0f);
    return (float3)(r * cos(p), r * sin(p), z) * d;
}

float3
random_in_box(float3 min, float3 max, tinymt64wp_t *rand) {
    float3 r = {
        tinymt64_double01(rand),
        tinymt64_double01(rand),
        tinymt64_double01(rand),
    };
    return min + r * (max - min);
}

float3
cosine_hemisphere(float3 n, tinymt64wp_t *rand) {
    float  r1  = 2 * M_PI * tinymt64_double01(rand);
    float  r2  = tinymt64_double01(rand);
    float  r2s = sqrt(r2);
    float3 u   = normalize(cross(fabs(n.x) > 0.1f ? (float3)(0, 1, 0) : (float3)(1, 0, 0), n));
    float3 v   = cross(n, u);
    return normalize(u * cos(r1) * r2s + v * sin(r1) * r2s + n * sqrt(1 - r2));
}

void
make_orthogonal(float3 w, float3 *v, float3 *u) {
    float3 n = (w.y < w.z) ? (float3)(0, 1, 0) : (float3)(0, 0, 1);
    *v       = normalize(cross(w, n));
    *u       = normalize(cross(w, *v));
}

float3
spherical_to_cartesian(float theta, float phi) {
    return (float3){
        cos(phi) * sin(theta),
        sin(phi) * sin(theta),
        cos(theta),
    };
}

float3
random_in_cone(float cos_theta, tinymt64wp_t *rand) {
    float z = cos_theta + (1 - cos_theta) * tinymt64_double01(rand);
    float p = tinymt64_double01(rand) * 2 * M_PI;
    float r = sqrt(1 - z * z);
    return (float3){
        r * cos(p),
        r * sin(p),
        z,
    };
}

float3
hable(float3 x) {
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

typedef struct Hit {
    float3 color;
    float3 emission;
} Hit;

float3
sample_light(float3 origin, float3 normal, int light_index, int ignore_index, Context *ctx) {
    float3 Lo = 0;
    float3 L  = ctx->lights[light_index].emission * ctx->lights[light_index].object.albedo;
    float3 c  = ctx->lights[light_index].object.sphere.center;
    float  r  = ctx->lights[light_index].object.sphere.radius;

    float3 w                = c - origin;
    float  distanceToCenter = length(w);
    // If the light intersects a surface, and the ray hits that surface within epsilon of the edge
    // of the light, this can cause numerical instability that leads to NaNs in the following
    // calculations. Return the emission of the light as if the ray had hit it directly.
    if (distanceToCenter <= r + 0.0001f) return L;
    w                = normalize(w);
    float  cos_theta = r / distanceToCenter;
    float  q         = sqrt(1 - cos_theta * cos_theta);
    float3 v, u;
    make_orthogonal(w, &v, &u);

    float3 toWorld[3] = {u, w, v};

    float r0 = tinymt64_double01(&ctx->rand);
    float r1 = tinymt64_double01(&ctx->rand);

    float theta = acos(1 - r0 + r0 * q);
    float phi   = 2 * M_PI * r1;

    float3 loc = {
        cos(phi) * sin(theta),
        cos(theta),
        sin(phi) * sin(theta),
    };
    float3 nwp = normalize(left_mul3x3(loc, toWorld));

    float dotNL = dot(nwp, normal);
    if (dotNL > 0) {
        HitInfo light_hit;
        if (occlusion_check(origin, nwp, ignore_index, ctx) == light_index + ctx->object_count) {
            float pdf_xp = 1.0f / (2 * M_PI * (1.0f - q));
            Lo += dotNL * (1.0f / pdf_xp) * L / M_PI;
        }
    }
    return Lo;
}

float3
trace_ray(float3 origin, float3 dir, int ignore_index, bool specular, int depth, Context *ctx) {
    float3 ret        = 0;
    float3 throughput = 1;

    float hue;
    if (ctx->debug) hue = tinymt64_double01(&ctx->rand);

    while (true) {
        HitInfo hit;
        // if (!intersect(origin, dir, &hit, ignore_index, ctx)) {
        if (!intersect(origin, dir, ignore_index, ctx, &hit)) {
            DEBUG(",Graphics3D[{Hue[%f],HalfLine[{", hue);
            DEBUG("%v3f", origin);
            DEBUG("},{");
            DEBUG("%v3f", dir);
            DEBUG("}]}]");
            float3 ambient = 0;
            return ret + ambient * throughput; // Ambient
        }
        return 0;

        DEBUG(",Graphics3D[{Hue[%f],Line[{{", hue);
        DEBUG("%v3f", origin);
        DEBUG("},{");
        DEBUG("%v3f", hit.point);
        DEBUG("}}]}]");

        float3 hit_color = hit.object->albedo;

        if (specular && hit.object_index >= ctx->object_count) {
            // Normally, the main light path ignores direct emission contribution so as not to
            // double-count when lights are explicitly sampled. If, however, we are on the first ray
            // from the camera OR if we are in a specular reflection, the emission needs to be
            // included. Otherwise, lights will look dark when viewed directly or through a specular
            // reflection.
            float3 hit_emission =
                ctx->lights[hit.object_index - ctx->object_count].emission * hit_color;
            ret += hit_emission * throughput;
        }

        float p = max(max(throughput.x, throughput.y), throughput.z);
        if (depth <= 0 || tinymt64_double01(&ctx->rand) > p) {
            break;
        }
        throughput /= p;

        typedef enum HitType {
            DIFFUSE,
            REFLECT,
            REFRACT,
        } HitType;

        float spec_prob = hit.object->specular_chance;
        float refr_prob = hit.object->refraction_chance;

        if (spec_prob > 0.0f) {
            float fresnel =
                fresnel_reflect_amount(1.0f, hit.object->IOR, dir, hit.normal, spec_prob, 1.0f);

            float multiplier = (1 - fresnel) / (1 - spec_prob);
            refr_prob *= multiplier;
            spec_prob = fresnel;
        }
        float   ray_prob = 1.0f;
        HitType hit_type = DIFFUSE;
        {
            float roll = tinymt64_double01(&ctx->rand);
            if (spec_prob > 0.0f && roll < spec_prob) {
                hit_type = REFLECT;
                ray_prob = spec_prob;
            } else if (refr_prob > 0.0f && roll < spec_prob + refr_prob) {
                hit_type = REFRACT;
                ray_prob = refr_prob;
            } else {
                ray_prob = 1.0f - (spec_prob + refr_prob);
            }
            ray_prob = max(ray_prob, 0.001f); // TODO is this necessary?
        }

        bool hit_light   = hit.object_index >= ctx->object_count;
        int  light_count = hit_light ? ctx->light_count - 1 : ctx->light_count;
        if (hit_type == DIFFUSE && light_count) {
            int light_index = tinymt64_uint64(&ctx->rand) % light_count;
            if (hit_light && light_index >= (hit.object_index - ctx->object_count)) {
                light_index++;
            }
            // for (int light_index = 0; light_index < ctx->light_count; light_index++) {
            float3 light_sample =
                sample_light(hit.point, hit.normal, light_index, hit.object_index, ctx) *
                light_count;
            if (any(light_sample > 0.0f)) {
                ret += light_sample * hit_color * throughput;
            }
        }

        throughput /= ray_prob;

        float3 diffuse_dir = cosine_hemisphere(hit.normal, &ctx->rand);

        switch (hit_type) {
            case DIFFUSE: dir = diffuse_dir; break;
            case REFLECT: dir -= 2 * dot(dir, hit.normal) * hit.normal; break;
            case REFRACT: return 0; // TODO
        }

        origin       = hit.point;
        ignore_index = hit.object_index;
        specular     = (hit_type != DIFFUSE);
        depth--;
        throughput *= hit_color;
    }
    return ret;
}

void kernel
my_kernel(
    __write_only image2d_t           output,
    __global __read_only float4     *camera,
    __global __read_only ulong      *seed,
    __read_only int                  input,
    __read_only ulong                noise,
    __global __read_only Object     *objects,
    __read_only uint                 object_count,
    __global __read_only Light      *lights,
    __read_only uint                 light_count,
    __read_only float3               lower_bound,
    __read_only float3               upper_bound,
    __global __read_only KDTreeNode *nodes,
    __global __read_only cl_uint    *prim_ids) {
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int2 res   = get_image_dim(output);

    bool debug = all(coord == res / 2);
    //    debug         = &is_debug;

    /*
    if (is_debug) {
        printf("Show[");
        for (int i = 0; i < object_count; i++) {
            if (i) printf(",");
            printf("Graphics3D[{Opacity[0.5],RGBColor[");
            printf("%v3f", objects[i].albedo);
            printf("],Specularity[");
            printf("%f", objects[i].specular_chance);
            printf("],");
            switch (objects[i].type) {
                case SPHERE:
                    printf("Sphere[{");
                    printf("%v3f", objects[i].sphere.center);
                    printf("},%f", objects[i].sphere.radius);
                    printf("]");
                    break;
                case BOX:
                    printf("Cuboid[{");
                    printf("%v3f", objects[i].box.min);
                    printf("},{");
                    printf("%v3f", objects[i].box.max);
                    printf("}]");
                    break;
            }
            printf("}]");
        }
        for (int i = 0; i < light_count; i++) {
            if (i || object_count) printf(",");
            printf("Graphics3D[{Glow[RGBColor[");
            printf("%v3f", lights[i].object.albedo * lights[i].emission);
            printf("]],Specularity[");
            printf("%f", lights[i].object.specular_chance);
            printf("],");
            switch (lights[i].object.type) {
                case SPHERE:
                    printf("Sphere[{");
                    printf("%v3f", lights[i].object.sphere.center);
                    printf("},%f", lights[i].object.sphere.radius);
                    printf("]");
                    break;
                case BOX:
                    printf("Cuboid[{");
                    printf("%v3f", lights[i].object.box.min);
                    printf("},{");
                    printf("%v3f", lights[i].object.box.max);
                    printf("}]");
                    break;
            }
            printf("}]");
        }
    }
     */

    Context ctx = {
        objects,
        object_count,
        lights,
        light_count,
        lower_bound,
        upper_bound,
        nodes,
        prim_ids,
        {0},
        debug,
        0,
    };

    tinymt64_init(&ctx.rand, seed[coord.x + coord.y * res.x] + noise);

    float3 color   = 0;
    int    samples = 0;
    do {
        float3 origin, dir;
        {
            float2 sample = (float2){
                (float)coord.x + tinymt64_double01(&ctx.rand),
                (float)coord.y + tinymt64_double01(&ctx.rand),
            };
            float2 rat = 2 * (sample / convert_float2(res)) - 1;
            float3 fcp = mul4x4(camera, (float3)(rat, 1.0f));

            origin = (float3){camera[0].z, camera[1].z, camera[2].z} / camera[3].z;
            dir    = normalize((fcp - origin).xyz);
        }

        float3 new_color = 0;
        {
            int  ignore_index      = -1;
            bool specular          = true;
            ctx.intersection_tests = 0;
            new_color   = trace_ray(origin, dir, ignore_index, specular, MAX_DEPTH, &ctx);
            new_color.x = ctx.intersection_tests / 25.5;
        }

        /*
        if (is_debug) {
            printf(",Graphics3D[{Hue[%f],Line[{{", tinymt64_double01(&ctx.rand));
            printf("%v3f", origin);
            printf("}");
            for (int i = 0; i <= *hit_depth; i++) {
                printf(",{");
                printf("%v3f", points[i]);
                printf("}");
            }
            printf("}]}]");
        }
         */

        color += new_color;
        samples++;
    } while (samples < input);

    color /= samples;
    if (ctx.debug) {
        color = (float3){1, 0, 0};
    }
    write_imagef(output, coord, (float4)(color, 1.0f));
}
