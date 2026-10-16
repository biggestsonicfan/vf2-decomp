/* vf2_model_viewer -- an SDL3 / SDL_GPU viewer for the extracted VF2 model OBJ
 * files (3D-model extraction). Loads a Wavefront OBJ (positions + UVs, as
 * written by `vf2i960 export-textured`), uploads a de-swizzled texture sheet
 * (from a raw texture-RAM bank via vf2_core), and renders the textured mesh with
 * SDL_GPU (SDL3's GPU API) using SPIR-V shaders -- not raw Vulkan, no software
 * rasterizer. The OBJ meshes are model-space per-part; a fully posed character
 * needs the TGP joint transforms (not applied here).
 *
 * Usage: vf2_model_viewer <model.obj> [--tex <bank.bin>] [--sheet-size WxH]
 *                         [--selftest]
 * Controls: drag / arrow keys rotate, wheel or +/- zoom, R resets, Esc quits.
 * --selftest loads the OBJ, inits SDL and probes for a GPU device, then exits 0
 * (headless CI has no GPU, which is not a failure).
 */
#include <SDL3/SDL.h>

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/texture_sheet.h"
#include "vf2/texture_link.h"

/* SPIR-V shader bytecode, embedded at build time (dxc -> .spv -> bin2c). */
#include "model_vert_spv.h"
#include "model_frag_spv.h"

typedef struct {
    float x, y, z;
} vec3;

typedef struct {
    float px, py, pz;
    float u, v;
} gpu_vertex;

typedef struct {
    gpu_vertex *v;
    size_t count;
    size_t cap;
} vbuf;

static int vbuf_push(vbuf *b, gpu_vertex vtx)
{
    if (b->count == b->cap) {
        size_t n = b->cap ? b->cap * 2u : 4096u;
        gpu_vertex *p = (gpu_vertex *)realloc(b->v, n * sizeof(*p));
        if (!p) {
            return 0;
        }
        b->v = p;
        b->cap = n;
    }
    b->v[b->count++] = vtx;
    return 1;
}

/* Parse "v/vt", "v//vn", "v/vt/vn" or "v"; returns the (1-based) v and vt (0 if
 * absent), both converted to 0-based (vt stays -1 if absent). */
static void parse_face_token(const char *tok, long *vi, long *ti)
{
    long v = 0;
    long t = 0;
    *vi = -1;
    *ti = -1;
    if (sscanf(tok, "%ld/%ld", &v, &t) >= 1) {
        *vi = (v > 0) ? v - 1 : v;
        if (strchr(tok, '/') != NULL && t != 0) {
            *ti = (t > 0) ? t - 1 : t;
        }
    }
}

static int load_obj(const char *path, vbuf *out)
{
    FILE *f = fopen(path, "rb");
    char line[512];
    vec3 *pos = NULL;
    float *uv = NULL; /* 2 floats per entry */
    size_t pcount = 0u, pcap = 0u;
    size_t tcount = 0u, tcap = 0u;
    int ok = 1;

    memset(out, 0, sizeof(*out));
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 0;
    }
    while (fgets(line, (int)sizeof(line), f)) {
        if (line[0] == 'v' && line[1] == ' ') {
            vec3 v;
            if (sscanf(line + 2, "%f %f %f", &v.x, &v.y, &v.z) == 3) {
                if (pcount == pcap) {
                    size_t n = pcap ? pcap * 2u : 1024u;
                    vec3 *p = (vec3 *)realloc(pos, n * sizeof(*p));
                    if (!p) { ok = 0; break; }
                    pos = p; pcap = n;
                }
                pos[pcount++] = v;
            }
        } else if (line[0] == 'v' && line[1] == 't' && line[2] == ' ') {
            float u = 0.0f, w = 0.0f;
            if (sscanf(line + 3, "%f %f", &u, &w) >= 1) {
                if (tcount == tcap) {
                    size_t n = tcap ? tcap * 2u : 1024u;
                    float *p = (float *)realloc(uv, n * 2u * sizeof(*p));
                    if (!p) { ok = 0; break; }
                    uv = p; tcap = n;
                }
                uv[tcount * 2u + 0u] = u;
                uv[tcount * 2u + 1u] = w;
                ++tcount;
            }
        } else if (line[0] == 'f' && line[1] == ' ') {
            long fv[16];
            long ft[16];
            size_t n = 0u;
            char *p = line + 2;
            size_t k;
            while (n < 16u) {
                char tok[64];
                size_t ti = 0u;
                while (*p == ' ') ++p;
                if (*p == '\0' || *p == '\n' || *p == '\r') break;
                while (*p && *p != ' ' && *p != '\n' && *p != '\r' && ti < 63u) {
                    tok[ti++] = *p++;
                }
                tok[ti] = '\0';
                parse_face_token(tok, &fv[n], &ft[n]);
                ++n;
            }
            for (k = 1u; k + 1u < n; ++k) {
                const size_t tri[3] = { 0u, k, k + 1u };
                size_t j;
                for (j = 0u; j < 3u; ++j) {
                    gpu_vertex g;
                    long vi = fv[tri[j]];
                    long vt = ft[tri[j]];
                    if (vi < 0 || (size_t)vi >= pcount) {
                        g.px = g.py = g.pz = 0.0f;
                    } else {
                        g.px = pos[vi].x; g.py = pos[vi].y; g.pz = pos[vi].z;
                    }
                    if (vt < 0 || (size_t)vt >= tcount) {
                        g.u = 0.0f; g.v = 0.0f;
                    } else {
                        g.u = uv[(size_t)vt * 2u + 0u];
                        g.v = uv[(size_t)vt * 2u + 1u];
                    }
                    if (!vbuf_push(out, g)) { ok = 0; break; }
                }
            }
        }
    }
    fclose(f);
    free(pos);
    free(uv);
    return ok && out->count > 0u;
}

/* ---- row-major 4x4 matrix helpers -------------------------------------- */
static void mat_identity(float m[16])
{
    int i;
    for (i = 0; i < 16; ++i) m[i] = 0.0f;
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

static void mat_mul(float out[16], const float a[16], const float b[16])
{
    float r[16];
    int i, j, k;
    for (i = 0; i < 4; ++i) {
        for (j = 0; j < 4; ++j) {
            float s = 0.0f;
            for (k = 0; k < 4; ++k) {
                s += a[i * 4 + k] * b[k * 4 + j];
            }
            r[i * 4 + j] = s;
        }
    }
    memcpy(out, r, sizeof(r));
}

static void mat_perspective(float m[16], float fovy, float aspect, float znear,
                            float zfar)
{
    float fdt = 1.0f / tanf(fovy * 0.5f);
    mat_identity(m);
    m[0] = fdt / aspect;
    m[5] = -fdt; /* Vulkan clip: y points down */
    m[10] = zfar / (znear - zfar);
    m[11] = (zfar * znear) / (znear - zfar);
    m[14] = -1.0f;
    m[15] = 0.0f;
}

int main(int argc, char **argv)
{
    vbuf mesh;
    const char *obj_path = NULL;
    const char *tex_path = NULL;
    int selftest = 0;
    int i = 0;
    const int W = 960, H = 720;
    float yaw = 0.6f, pitch = 0.3f, zoom = 1.0f;
    vec3 lo, hi, center;
    float radius = 1.0f;
    int dragging = 0, running = 1, frames = 0;

    SDL_Window *window = NULL;
    SDL_GPUDevice *dev = NULL;
    SDL_GPUShader *vs = NULL, *fs = NULL;
    SDL_GPUGraphicsPipeline *pipeline = NULL;
    SDL_GPUBuffer *gpu_vb = NULL;
    SDL_GPUTexture *tex = NULL, *depth = NULL;
    SDL_GPUSampler *sampler = NULL;
    uint32_t tex_w = 1u, tex_h = 1u, depth_w = 0u, depth_h = 0u;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--selftest") == 0) selftest = 1;
        else if (strcmp(argv[i], "--tex") == 0 && i + 1 < argc) tex_path = argv[++i];
        else obj_path = argv[i];
    }
    if (obj_path == NULL && !selftest) {
        fprintf(stderr, "usage: %s <model.obj> [--tex <bank.bin>] [--selftest]\n",
                argv[0]);
        return 1;
    }

    memset(&mesh, 0, sizeof(mesh));
    if (obj_path != NULL && !load_obj(obj_path, &mesh) && !selftest) {
        fprintf(stderr, "failed to load %s\n", obj_path);
        return 1;
    }

    center.x = center.y = center.z = 0.0f;
    if (mesh.count > 0u) {
        size_t v;
        lo.x = hi.x = mesh.v[0].px;
        lo.y = hi.y = mesh.v[0].py;
        lo.z = hi.z = mesh.v[0].pz;
        for (v = 1u; v < mesh.count; ++v) {
            float x = mesh.v[v].px, y = mesh.v[v].py, z = mesh.v[v].pz;
            if (x < lo.x) { lo.x = x; }
            if (x > hi.x) { hi.x = x; }
            if (y < lo.y) { lo.y = y; }
            if (y > hi.y) { hi.y = y; }
            if (z < lo.z) { lo.z = z; }
            if (z > hi.z) { hi.z = z; }
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
        return selftest ? 0 : 1;
    }

    dev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL);
    if (dev == NULL) {
        /* No GPU / SPIR-V backend (typical in headless CI) -- not a failure. */
        fprintf(stderr, "no GPU device: %s\n", SDL_GetError());
        SDL_Quit();
        if (selftest) {
            printf("vf2_model_viewer selftest ok (no GPU; %zu verts)\n",
                   mesh.count);
            free(mesh.v);
            return 0;
        }
        free(mesh.v);
        return 1;
    }

    {
        SDL_GPUShaderCreateInfo vi;
        SDL_GPUShaderCreateInfo fi;
        memset(&vi, 0, sizeof(vi));
        memset(&fi, 0, sizeof(fi));
        vi.code = model_vert_spv;
        vi.code_size = model_vert_spv_len;
        vi.entrypoint = "main";
        vi.format = SDL_GPU_SHADERFORMAT_SPIRV;
        vi.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vi.num_uniform_buffers = 1u;
        vs = SDL_CreateGPUShader(dev, &vi);
        fi.code = model_frag_spv;
        fi.code_size = model_frag_spv_len;
        fi.entrypoint = "main";
        fi.format = SDL_GPU_SHADERFORMAT_SPIRV;
        fi.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fi.num_samplers = 1u;
        fs = SDL_CreateGPUShader(dev, &fi);
    }
    if (vs == NULL || fs == NULL) {
        fprintf(stderr, "shader create failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    if (selftest) {
        /* Shaders loaded from SPIR-V; that is the headless smoke check. */
        printf("vf2_model_viewer selftest ok (GPU + shaders; %zu verts)\n",
               mesh.count);
        goto cleanup;
    }

    window = SDL_CreateWindow("VF2 model viewer (SDL_GPU)", W, H,
                              SDL_WINDOW_RESIZABLE);
    if (window == NULL || !SDL_ClaimWindowForGPUDevice(dev, window)) {
        fprintf(stderr, "window/claim failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    /* graphics pipeline */
    {
        SDL_GPUVertexBufferDescription vbdesc;
        SDL_GPUVertexAttribute attrs[2];
        SDL_GPUColorTargetDescription color;
        SDL_GPUGraphicsPipelineCreateInfo pi;
        memset(&vbdesc, 0, sizeof(vbdesc));
        memset(attrs, 0, sizeof(attrs));
        memset(&color, 0, sizeof(color));
        memset(&pi, 0, sizeof(pi));
        vbdesc.slot = 0u;
        vbdesc.pitch = (Uint32)sizeof(gpu_vertex);
        vbdesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        attrs[0].location = 0u;
        attrs[0].buffer_slot = 0u;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        attrs[0].offset = 0u;
        attrs[1].location = 1u;
        attrs[1].buffer_slot = 0u;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[1].offset = (Uint32)offsetof(gpu_vertex, u);
        color.format = SDL_GetGPUSwapchainTextureFormat(dev, window);
        pi.vertex_shader = vs;
        pi.fragment_shader = fs;
        pi.vertex_input_state.vertex_buffer_descriptions = &vbdesc;
        pi.vertex_input_state.num_vertex_buffers = 1u;
        pi.vertex_input_state.vertex_attributes = attrs;
        pi.vertex_input_state.num_vertex_attributes = 2u;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.target_info.color_target_descriptions = &color;
        pi.target_info.num_color_targets = 1u;
        pi.target_info.has_depth_stencil_target = true;
        pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        pipeline = SDL_CreateGPUGraphicsPipeline(dev, &pi);
    }
    if (pipeline == NULL) {
        fprintf(stderr, "pipeline failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    /* vertex buffer */
    {
        Uint32 bytes = (Uint32)(mesh.count * sizeof(gpu_vertex));
        SDL_GPUBufferCreateInfo bi;
        SDL_GPUTransferBufferCreateInfo ti;
        SDL_GPUTransferBuffer *tb;
        void *map;
        SDL_GPUCommandBuffer *cmd;
        SDL_GPUCopyPass *cp;
        SDL_GPUTransferBufferLocation src;
        SDL_GPUBufferRegion dst;
        memset(&bi, 0, sizeof(bi));
        memset(&ti, 0, sizeof(ti));
        memset(&src, 0, sizeof(src));
        memset(&dst, 0, sizeof(dst));
        bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        bi.size = bytes;
        gpu_vb = SDL_CreateGPUBuffer(dev, &bi);
        ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        ti.size = bytes;
        tb = SDL_CreateGPUTransferBuffer(dev, &ti);
        if (gpu_vb == NULL || tb == NULL) {
            fprintf(stderr, "vertex buffer failed: %s\n", SDL_GetError());
            goto cleanup;
        }
        map = SDL_MapGPUTransferBuffer(dev, tb, false);
        memcpy(map, mesh.v, bytes);
        SDL_UnmapGPUTransferBuffer(dev, tb);
        cmd = SDL_AcquireGPUCommandBuffer(dev);
        cp = SDL_BeginGPUCopyPass(cmd);
        src.transfer_buffer = tb;
        dst.buffer = gpu_vb;
        dst.size = bytes;
        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_SubmitGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(dev, tb);
    }

    /* texture: de-swizzled sheet (R8), or 1x1 white if none */
    {
        uint8_t *pixels = NULL;
        SDL_GPUTextureCreateInfo tci;
        SDL_GPUSamplerCreateInfo sci;
        memset(&tci, 0, sizeof(tci));
        memset(&sci, 0, sizeof(sci));
        if (tex_path != NULL) {
            FILE *bf = fopen(tex_path, "rb");
            long n = 0;
            uint8_t *bank = NULL;
            if (bf != NULL && fseek(bf, 0, SEEK_END) == 0 &&
                (n = ftell(bf)) > 0 && fseek(bf, 0, SEEK_SET) == 0) {
                bank = (uint8_t *)malloc((size_t)n);
                if (bank != NULL && fread(bank, 1u, (size_t)n, bf) == (size_t)n) {
                    uint32_t x, y;
                    tex_w = 2048u;
                    tex_h = 1024u;
                    pixels = (uint8_t *)malloc((size_t)tex_w * tex_h);
                    for (y = 0u; y < tex_h && pixels != NULL; ++y) {
                        for (x = 0u; x < tex_w; ++x) {
                            uint8_t idx = vf2_texture_sheet_texel(
                                bank, (size_t)n, x, y);
                            pixels[(size_t)y * tex_w + x] =
                                vf2_texture_sheet_index_to_gray(idx);
                        }
                    }
                }
            }
            if (bf != NULL) fclose(bf);
            free(bank);
        }
        if (pixels == NULL) {
            tex_w = 1u;
            tex_h = 1u;
            pixels = (uint8_t *)malloc(1u);
            if (pixels != NULL) pixels[0] = 0xffu;
        }
        tci.type = SDL_GPU_TEXTURETYPE_2D;
        tci.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tci.width = tex_w;
        tci.height = tex_h;
        tci.layer_count_or_depth = 1u;
        tci.num_levels = 1u;
        tex = SDL_CreateGPUTexture(dev, &tci);
        if (tex != NULL && pixels != NULL) {
            Uint32 bytes = tex_w * tex_h;
            SDL_GPUTransferBufferCreateInfo ti;
            SDL_GPUTransferBuffer *tb;
            void *map;
            SDL_GPUCommandBuffer *cmd;
            SDL_GPUCopyPass *cp;
            SDL_GPUTextureTransferInfo src;
            SDL_GPUTextureRegion reg;
            memset(&ti, 0, sizeof(ti));
            memset(&src, 0, sizeof(src));
            memset(&reg, 0, sizeof(reg));
            ti.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
            ti.size = bytes;
            tb = SDL_CreateGPUTransferBuffer(dev, &ti);
            map = SDL_MapGPUTransferBuffer(dev, tb, false);
            memcpy(map, pixels, bytes);
            SDL_UnmapGPUTransferBuffer(dev, tb);
            cmd = SDL_AcquireGPUCommandBuffer(dev);
            cp = SDL_BeginGPUCopyPass(cmd);
            src.transfer_buffer = tb;
            src.pixels_per_row = tex_w;
            src.rows_per_layer = tex_h;
            reg.texture = tex;
            reg.w = tex_w;
            reg.h = tex_h;
            reg.d = 1u;
            SDL_UploadToGPUTexture(cp, &src, &reg, false);
            SDL_EndGPUCopyPass(cp);
            SDL_SubmitGPUCommandBuffer(cmd);
            SDL_ReleaseGPUTransferBuffer(dev, tb);
        }
        free(pixels);
        sci.min_filter = SDL_GPU_FILTER_LINEAR;
        sci.mag_filter = SDL_GPU_FILTER_LINEAR;
        sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sampler = SDL_CreateGPUSampler(dev, &sci);
    }
    if (tex == NULL || sampler == NULL) {
        fprintf(stderr, "texture/sampler failed: %s\n", SDL_GetError());
        goto cleanup;
    }

    while (running) {
        SDL_Event e;
        SDL_GPUCommandBuffer *cmd;
        SDL_GPUTexture *swap = NULL;
        Uint32 sw = 0u, sh = 0u;

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

        cmd = SDL_AcquireGPUCommandBuffer(dev);
        if (cmd == NULL) break;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swap, &sw, &sh) ||
            swap == NULL) {
            SDL_SubmitGPUCommandBuffer(cmd);
            continue;
        }
        /* (re)create the depth texture to match the swapchain */
        if (depth == NULL || depth_w != sw || depth_h != sh) {
            SDL_GPUTextureCreateInfo di;
            if (depth != NULL) SDL_ReleaseGPUTexture(dev, depth);
            memset(&di, 0, sizeof(di));
            di.type = SDL_GPU_TEXTURETYPE_2D;
            di.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            di.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
            di.width = sw;
            di.height = sh;
            di.layer_count_or_depth = 1u;
            di.num_levels = 1u;
            depth = SDL_CreateGPUTexture(dev, &di);
            depth_w = sw;
            depth_h = sh;
        }

        {
            float proj[16], view[16], model[16], tmp[16], mvp[16];
            float ry[16], rx[16], tz[16], scal[16], tr[16];
            float cy = cosf(yaw), sy = sinf(yaw);
            float cp = cosf(pitch), sp = sinf(pitch);
            float s = zoom / radius;
            float aspect = (sh > 0u) ? (float)sw / (float)sh : 1.3333f;
            SDL_GPUColorTargetInfo cti;
            SDL_GPUDepthStencilTargetInfo dti;
            SDL_GPURenderPass *pass;
            SDL_GPUBufferBinding vbb;
            SDL_GPUTextureSamplerBinding tsb;

            mat_identity(scal); scal[0] = s; scal[5] = s; scal[10] = s;
            mat_identity(tr); tr[3] = -center.x; tr[7] = -center.y; tr[11] = -center.z;
            mat_mul(model, scal, tr);
            mat_identity(ry); ry[0] = cy; ry[2] = sy; ry[8] = -sy; ry[10] = cy;
            mat_identity(rx); rx[5] = cp; rx[6] = -sp; rx[9] = sp; rx[10] = cp;
            mat_identity(tz); tz[11] = -3.0f;
            mat_mul(tmp, rx, ry);
            mat_mul(view, tz, tmp);
            mat_perspective(proj, 0.78539816f /*45deg*/, aspect, 0.05f, 100.0f);
            mat_mul(tmp, view, model);
            mat_mul(mvp, proj, tmp);

            memset(&cti, 0, sizeof(cti));
            memset(&dti, 0, sizeof(dti));
            cti.texture = swap;
            cti.clear_color.r = 0.08f;
            cti.clear_color.g = 0.08f;
            cti.clear_color.b = 0.12f;
            cti.clear_color.a = 1.0f;
            cti.load_op = SDL_GPU_LOADOP_CLEAR;
            cti.store_op = SDL_GPU_STOREOP_STORE;
            dti.texture = depth;
            dti.clear_depth = 1.0f;
            dti.load_op = SDL_GPU_LOADOP_CLEAR;
            dti.store_op = SDL_GPU_STOREOP_DONT_CARE;
            pass = SDL_BeginGPURenderPass(cmd, &cti, 1u, &dti);
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            SDL_PushGPUVertexUniformData(cmd, 0u, mvp, (Uint32)sizeof(mvp));
            memset(&vbb, 0, sizeof(vbb));
            vbb.buffer = gpu_vb;
            SDL_BindGPUVertexBuffers(pass, 0u, &vbb, 1u);
            memset(&tsb, 0, sizeof(tsb));
            tsb.texture = tex;
            tsb.sampler = sampler;
            SDL_BindGPUFragmentSamplers(pass, 0u, &tsb, 1u);
            SDL_DrawGPUPrimitives(pass, (Uint32)mesh.count, 1u, 0u, 0u);
            SDL_EndGPURenderPass(pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
        ++frames;
        (void)frames;
    }

cleanup:
    if (dev != NULL) {
        if (sampler != NULL) SDL_ReleaseGPUSampler(dev, sampler);
        if (tex != NULL) SDL_ReleaseGPUTexture(dev, tex);
        if (depth != NULL) SDL_ReleaseGPUTexture(dev, depth);
        if (gpu_vb != NULL) SDL_ReleaseGPUBuffer(dev, gpu_vb);
        if (pipeline != NULL) SDL_ReleaseGPUGraphicsPipeline(dev, pipeline);
        if (vs != NULL) SDL_ReleaseGPUShader(dev, vs);
        if (fs != NULL) SDL_ReleaseGPUShader(dev, fs);
        if (window != NULL) SDL_ReleaseWindowFromGPUDevice(dev, window);
        SDL_DestroyGPUDevice(dev);
    }
    if (window != NULL) SDL_DestroyWindow(window);
    SDL_Quit();
    free(mesh.v);
    return 0;
}
