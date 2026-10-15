/* vf2_model_viewer -- an SDL3 viewer for the extracted VF2 model OBJ files
 * (3D-model extraction, slice 4). Loads a Wavefront OBJ (as written by
 * `vf2i960 extract-models`), displays it with mouse/keyboard rotate + zoom, and
 * flat-shades the triangles with SDL_RenderGeometry. Rendering is a small software
 * transform (rotate, perspective divide, viewport) -- no GPU shaders -- so it runs
 * anywhere SDL3 does. The OBJ meshes are model-space per-part; a fully posed
 * character needs the TGP joint transforms (not applied here).
 *
 * Controls: drag / arrow keys rotate, wheel or +/- zoom, R resets, Esc quits.
 * --selftest renders a few frames with the dummy video driver and exits (CI).
 */
#include <SDL3/SDL.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { float x, y, z; } vec3;

typedef struct {
    vec3 *verts;
    size_t vcount;
    size_t vcap;
    int *idx; /* triangle vertex indices, 3 per face */
    size_t icount;
    size_t icap;
} mesh;

static int mesh_push_vert(mesh *m, vec3 v)
{
    if (m->vcount == m->vcap) {
        size_t n = m->vcap ? m->vcap * 2u : 1024u;
        vec3 *p = (vec3 *)realloc(m->verts, n * sizeof(*p));
        if (!p) return 0;
        m->verts = p; m->vcap = n;
    }
    m->verts[m->vcount++] = v;
    return 1;
}

static int mesh_push_tri(mesh *m, int a, int b, int c)
{
    if (m->icount + 3u > m->icap) {
        size_t n = m->icap ? m->icap * 2u : 3072u;
        int *p = (int *)realloc(m->idx, n * sizeof(*p));
        if (!p) return 0;
        m->idx = p; m->icap = n;
    }
    m->idx[m->icount++] = a; m->idx[m->icount++] = b; m->idx[m->icount++] = c;
    return 1;
}

static int load_obj(const char *path, mesh *m)
{
    FILE *f = fopen(path, "rb");
    char line[512];
    memset(m, 0, sizeof(*m));
    if (!f) { fprintf(stderr, "cannot open %s\n", path); return 0; }
    while (fgets(line, (int)sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            vec3 v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3) {
                if (!mesh_push_vert(m, v)) break;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            int fv[16];
            size_t n = 0u;
            char *p = line + 2;
            while (n < 16u) {
                int val = 0;
                int got = 0;
                while (*p == ' ') ++p;
                if (*p == '\0' || *p == '\n' || *p == '\r') break;
                got = sscanf(p, "%d", &val);
                if (got != 1) break;
                fv[n++] = val > 0 ? val - 1 : val; /* 1-indexed */
                while (*p && *p != ' ') ++p;       /* skip token (no v/t/n here) */
            }
            /* fan-triangulate */
            {
                size_t k = 1u;
                for (k = 1u; k + 1u < n; ++k) {
                    if (!mesh_push_tri(m, fv[0], fv[k], fv[k + 1u])) break;
                }
            }
        }
    }
    fclose(f);
    return m->vcount > 0u;
}

int main(int argc, char **argv)
{
    mesh m;
    const char *obj_path = NULL;
    int selftest = 0;
    int i = 0;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    float yaw = 0.6f, pitch = 0.3f, zoom = 1.0f;
    vec3 center = { 0.0f, 0.0f, 0.0f };
    float radius = 1.0f;
    int dragging = 0;
    int running = 1;
    int frames = 0;
    const int W = 800, H = 600;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) selftest = 1;
        else obj_path = argv[i];
    }
    if (obj_path == NULL && !selftest) {
        fprintf(stderr, "usage: %s <model.obj> [--selftest]\n", argv[0]);
        return 1;
    }

    memset(&m, 0, sizeof(m));
    if (obj_path != NULL) {
        if (!load_obj(obj_path, &m)) {
            fprintf(stderr, "failed to load %s\n", obj_path);
            if (!selftest) return 1;
        }
    }
    /* bounds -> center + radius for auto-fit */
    if (m.vcount > 0u) {
        vec3 lo = m.verts[0], hi = m.verts[0];
        size_t v = 0u;
        for (v = 1u; v < m.vcount; ++v) {
            vec3 p = m.verts[v];
            if (p.x < lo.x) { lo.x = p.x; }
            if (p.x > hi.x) { hi.x = p.x; }
            if (p.y < lo.y) { lo.y = p.y; }
            if (p.y > hi.y) { hi.y = p.y; }
            if (p.z < lo.z) { lo.z = p.z; }
            if (p.z > hi.z) { hi.z = p.z; }
        }
        center.x = (lo.x + hi.x) * 0.5f;
        center.y = (lo.y + hi.y) * 0.5f;
        center.z = (lo.z + hi.z) * 0.5f;
        radius = 0.5f * (hi.x - lo.x);
        if (0.5f * (hi.y - lo.y) > radius) radius = 0.5f * (hi.y - lo.y);
        if (0.5f * (hi.z - lo.z) > radius) radius = 0.5f * (hi.z - lo.z);
        if (radius < 1e-4f) radius = 1.0f;
    }

    if (selftest) {
        SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    }
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return selftest ? 0 : 1; /* no display in CI is not a failure */
    }
    window = SDL_CreateWindow("VF2 model viewer", W, H, 0);
    if (window != NULL) renderer = SDL_CreateRenderer(window, NULL);
    if (renderer == NULL) {
        fprintf(stderr, "no renderer: %s\n", SDL_GetError());
        SDL_Quit();
        return selftest ? 0 : 1;
    }

    while (running) {
        SDL_Event e;
        size_t t = 0u;
        float cy = cosf(yaw), sy = sinf(yaw), cp = cosf(pitch), sp = sinf(pitch);
        float scale = (0.42f * (float)H / radius) * zoom;

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
            else if (e.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode k = e.key.key;
                if (k == SDLK_ESCAPE) running = 0;
                else if (k == SDLK_LEFT) yaw -= 0.1f;
                else if (k == SDLK_RIGHT) yaw += 0.1f;
                else if (k == SDLK_UP) pitch -= 0.1f;
                else if (k == SDLK_DOWN) pitch += 0.1f;
                else if (k == SDLK_EQUALS || k == SDLK_PLUS) zoom *= 1.1f;
                else if (k == SDLK_MINUS) zoom /= 1.1f;
                else if (k == SDLK_R) { yaw = 0.6f; pitch = 0.3f; zoom = 1.0f; }
            } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) dragging = 1;
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) dragging = 0;
            else if (e.type == SDL_EVENT_MOUSE_MOTION && dragging) {
                yaw += e.motion.xrel * 0.01f;
                pitch += e.motion.yrel * 0.01f;
            } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                zoom *= (e.wheel.y > 0) ? 1.1f : (e.wheel.y < 0 ? 1.0f / 1.1f : 1.0f);
            }
        }

        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        for (t = 0u; t + 2u < m.icount + 1u && t + 2u <= m.icount; t += 3u) {
            SDL_Vertex sv[3];
            vec3 p[3];
            float nx, ny, nz, nl, shade;
            int j = 0;
            for (j = 0; j < 3; ++j) {
                int id = m.idx[t + (size_t)j];
                vec3 a;
                float x, y, z, rx, rz, ry2, rz2;
                if (id < 0 || (size_t)id >= m.vcount) { a.x = a.y = a.z = 0.0f; }
                else a = m.verts[id];
                x = a.x - center.x; y = a.y - center.y; z = a.z - center.z;
                /* yaw about Y, then pitch about X */
                rx = x * cy + z * sy;
                rz = -x * sy + z * cy;
                ry2 = y * cp - rz * sp;
                rz2 = y * sp + rz * cp;
                p[j].x = rx; p[j].y = ry2; p[j].z = rz2;
            }
            /* flat shade from face normal (z component) */
            nx = (p[1].y - p[0].y) * (p[2].z - p[0].z) - (p[1].z - p[0].z) * (p[2].y - p[0].y);
            ny = (p[1].z - p[0].z) * (p[2].x - p[0].x) - (p[1].x - p[0].x) * (p[2].z - p[0].z);
            nz = (p[1].x - p[0].x) * (p[2].y - p[0].y) - (p[1].y - p[0].y) * (p[2].x - p[0].x);
            nl = sqrtf(nx * nx + ny * ny + nz * nz);
            shade = nl > 1e-6f ? (0.3f + 0.7f * fabsf(nz) / nl) : 0.5f;
            for (j = 0; j < 3; ++j) {
                sv[j].position.x = (float)W * 0.5f + p[j].x * scale;
                sv[j].position.y = (float)H * 0.5f - p[j].y * scale;
                sv[j].color.r = shade; sv[j].color.g = shade; sv[j].color.b = shade;
                sv[j].color.a = 1.0f;
                sv[j].tex_coord.x = 0.0f; sv[j].tex_coord.y = 0.0f;
            }
            SDL_RenderGeometry(renderer, NULL, sv, 3, NULL, 0);
        }

        SDL_RenderPresent(renderer);
        ++frames;
        if (selftest && frames >= 3) running = 0;
        if (!selftest) SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(m.verts);
    free(m.idx);
    if (selftest) {
        printf("vf2_model_viewer selftest ok (%zu verts, %zu tris)\n",
               m.vcount, m.icount / 3u);
    }
    return 0;
}
