/* Native camera oracle for the browser view. Only world dimensions are needed. */
#include "../view/az_view.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void vector(const char *name, const float p[3])
{
    printf("\"%s\":[%.9g,%.9g,%.9g],", name, (double)p[0], (double)p[1], (double)p[2]);
}

int main(int argc, char **argv)
{
    if (argc != 9 && argc != 12) {
        fprintf(stderr, "usage: az_camera_probe GX GY GZ WIDTH HEIGHT YAW PITCH DIST [TX TY TZ]\n");
        return 2;
    }
    u32 sizes[5];
    float values[6];
    for (int i = 1; i < argc; i++) {
        char *end;
        if (i <= 5) {
            unsigned long n = strtoul(argv[i], &end, 10);
            if (!*argv[i] || *end || n == 0 || n > (i <= 3 ? 4096ul : 16384ul)) return 2;
            sizes[i-1] = (u32)n;
        } else {
            float x = strtof(argv[i], &end);
            if (!*argv[i] || *end || !isfinite(x)) return 2;
            values[i-6] = x;
        }
    }
    AzWorld *world = calloc(1, sizeof *world);
    AzView *view = az_view_create(sizes[3], sizes[4]);
    if (!world || !view) { free(world); az_view_destroy(view); return 1; }
    world->gx = sizes[0]; world->gy = sizes[1]; world->gz = sizes[2];
    az_view_camera(view, values[0], values[1], values[2], argc == 12 ? &values[3] : NULL);
    AzCameraPose p;
    bool ok = az_view_camera_pose(view, world, &p);
    if (ok) {
        putchar('{');
        vector("eye", p.eye); vector("right", p.right); vector("up", p.up);
        vector("forward", p.forward); vector("target", p.target);
        printf("\"focal\":%.9g}\n", (double)p.focal);
    }
    az_view_destroy(view);
    free(world);
    return ok ? 0 : 1;
}
