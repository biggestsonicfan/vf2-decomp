/* vf2_host: SDL3 window / input / audio shell around the portable core.
 * This is the only translation unit that includes SDL headers.  It never
 * embeds a ROM or asset; --rom-dir and --snapshot are supplied by the user. */

#include <SDL3/SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/file.h"
#include "vf2/host.h"
#include "vf2/platform.h"
#include "vf2/session.h"
#include "vf2/version.h"

#define FRAMES_PER_SECOND 60u
#define AUDIO_FRAMES_PER_VIDEO_FRAME (VF2_SESSION_SAMPLE_RATE / FRAMES_PER_SECOND)

typedef struct host {
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    SDL_AudioStream *audio;
    SDL_Mutex *audio_lock;
    vf2_host_audio_ring ring;
    uint64_t audio_consumed; /* frames handed to the device (under lock) */
    vf2_host_bindings bindings;
    vf2_host_input input;
    int quit;
    int paused;
    int step_once;
    int integer_scale;
    int fullscreen;
} host;

static void SDLCALL audio_callback(
    void *userdata, SDL_AudioStream *stream, int additional, int total
)
{
    host *h = (host *)userdata;
    int16_t buffer[2048 * 2];
    (void)total;
    while (additional > 0) {
        int frames = additional / 4;
        if (frames > 2048) frames = 2048;
        if (frames <= 0) break;
        SDL_LockMutex(h->audio_lock);
        vf2_host_audio_ring_read(&h->ring, buffer, (size_t)frames);
        h->audio_consumed += (uint64_t)frames;
        SDL_UnlockMutex(h->audio_lock);
        SDL_PutAudioStreamData(stream, buffer, frames * 4);
        additional -= frames * 4;
    }
}

static void host_push_audio(host *h, const int16_t *l, const int16_t *r, size_t n)
{
    SDL_LockMutex(h->audio_lock);
    (void)vf2_host_audio_ring_write(&h->ring, l, r, n);
    SDL_UnlockMutex(h->audio_lock);
}

static void source_name(char *out, size_t size, const char *prefix, const char *name)
{
    SDL_snprintf(out, size, "%s%s", prefix, name != NULL ? name : "");
}

static void toggle_fullscreen(host *h)
{
    h->fullscreen = !h->fullscreen;
    SDL_SetWindowFullscreen(h->window, h->fullscreen != 0);
}

static void handle_event(host *h, const SDL_Event *e)
{
    char src[48];

    switch (e->type) {
    case SDL_EVENT_QUIT:
        h->quit = 1;
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
        const int down = e->type == SDL_EVENT_KEY_DOWN;
        if (e->key.repeat) break;
        if (down) {
            switch (e->key.scancode) {
            case SDL_SCANCODE_ESCAPE: h->quit = 1; return;
            case SDL_SCANCODE_P: h->paused = !h->paused; return;
            case SDL_SCANCODE_N: h->step_once = 1; return;
            case SDL_SCANCODE_F10: h->integer_scale = !h->integer_scale; return;
            case SDL_SCANCODE_F11: toggle_fullscreen(h); return;
            default: break;
            }
        }
        source_name(src, sizeof(src), "", SDL_GetScancodeName(e->key.scancode));
        vf2_host_input_event(&h->input, src, down);
        break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
        (void)SDL_OpenGamepad(e->gdevice.which);
        break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        source_name(src, sizeof(src), "pad:",
            SDL_GetGamepadStringForButton((SDL_GamepadButton)e->gbutton.button));
        vf2_host_input_event(&h->input, src, e->type == SDL_EVENT_GAMEPAD_BUTTON_DOWN);
        break;
    default:
        break;
    }
}

static int host_open(host *h, const char *title)
{
    SDL_AudioSpec spec;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 0;
    }
    h->window = SDL_CreateWindow(
        title, (int)VF2_SESSION_WIDTH * 2, (int)VF2_SESSION_HEIGHT * 2,
        SDL_WINDOW_RESIZABLE
    );
    if (h->window != NULL) h->renderer = SDL_CreateRenderer(h->window, NULL);
    if (h->renderer != NULL) {
        h->texture = SDL_CreateTexture(
            h->renderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING,
            (int)VF2_SESSION_WIDTH, (int)VF2_SESSION_HEIGHT
        );
    }
    if (h->texture == NULL) {
        fprintf(stderr, "video: %s\n", SDL_GetError());
        return 0;
    }
    SDL_SetTextureScaleMode(h->texture, SDL_SCALEMODE_NEAREST);

    h->audio_lock = SDL_CreateMutex();
    if (h->audio_lock == NULL ||
        vf2_host_audio_ring_init(&h->ring, VF2_SESSION_SAMPLE_RATE / 4u) != VF2_OK) {
        fprintf(stderr, "audio ring: out of memory\n");
        return 0;
    }
    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = (int)VF2_SESSION_SAMPLE_RATE;
    h->audio = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, audio_callback, h
    );
    if (h->audio != NULL) {
        SDL_ResumeAudioStreamDevice(h->audio);
    } else {
        fprintf(stderr, "warning: no audio device (%s); running silent\n",
                SDL_GetError());
    }
    return 1;
}

static void host_close(host *h)
{
    if (h->audio != NULL) SDL_DestroyAudioStream(h->audio);
    if (h->audio_lock != NULL) SDL_DestroyMutex(h->audio_lock);
    vf2_host_audio_ring_destroy(&h->ring);
    if (h->texture != NULL) SDL_DestroyTexture(h->texture);
    if (h->renderer != NULL) SDL_DestroyRenderer(h->renderer);
    if (h->window != NULL) SDL_DestroyWindow(h->window);
    SDL_Quit();
}

static void host_present(host *h, const uint32_t *pixels)
{
    int ww = 0, wh = 0;
    vf2_host_rect r;
    SDL_FRect dst;

    SDL_UpdateTexture(h->texture, NULL, pixels, (int)VF2_SESSION_WIDTH * 4);
    SDL_GetWindowSize(h->window, &ww, &wh);
    SDL_GetCurrentRenderOutputSize(h->renderer, &ww, &wh);
    r = vf2_host_fit(ww, wh, (int)VF2_SESSION_WIDTH, (int)VF2_SESSION_HEIGHT,
                     h->integer_scale);
    dst.x = (float)r.x; dst.y = (float)r.y; dst.w = (float)r.w; dst.h = (float)r.h;
    SDL_SetRenderDrawColor(h->renderer, 0, 0, 0, 255);
    SDL_RenderClear(h->renderer);
    SDL_RenderTexture(h->renderer, h->texture, NULL, &dst);
    SDL_RenderPresent(h->renderer);
}

/* Binary PPM of an XRGB8888 frame; used by --screenshot for CI and bug reports. */
static int write_ppm(const char *path, const uint32_t *px)
{
    FILE *f = fopen(path, "wb");
    size_t n = (size_t)VF2_SESSION_WIDTH * VF2_SESSION_HEIGHT;
    if (f == NULL) return 0;
    fprintf(f, "P6\n%u %u\n255\n", (unsigned)VF2_SESSION_WIDTH,
            (unsigned)VF2_SESSION_HEIGHT);
    for (size_t i = 0u; i < n; ++i) {
        const unsigned char rgb[3] = {
            (unsigned char)(px[i] >> 16), (unsigned char)(px[i] >> 8),
            (unsigned char)px[i]};
        if (fwrite(rgb, 1u, 3u, f) != 3u) { fclose(f); return 0; }
    }
    return fclose(f) == 0;
}

static void usage(const char *p)
{
    fprintf(stderr,
        "Usage: %s --rom-dir <dir> --snapshot <file> [--bindings <ini>]\n"
        "          [--frames <n>] [--fullscreen] [--integer]\n"
        "          [--screenshot <file.ppm>] [--digest <n>] [--fast] [--oracle]\n"
        "       %s --selftest [--frames <n>]\n"
        "Keys: arrows, Z/X/C, Enter, 5=coin, F1=test, F2=service, P2 on IJKL/ASD;\n"
        "      P pause, N step, F10 integer scale, F11 fullscreen, Esc quit.\n",
        p, p);
}

static int load_bindings(host *h, const char *path)
{
    uint8_t *data = NULL;
    char *text = NULL;
    size_t size = 0u, bad = 0u;
    vf2_status st = vf2_read_file(path, &data, &size);
    if (st != VF2_OK) {
        fprintf(stderr, "bindings: cannot read %s\n", path);
        return 0;
    }
    text = (char *)realloc(data, size + 1u);
    if (text == NULL) {
        free(data);
        return 0;
    }
    text[size] = '\0';
    st = vf2_host_bindings_parse(&h->bindings, text, &bad);
    free(text);
    if (st != VF2_OK) {
        fprintf(stderr, "bindings: %s line %zu rejected\n", path, bad);
        return 0;
    }
    return 1;
}

/* Inject a key through SDL's own queue so the real event path is exercised. */
static void inject_key(SDL_Scancode sc, int down)
{
    SDL_Event e;
    SDL_zero(e);
    e.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
    e.key.scancode = sc;
    e.key.down = down != 0;
    SDL_PushEvent(&e);
}

static void pump(host *h)
{
    SDL_Event e;
    while (SDL_PollEvent(&e)) handle_event(h, &e);
}

static int selftest(host *h, uint32_t frames)
{
    static uint32_t pixels[VF2_SESSION_WIDTH * VF2_SESSION_HEIGHT];
    int16_t l[AUDIO_FRAMES_PER_VIDEO_FRAME], r[AUDIO_FRAMES_PER_VIDEO_FRAME];
    int failures = 0;

    for (uint32_t f = 0u; f < frames; ++f) {
        if (f == 2u) inject_key(SDL_SCANCODE_Z, 1);
        if (f == 3u) inject_key(SDL_SCANCODE_LEFT, 1);
        pump(h);
        if (f == 4u) {
            const uint32_t want = VF2_PLATFORM_BUTTON_PUNCH | VF2_PLATFORM_BUTTON_LEFT;
            if (vf2_host_input_mask(&h->input) != want) {
                fprintf(stderr, "selftest: mask 0x%x, want 0x%x\n",
                        (unsigned)vf2_host_input_mask(&h->input), (unsigned)want);
                ++failures;
            }
            inject_key(SDL_SCANCODE_Z, 0);
            inject_key(SDL_SCANCODE_LEFT, 0);
        }
        if (f == 6u && vf2_host_input_mask(&h->input) != 0u) {
            fprintf(stderr, "selftest: keys not released\n");
            ++failures;
        }
        for (size_t i = 0u; i < sizeof(pixels) / sizeof(pixels[0]); ++i) {
            pixels[i] = 0x00202020u + (uint32_t)((i + f) & 0xffu);
        }
        for (size_t i = 0u; i < AUDIO_FRAMES_PER_VIDEO_FRAME; ++i) {
            l[i] = (int16_t)(((i / 50u) & 1u) ? 1000 : -1000);
            r[i] = l[i];
        }
        host_push_audio(h, l, r, AUDIO_FRAMES_PER_VIDEO_FRAME);
        host_present(h, pixels);
        SDL_Delay(1000u / FRAMES_PER_SECOND);
    }
    if (h->audio != NULL) {
        SDL_LockMutex(h->audio_lock);
        if (h->audio_consumed == 0u) {
            fprintf(stderr, "selftest: audio device consumed nothing\n");
            ++failures;
        }
        SDL_UnlockMutex(h->audio_lock);
    }
    printf("host selftest: %s (%s video, %s audio)\n",
           failures == 0 ? "ok" : "FAILED",
           SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver());
    return failures == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    host h;
    const char *rom_dir = NULL, *snapshot = NULL, *bindings_path = NULL;
    const char *screenshot = NULL;
    uint32_t digest_every = 0u;
    int fast = 0;
    int oracle = 0;
    uint32_t frame_limit = 0u;
    int do_selftest = 0, rc = 0;
    vf2_session *session = NULL;
    vf2_status st;
    char title[96];

    memset(&h, 0, sizeof(h));
    vf2_host_bindings_defaults(&h.bindings);
    vf2_host_input_init(&h.input, &h.bindings);

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--rom-dir") == 0 && i + 1 < argc) rom_dir = argv[++i];
        else if (strcmp(argv[i], "--snapshot") == 0 && i + 1 < argc) snapshot = argv[++i];
        else if (strcmp(argv[i], "--bindings") == 0 && i + 1 < argc) bindings_path = argv[++i];
        else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) frame_limit = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) screenshot = argv[++i];
        else if (strcmp(argv[i], "--fast") == 0) fast = 1;
        else if (strcmp(argv[i], "--oracle") == 0) oracle = 1;
        else if (strcmp(argv[i], "--digest") == 0 && i + 1 < argc) digest_every = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--fullscreen") == 0) h.fullscreen = 1;
        else if (strcmp(argv[i], "--integer") == 0) h.integer_scale = 1;
        else if (strcmp(argv[i], "--selftest") == 0) do_selftest = 1;
        else { usage(argv[0]); return EXIT_FAILURE; }
    }
    if (!do_selftest && (rom_dir == NULL || snapshot == NULL)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (bindings_path != NULL && !load_bindings(&h, bindings_path)) return EXIT_FAILURE;

    SDL_snprintf(title, sizeof(title), "vf2_host v%s", VF2_VERSION_STRING);
    if (!host_open(&h, title)) {
        host_close(&h);
        return EXIT_FAILURE;
    }
    if (h.fullscreen) SDL_SetWindowFullscreen(h.window, true);

    if (do_selftest) {
        rc = selftest(&h, frame_limit != 0u ? frame_limit : 30u);
        host_close(&h);
        return rc;
    }

    st = vf2_session_open(rom_dir, snapshot, &session);
    if (st != VF2_OK) {
        fprintf(stderr, "session: %s\n", vf2_status_string(st));
        host_close(&h);
        return EXIT_FAILURE;
    }

    if (vf2_session_audio_status(session) != VF2_OK) {
        fprintf(stderr, "note: audio unavailable (%s); running silent\n",
                vf2_status_string(vf2_session_audio_status(session)));
    }
    {
        int16_t left[AUDIO_FRAMES_PER_VIDEO_FRAME], right[AUDIO_FRAMES_PER_VIDEO_FRAME];
        const uint64_t period = SDL_NS_PER_SECOND / FRAMES_PER_SECOND;
        uint64_t next = SDL_GetTicksNS();
        uint32_t frame = 0u;

        while (!h.quit && (frame_limit == 0u || frame < frame_limit)) {
            pump(&h);
            if (!h.paused || h.step_once) {
                h.step_once = 0;
                st = oracle
                    ? vf2_session_step_oracle(session, vf2_host_input_mask(&h.input))
                    : vf2_session_step(session, vf2_host_input_mask(&h.input));
                if (st != VF2_OK) {
                    fprintf(stderr, "frame %u failed: %s\n", (unsigned)frame,
                            vf2_status_string(st));
                    rc = EXIT_FAILURE;
                    break;
                }
                /* Live-sound bridge (T3): drain newly filed queue
                 * bytes into the board and dispatch them. Idle
                 * unless game code filed commands this frame. */
                {
                    vf2_session_audio_pump_report audio_pump;
                    memset(&audio_pump, 0, sizeof(audio_pump));
                    if (vf2_session_pump_audio(session, &audio_pump) ==
                            VF2_OK &&
                        audio_pump.bytes_moved != 0u) {
                        fprintf(stderr, "frame %u: audio pump bytes=%u filed=%u dispatched=%u refused=%u\n",
                                (unsigned)frame,
                                (unsigned)audio_pump.bytes_moved,
                                (unsigned)audio_pump.entries_filed,
                                (unsigned)audio_pump.entries_dispatched,
                                (unsigned)audio_pump.entries_refused);
                    }
                }
                if (vf2_session_render_audio(session, left, right,
                        AUDIO_FRAMES_PER_VIDEO_FRAME) == VF2_OK) {
                    host_push_audio(&h, left, right, AUDIO_FRAMES_PER_VIDEO_FRAME);
                }
                ++frame;
                if (digest_every != 0u && frame % digest_every == 0u) {
                    uint32_t d = 0u;
                    if (vf2_session_work_ram_digest(session, &d) == VF2_OK) {
                        printf("digest frame %u 0x%08x\n", (unsigned)frame, (unsigned)d);
                    }
                }
            }
            if (vf2_session_pixels(session) != NULL) {
                host_present(&h, vf2_session_pixels(session));
            }
            next += period;
            {
                const uint64_t now = SDL_GetTicksNS();
                if (fast) next = now;
                else if (next > now) SDL_DelayPrecise(next - now);
                else if (now - next > period * 5u) next = now; /* don't spiral */
            }
        }
        {
            /* Phase 2 baseline: how much of the frame loop still runs through
             * the i960 interpreter versus recovered C.  A fully recovered
             * session reaches interpreted=0 (plan phase 4). */
            uint64_t executed = 0u, interpreted = 0u;
            vf2_session_instruction_counts(session, &executed, &interpreted);
            printf("interp mode %s frames %u executed %llu interpreted %llu recovered %llu\n",
                   oracle ? "oracle" : "native",
                   (unsigned)frame,
                   (unsigned long long)executed,
                   (unsigned long long)interpreted,
                   (unsigned long long)(executed - interpreted));
        }
    }
    if (screenshot != NULL && vf2_session_pixels(session) != NULL &&
        !write_ppm(screenshot, vf2_session_pixels(session))) {
        fprintf(stderr, "screenshot: cannot write %s\n", screenshot);
        rc = EXIT_FAILURE;
    }
    vf2_session_close(session);
    host_close(&h);
    return rc;
}
