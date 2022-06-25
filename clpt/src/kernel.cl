#define KERNEL_PROGRAM
#define HAVE_DOUBLE

#include "tinymt64.clh"
#include "cl_struct.h"

#undef KERNEL_PROGRAM

__private bool *debug;

#define DEBUG(...) \
    if (*debug) printf(__VA_ARGS__)

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
intersect_object(float3 pos, float3 dir, float *t, int *side, __global __read_only Object *object) {
    switch (object->type) {
        case SPHERE:
            return ray_sphere_intersect(pos, dir, object->sphere.center, object->sphere.radius, t);
        case BOX: return ray_box_intersect(pos, 1 / dir, object->box.min, object->box.max, t, side);
    }
}

bool
intersect(
    float3                       pos,
    float3                       dir,
    float                       *t,
    int                         *id,
    float3                      *norm,
    int                          ignore,
    __global __read_only Object *objects,
    __read_only uint             object_count,
    __global __read_only Light  *lights,
    __read_only uint             light_count) {
    bool  did_hit = false;
    float min_t;
    int   hit_index;
    int   hit_side;

    for (int i = 0; i < object_count; i++) {
        if (i == ignore) continue; // Don't re-test the surface we're reflecting off of

        float t;
        bool  hit;
        int   side;

        hit = intersect_object(pos, dir, &t, &side, &objects[i]);

        if (hit && (!did_hit || t < min_t)) {
            did_hit   = true;
            min_t     = t;
            hit_index = i;
            hit_side  = side;
        }
    }
    for (int i = 0; i < light_count; i++) {
        if (i == ignore - object_count) continue;

        float t;
        bool  hit;
        int   side;

        hit = intersect_object(pos, dir, &t, &side, &lights[i].object);

        if (hit && (!did_hit || t < min_t)) {
            did_hit   = true;
            min_t     = t;
            hit_index = object_count + i;
            hit_side  = side;
        }
    }
    if (!did_hit) {
        return false;
    }

    if (t) *t = min_t;
    if (id) *id = hit_index;

    if (!norm) {
        return true;
    }

    pos += dir * min_t;

    __global __read_only Object *object =
        (hit_index < object_count) ? &objects[hit_index] : &lights[hit_index - object_count].object;

    float3 n;
    switch (object->type) {
        case SPHERE: n = normalize(pos - object->sphere.center); break;
        case BOX:
            switch (hit_side) {
                case 0: n = (float3)(-1, 0, 0); break;
                case 1: n = (float3)(1, 0, 0); break;
                case 2: n = (float3)(0, -1, 0); break;
                case 3: n = (float3)(0, 1, 0); break;
                case 4: n = (float3)(0, 0, -1); break;
                case 5: n = (float3)(0, 0, 1); break;
            }
            n = dot(n, dir) < 0 ? n : -n;
            break;
    }

    *norm = n;
    return true;
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

float3
col(uchar3 c) {
    return convert_float3(c) / 255.0;
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

#define MAX_DEPTH 2

void kernel
my_kernel(
    __write_only image2d_t       output,
    __global __read_only float4 *camera,
    __global __read_only ulong  *seed,
    __read_only int              input,
    __read_only ulong            noise,
    __global __read_only Object *objects,
    __read_only uint             object_count,
    __global __read_only Light  *lights,
    __read_only uint             light_count) {
    int2 coord = (int2)(get_global_id(0), get_global_id(1));
    int2 res   = get_image_dim(output);

    bool is_debug = false; // all(coord == res / 2);
    debug         = &is_debug;

    if (is_debug) {
        printf("Show[");
        for (int i = 0; i < object_count; i++) {
            if (i) printf(",");
            printf("Graphics3D[{RGBColor[");
            printf("%v3f", col(objects[i].color));
            printf("],Specularity[");
            printf("%f", objects[i].pct_spec);
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
            printf("%v3f", col(lights[i].object.color) * lights[i].emission);
            printf("]],Specularity[");
            printf("%f", lights[i].object.pct_spec);
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

    tinymt64wp_t rand = {0};
    tinymt64_init(&rand, seed[coord.x + coord.y * res.x] + noise);

    float3 color   = 0;
    int    samples = 0;
    do {
        float3 pos, dir;
        {
            float2 sample = (float2){
                (float)coord.x + tinymt64_double01(&rand),
                (float)coord.y + tinymt64_double01(&rand),
            };
            float2 rat = 2 * (sample / convert_float2(res)) - 1;
            float3 fcp = mul4x4(camera, (float3)(rat, 1.0f));

            pos = (float3){camera[0].z, camera[1].z, camera[2].z} / camera[3].z;
            dir = normalize((fcp - pos).xyz);
        }

        DEBUG(",Graphics3D[{Hue[%f],", tinymt64_double01(&rand));

        Hit hits[MAX_DEPTH];
        int last_hit_index = -1;
        int depth;
        // If last hit was a specular reflection, or if it's our first ray, collect the next hits
        // emission directly. Otherwise, light sources will look dark when viewed directly or
        // through a specular reflection.
        bool last_specular = true;
        for (depth = 0; depth < MAX_DEPTH; depth++) {
            float  min_t;
            int    hit_index;
            float3 n;
            if (!intersect(
                    pos,
                    dir,
                    &min_t,
                    &hit_index,
                    &n,
                    last_hit_index,
                    objects,
                    object_count,
                    lights,
                    light_count)) {
                hits[depth].color    = 1;
                hits[depth].emission = 0;
                depth++;
                break;
            }

            last_hit_index = hit_index;

            __global __read_only Object *object = (hit_index < object_count)
                                                      ? &objects[hit_index]
                                                      : &lights[hit_index - object_count].object;

            float3 hit_color = col(object->color);

            hits[depth].emission = (hit_index < object_count) ? 0
                                   // : lights[hit_index - object_count].emission * hit_color;
                                   : last_specular
                                       ? lights[hit_index - object_count].emission * hit_color
                                       : 0;

            last_specular = false;

            DEBUG(",Line[{{");
            DEBUG("%v3f", pos);
            DEBUG("}");

            pos += dir * min_t;

            DEBUG(",{");
            DEBUG("%v3f", pos);
            DEBUG("}}]");

            float spec_prob = object->pct_spec;
            //            if (spec_prob > 0.0f) {
            //                spec_prob = fresnel_reflect_amount(1.0f, object->IOR, dir, n,
            //                spec_prob, 1.0f);
            //            }
            bool diffuse = spec_prob < tinymt64_double01(&rand);

            if (diffuse) {
                float3 Lo = 0;
                for (int index = 0; index < light_count; index++) {
                    float3 L = lights[index].emission * col(lights[index].object.color);
                    float3 c = lights[index].object.sphere.center;
                    float  r = lights[index].object.sphere.radius;

                    float3 o = pos;

                    float3 w                = c - o;
                    float  distanceToCenter = length(w);
                    w                       = normalize(w);
                    float  cos_theta        = r / distanceToCenter;
                    float  q                = sqrt(1 - cos_theta * cos_theta);
                    float3 v, u;
                    make_orthogonal(w, &v, &u);

                    //                if (is_debug) {
                    //                    printf(",Line[{{");
                    //                    printf("%v3f", o);
                    //                    printf("},{");
                    //                    printf("%v3f", o + w * 0.1f);
                    //                    printf("}}]");
                    //                    printf(",Line[{{");
                    //                    printf("%v3f", o);
                    //                    printf("},{");
                    //                    printf("%v3f", o + v * 0.1f);
                    //                    printf("}}]");
                    //                    printf(",Line[{{");
                    //                    printf("%v3f", o);
                    //                    printf("},{");
                    //                    printf("%v3f", o + u * 0.1f);
                    //                    printf("}}]");
                    //                }

                    float3 toWorld[3] = {u, w, v};

                    float r0 = tinymt64_double01(&rand);
                    float r1 = tinymt64_double01(&rand);

                    float theta = acos(1 - r0 + r0 * q);
                    float phi   = 2 * M_PI * r1;

                    float3 loc = {
                        cos(phi) * sin(theta),
                        cos(theta),
                        sin(phi) * sin(theta),
                    };
                    float3 nwp = normalize(left_mul3x3(loc, toWorld));

                    if (dot(nwp, n) > 0) {
                        float dist;
                        int   id;
                        if (intersect(
                                o,
                                nwp,
                                &dist,
                                &id,
                                NULL,
                                hit_index,
                                objects,
                                object_count,
                                lights,
                                light_count)) {
                            if (id == index + object_count) {
                                if (is_debug) {
                                    printf(",Line[{{");
                                    printf("%v3f", o);
                                    printf("},{");
                                    printf("%v3f", o + nwp * dist);
                                    printf("}}]");
                                }
                                float dotNL = clamp(dot(nwp, n), 0.0f, 1.0f);
                                if (dotNL > 0) {
                                    float pdf_xp = 1.0f / (2 * M_PI * (1.0f - q));
                                    Lo += dotNL * (1.0f / pdf_xp) * L / M_PI;
                                }
                            } else {
                                if (is_debug) {
                                    printf(",HalfLine[{");
                                    printf("%v3f", o);
                                    printf("},{");
                                    printf("%v3f", nwp);
                                    printf("}]");
                                }
                            }
                        } else {
                            if (is_debug) {
                                printf(",HalfLine[{");
                                printf("%v3f", o);
                                printf("},{");
                                printf("%v3f", nwp);
                                printf("}]");
                            }
                        }
                    }
                }
                Lo /= light_count;
                hits[depth].emission += hit_color * Lo;
            }
            /*
            bool cond = false;
            for (int i = 0; i < light_count; i++) {
                if (lights[i].object.type != SPHERE) continue;
                float3 x         = pos;
                float  sr        =
            lights[i].object.sphere.radius; float3 sp        =
            lights[i].object.sphere.center; float3 sw        = sp
            - x; float3 su        = normalize(cross((float3)(0,
            1, 0), sw)); float3 sv        = cross(sw, su); float
            cos_a_max = sqrt(1 - sr * sr / dot(x - sp, x - sp));
                float  eps1      = tinymt64_double01(&rand);
                float  eps2      = tinymt64_double01(&rand);
                float  cos_a     = 1 - eps1 + eps1 * cos_a_max;
                float  sin_a     = sqrt(1 - cos_a * cos_a);
                float  phi       = 2 * M_PI * eps2;

                float3 l = normalize(su * cos(phi) * sin_a + sv *
            sin(phi) * sin_a + sw * cos_a);

                int index;
                if (intersect(
                        pos,
                        l,
                        0,
                        &index,
                        0,
                        hit_index,
                        objects,
                        object_count,
                        lights,
                        light_count) &&
                    index == object_count + i) {
                    float omega = 2 * M_PI * (1 - cos_a_max);
                    hits[depth].emission +=
            col(objects[hit_index].color) * lights[i].emission *
                                            col(lights[i].object.color)
            * dot(l, n) * M_1_PI;
                }

                //
                float3 p;
                switch (objects[i].type) {
                    case SPHERE:
                        p = objects[i].sphere.center +
                            random_in_sphere(&rand) *
            objects[i].sphere.radius; break; case BOX: p =
            random_in_box(objects[i].box.min, objects[i].box.max,
            &rand); break;
                }
                float3 d = normalize(p - pos);
                if (dot(d, n) < 0) continue;

                int index;
                if (intersect(pos, d, 0, &index, 0, hit_index,
            objects, object_count) && index == i) { cond     =
            true; float3 e = objects[i].emission *
            convert_float3(objects[i].color) / 255.0;
                    hits[depth].emission += dot(d, n) * e;
                }
                 */
            //}

            /*
            float p =
                1 - ((float)(depth * depth) /
                     (float)((MAX_DEPTH - 1) * (MAX_DEPTH - 1))); // max(max(hit_color.x,
                                                                  // hit_color.y), hit_color.z);
            if (!p) {
                if (tinymt64_double01(&rand) < p) {
                    hit_color = hit_color * (1 / p);
                } else {
                    break;
                }
            }

             */

            hits[depth].color = hit_color;

            float3 diffuse_dir = cosine_hemisphere(n, &rand);
            if (diffuse) {
                dir = diffuse_dir;
            } else {
                dir -= 2 * dot(dir, n) * n;
                if (object->roughness > 0) {
                    dir += (diffuse_dir - dir) * object->roughness;
                }
                last_specular = true;
            }

            /*
            // Uniform hemisphere distribution
            {
                float z = tinymt64_double01(&rand);
                float p = tinymt64_double01(&rand) * 2 * M_PI;
                float r = sqrt(1 - z * z);
                dir     = (float3){
                        r * cos(p),
                        r * sin(p),
                        z,
                };
                if (dot(dir, n) < 0) dir = -dir;
            }
            hits[depth].color *= 2 * dot(dir, n);
            */

            /*
            // Cosine-weighted hemisphere distribution
            dir = cosine_hemisphere(n, &rand);
             */

            /*
            if (object->pct_spec) {
                dir -= 2 * dot(dir, n) * n;
                last_specular = true;
            } else {
                dir = cosine_hemisphere(n, &rand);
            }
            */
            /*
            float3 diff       = cosine_hemisphere(n, &rand);
            float  spec       = object->pct_spec;
            if (spec > 0.0f) {
                spec = fresnel_reflect_amount(1.0f, object->IOR,
            dir, n, spec, 1.0f);
            }
            if (spec == 1.0 || (0 < spec &&
            tinymt64_double01(&rand) <= spec)) {
                // Specular reflection
                dir -= 2 * dot(dir, n) * n;
                if (object->roughness > 0) {
                    dir += (diff - dir) * object->roughness;
                }
                last_specular = true;
                //                dir = random_hemisphere(dir,
            &rand, input / 100.0f); } else {
                // Diffuse reflection
                dir = diff;
            }
             */

            //            dir -= 2 * dot(dir, n) * n;
            //            dir = random_hemisphere(dir, &rand,
            //            input / 100.0f);
            // dir               = cosine_hemisphere(n, &rand);

            // dir               = random_hemisphere(n, &rand);
            // hits[depth].color = 2 * hit_color * dot(dir, n);

            /*
            if (object->type == BOX || true) {
                float  r1  = 2 * M_PI * tinymt64_double01(&rand);
                float  r2  = tinymt64_double01(&rand);
                float  r2s = sqrt(r2);
                float3 w   = n;
                float3 u =
                    normalize(cross(fabs(w.x) > 0.1f ?
            (float3)(0, 1, 0) : (float3)(1, 0, 0), w)); float3 v
            = cross(w, u); dir      = normalize(u * cos(r1) * r2s
            + v * sin(r1) * r2s + w * sqrt(1 - r2)); } else { n =
            random_hemisphere(n, &rand, 0.1f); dir -= 2 *
            dot(dir, n) * n;
            }
            */
        }

        DEBUG("}]");
        depth--;
        float3 new_color = hits[depth].emission * hits[depth].color;
        for (depth--; depth >= 0; depth--) {
            new_color *= hits[depth].color;
            new_color += hits[depth].emission;
        }
        color += new_color;

        // color += hits[0].emission;

        /*
        double hue = tinymt64_double01(&rand);

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
                bool is_reflection = tinymt64_double01(&rand) <= prob;
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
        //        color += trace_path(pos, dir, 0, -1, &rand, objects, object_count, 1, debug);
        samples++;
    } while (samples < input);
    color /= samples;
    if (is_debug) {
        if (light_count) {
            printf(",Lighting->{");
            for (int i = 0; i < light_count; i++) {
                if (i) printf(",");
                printf("{\"Point\",RGBColor[");
                printf("%v3f", col(lights[i].object.color) * lights[i].emission);
                printf("],{");
                switch (objects[i].type) {
                    case SPHERE: printf("%v3f", lights[i].object.sphere.center); break;
                    case BOX:
                        printf("%v3f", (lights[i].object.box.min + lights[i].object.box.max) / 2);
                        break;
                }
                printf("}}");
            }
            printf("}");
        }
        printf("]\n\n");
        color = (float3)(1, 0, 0);
    }
    /*
    if (input % 2) {
        float  exposure_bias = 2.0f;
        float3 curr          = hable(color * exposure_bias);
        float3 W             = 11.2f;
        float3 white_scale   = 1 / hable(W);
        color                = curr * white_scale;
    }*/
    write_imagef(output, coord, (float4)(color, 1.0f));
}
