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
        "          [--audio-evidence-jsonl <output.jsonl>] (requires --oracle)\n"
        "          [--audio-pre-event-snapshot <output.vf2snap>] (requires --oracle)\n"
        "          [--audio-pre-event-pc <original-i960-pc>] (checkpoint only on PC match)\n"
        "          [--audio-pre-event-word <u32>] (checkpoint only on matching command)\n"
        "          [--verified-audio-copy] (owner-ROM D92 upload opt-in)\n"
        "          [--verified-audio-90] (owner-ROM frame342 9E KEYON opt-in)\n"
        "          [--input-cycle <mask,mask,...>] (repeat deterministic controller inputs)\n"
        "       %s --selftest [--frames <n>]\n"
        "Keys: arrows, Z/X/C, Enter, 5=coin, F1=test, F2=service, P2 on IJKL/ASD;\n"
        "      P pause, N step, F10 integer scale, F11 fullscreen, Esc quit.\n",
        p, p);
}

/* Deterministic HEADLESS gameplay input, never synthetic audio packets.
 * Each comma-separated mask applies to one frame; the pattern repeats.
 * Strict decimal/0x integer parsing rejects truncation, blanks and
 * invalid characters. The masks are ordinary game-controller bits. */
enum { VF2_HOST_INPUT_CYCLE_CAP = 64u };

static int parse_input_cycle(
    const char *text,
    uint32_t *values,
    size_t *count
)
{
    const char *cursor = text;
    size_t n = 0u;
    if (cursor == NULL || *cursor == '\0' || values == NULL ||
        count == NULL) return 0;
    for (;;) {
        char *end = NULL;
        unsigned long long value;
        if (*cursor == '-' || *cursor == ',' ||
            n == VF2_HOST_INPUT_CYCLE_CAP) return 0;
        value = strtoull(cursor, &end, 0);
        if (end == cursor || value > UINT32_MAX ||
            (*end != ',' && *end != '\0')) return 0;
        values[n++] = (uint32_t)value;
        if (*end == '\0') break;
        if (end[1] == '\0') return 0;
        cursor = end + 1;
    }
    *count = n;
    return 1;
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
    {
        uint32_t masks[VF2_HOST_INPUT_CYCLE_CAP] = {0u};
        size_t n = 0u;
        if (!parse_input_cycle("0x100,0,0x80,0x10", masks, &n) ||
            n != 4u || masks[0] != 0x100u ||
            masks[1] != 0u || masks[2] != 0x80u ||
            masks[3] != 0x10u ||
            parse_input_cycle("0x100,", masks, &n) ||
            parse_input_cycle("0,,1", masks, &n) ||
            parse_input_cycle("0x100garbage", masks, &n) ||
            parse_input_cycle("4294967296", masks, &n) ||
            parse_input_cycle("-1", masks, &n)) {
            fprintf(stderr, "selftest: deterministic input-cycle parser failed\n");
            ++failures;
        }
    }

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

/* This observer is active ONLY during vf2_session_step_oracle,
 * never during the subsequent shadow UART pump. A successful 4-byte
 * write to an aligned original sound-queue slot is a strong original
 * i960 PRODUCER-BUS witness, but it does not identify writer IP
 * or prove the selected command was a fighter's hit. */
typedef struct sound_writer_capture {
    vf2_session *session;
    int original_frame_active;
    uint32_t observed_writes;
    uint32_t first_address;
    uint32_t first_command;
    uint32_t first_writer_pc;
    int first_writer_pc_proven;
    int filter_by_pc;
    uint32_t requested_writer_pc;
    int filter_by_word;
    uint32_t requested_writer_word;
    int matched_pre_event;
    uint32_t matched_slot;
    uint32_t matched_word;
    uint32_t matched_pc;
} sound_writer_capture;

static void sound_writer_observe(
    const vf2_model2a_memory_access *access,
    void *context
)
{
    sound_writer_capture *w = (sound_writer_capture *)context;
    const uint8_t *data;
    if (w == NULL || !w->original_frame_active || access == NULL ||
        access->kind != VF2_MODEL2A_MEMORY_WRITE ||
        access->size != 4u || access->data == NULL ||
        access->address < UINT32_C(0x00504020) ||
        access->address >= UINT32_C(0x00504060) ||
        (access->address & UINT32_C(3)) != 0u) {
        return;
    }
    data = (const uint8_t *)access->data;
    if (w->observed_writes == 0u) {
        uint32_t instruction_ip = 0u;
        w->first_writer_pc_proven =
            vf2_session_current_oracle_instruction_ip(
                w->session, &instruction_ip) == VF2_OK;
        w->first_writer_pc =
            w->first_writer_pc_proven ? instruction_ip : 0u;
        w->first_address = access->address;
        w->first_command = (uint32_t)data[0] |
            ((uint32_t)data[1] << 8u) |
            ((uint32_t)data[2] << 16u) |
            ((uint32_t)data[3] << 24u);
    }
    if (!w->matched_pre_event) {
        uint32_t ip = 0u;
        uint32_t word = (uint32_t)data[0] |
            ((uint32_t)data[1] << 8u) |
            ((uint32_t)data[2] << 16u) |
            ((uint32_t)data[3] << 24u);
        int ip_valid = vf2_session_current_oracle_instruction_ip(
            w->session, &ip) == VF2_OK;
        if ((!w->filter_by_pc ||
             (ip_valid && ip == w->requested_writer_pc)) &&
            (!w->filter_by_word || word == w->requested_writer_word)) {
            w->matched_pre_event = 1;
            w->matched_slot = access->address;
            w->matched_word = word;
            w->matched_pc = ip_valid ? ip : 0u;
        }
    }
    if (w->observed_writes < UINT32_MAX) {
        ++w->observed_writes;
    }
}

/* Optional frame-level evidence: these are real oracle-session frame
 * observations, but the UART forwarding path remains a SHADOW i960
 * transport and cannot establish the original ISR PC nor fight-hit
 * causality. Never group bytes into a supposed original packet: a
 * partially transmitted packet may straddle the frame boundary. */
static int write_audio_evidence_frame(
    FILE *out,
    uint32_t frame,
    uint32_t input,
    uint8_t queue_before,
    uint8_t queue_after,
    const sound_writer_capture *writer,
    const vf2_session_audio_pump_report *pump_report,
    vf2_status pump_status,
    vf2_status render_status,
    const int16_t *left,
    const int16_t *right,
    size_t frames
)
{
    size_t i;
    size_t nonzero = 0u;
    unsigned peak = 0u;
    if (out == NULL || writer == NULL || pump_report == NULL ||
        pump_report->observed_uart_count >
            VF2_SESSION_AUDIO_UART_SAMPLE_CAP) {
        return 0;
    }
    if (render_status == VF2_OK) {
        for (i = 0u; i < frames; ++i) {
            int32_t l = left[i], r = right[i];
            unsigned al = (unsigned)(l < 0 ? -l : l);
            unsigned ar = (unsigned)(r < 0 ? -r : r);
            if (al > peak) peak = al;
            if (ar > peak) peak = ar;
            if (l != 0 || r != 0) ++nonzero;
        }
    }
    if (fprintf(out,
                "{\"type\":\"vf2_audio_frame\",\"frame\":%u,\"input\":%u,"
                "\"origin\":\"oracle_frame_shadow_uart_pump\","
                "\"queue_before_step\":%u,\"queue_after_step\":%u,"
                "\"original_i960_ring_writes\":%u,"
                "\"original_i960_first_ring_slot\":%u,"
                "\"original_i960_first_ring_word\":%u,"
                "\"original_i960_first_writer_pc\":%u,"
                "\"original_i960_writer_pc_proven\":%s,"
                "\"original_i960_target_writer_observed\":%s,"
                "\"original_i960_target_writer_pc\":%u,"
                "\"original_i960_target_slot\":%u,"
                "\"original_i960_target_word\":%u,"
                "\"uart_forwarded_hex\":\"",
                (unsigned)frame, (unsigned)input,
                (unsigned)queue_before, (unsigned)queue_after,
                (unsigned)writer->observed_writes,
                (unsigned)writer->first_address,
                (unsigned)writer->first_command,
                (unsigned)writer->first_writer_pc,
                writer->first_writer_pc_proven ? "true" : "false",
                writer->matched_pre_event ? "true" : "false",
                (unsigned)writer->matched_pc,
                (unsigned)writer->matched_slot,
                (unsigned)writer->matched_word) < 0) {
        return 0;
    }
    for (i = 0u; i < pump_report->observed_uart_count; ++i) {
        if (fprintf(out, "%02x",
                    (unsigned)pump_report->observed_uart[i]) < 0) {
            return 0;
        }
    }
    if (fprintf(out,
                "\",\"uart_count\":%u,\"uart_truncated\":%u,"
                "\"bytes_moved\":%u,\"entries_filed\":%u,"
                "\"entries_dispatched\":%u,\"entries_refused\":%u,"
                "\"d92_keyons_retired\":%u,\"keyon_90_retired\":%u,"
                "\"pump_status\":%d,"
                "\"drain_status\":%d,\"stream_status\":%d,"
                "\"voice_status\":%d,\"render_status\":%d,"
                "\"pcm_frames\":%u,\"pcm_nonzero_frames\":%u,"
                "\"pcm_peak\":%u,\"natural_fighter_hit_proven\":false,"
                "\"original_uart_irq_proven\":false,"
                "\"hardware_pcm_proven\":false}\n",
                (unsigned)pump_report->observed_uart_count,
                (unsigned)pump_report->observed_uart_truncated,
                (unsigned)pump_report->bytes_moved,
                (unsigned)pump_report->entries_filed,
                (unsigned)pump_report->entries_dispatched,
                (unsigned)pump_report->entries_refused,
                (unsigned)pump_report->d92_keyons_retired,
                (unsigned)pump_report->keyon_90_retired,
                (int)pump_status, (int)pump_report->drain_status,
                (int)pump_report->stream_status,
                (int)pump_report->voice_status, (int)render_status,
                (unsigned)(render_status == VF2_OK ? frames : 0u),
                (unsigned)nonzero, peak) < 0) {
        return 0;
    }
    return fflush(out) == 0;
}

int main(int argc, char **argv)
{
    host h;
    const char *rom_dir = NULL, *snapshot = NULL, *bindings_path = NULL;
    const char *screenshot = NULL;
    const char *audio_evidence_path = NULL;
    const char *audio_pre_event_path = NULL;
    const char *input_cycle_text = NULL;
    const char *pre_event_pc_text = NULL;
    const char *pre_event_word_text = NULL;
    uint32_t input_cycle[VF2_HOST_INPUT_CYCLE_CAP] = {0u};
    size_t input_cycle_count = 0u;
    vf2_i960_snapshot pre_event;
    sound_writer_capture writer = {0};
    int pre_event_written = 0;
    FILE *audio_evidence = NULL;
    uint32_t digest_every = 0u;
    int fast = 0;
    int oracle = 0;
    int verified_audio_copy = 0;
    int verified_audio_90 = 0;
    uint32_t frame_limit = 0u;
    int do_selftest = 0, rc = 0;
    vf2_session *session = NULL;
    vf2_status st;
    char title[96];

    vf2_i960_snapshot_init(&pre_event);
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
        else if (strcmp(argv[i], "--verified-audio-copy") == 0)
            verified_audio_copy = 1;
        else if (strcmp(argv[i], "--verified-audio-90") == 0)
            verified_audio_90 = 1;
        else if (strcmp(argv[i], "--audio-evidence-jsonl") == 0 &&
                 i + 1 < argc)
            audio_evidence_path = argv[++i];
        else if (strcmp(argv[i], "--audio-pre-event-snapshot") == 0 &&
                 i + 1 < argc)
            audio_pre_event_path = argv[++i];
        else if (strcmp(argv[i], "--input-cycle") == 0 &&
                 i + 1 < argc)
            input_cycle_text = argv[++i];
        else if (strcmp(argv[i], "--audio-pre-event-pc") == 0 &&
                 i + 1 < argc)
            pre_event_pc_text = argv[++i];
        else if (strcmp(argv[i], "--audio-pre-event-word") == 0 &&
                 i + 1 < argc)
            pre_event_word_text = argv[++i];
        else if (strcmp(argv[i], "--digest") == 0 && i + 1 < argc) digest_every = (uint32_t)strtoul(argv[++i], NULL, 0);
        else if (strcmp(argv[i], "--fullscreen") == 0) h.fullscreen = 1;
        else if (strcmp(argv[i], "--integer") == 0) h.integer_scale = 1;
        else if (strcmp(argv[i], "--selftest") == 0) do_selftest = 1;
        else { usage(argv[0]); return EXIT_FAILURE; }
    }
    /* Evidence requires ORIGINAL i960 frame stepping; never log
     * recovered-native frames as an original oracle observation. */
    if ((audio_evidence_path != NULL || audio_pre_event_path != NULL) &&
        (!oracle || do_selftest)) {
        fprintf(stderr,
                "audio evidence/snapshot capture requires --oracle and a snapshot\n");
        return EXIT_FAILURE;
    }
    if (!do_selftest && (rom_dir == NULL || snapshot == NULL)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }
    if (pre_event_pc_text != NULL) {
        uint32_t masks[VF2_HOST_INPUT_CYCLE_CAP] = {0u};
        size_t n = 0u;
        if (audio_pre_event_path == NULL ||
            !parse_input_cycle(pre_event_pc_text, masks, &n) ||
            n != 1u || masks[0] == 0u || (masks[0] & 3u) != 0u) {
            fprintf(stderr, "--audio-pre-event-pc requires a nonzero aligned "
                            "original i960 instruction IP and snapshot path\n");
            return EXIT_FAILURE;
        }
        writer.filter_by_pc = 1;
        writer.requested_writer_pc = masks[0];
    }
    if (pre_event_word_text != NULL) {
        uint32_t masks[VF2_HOST_INPUT_CYCLE_CAP] = {0u};
        size_t n = 0u;
        if (audio_pre_event_path == NULL ||
            !parse_input_cycle(pre_event_word_text, masks, &n) ||
            n != 1u) {
            fprintf(stderr, "--audio-pre-event-word requires a valid uint32 "
                            "word and snapshot path\n");
            return EXIT_FAILURE;
        }
        writer.filter_by_word = 1;
        writer.requested_writer_word = masks[0];
    }
    if (input_cycle_text != NULL &&
        !parse_input_cycle(input_cycle_text, input_cycle,
                           &input_cycle_count)) {
        fprintf(stderr, "invalid --input-cycle (expected comma-separated uint32 masks, max 64)\n");
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

    if (verified_audio_copy) {
        vf2_status audio_optin =
            vf2_session_set_verified_d92_copy(session, 1);
        if (audio_optin != VF2_OK) {
            fprintf(stderr,
                    "verified-audio-copy unavailable (%s); keeping "
                    "all unrecovered routes disabled\n",
                    vf2_status_string(audio_optin));
        } else {
            fprintf(stderr,
                    "verified-audio-copy enabled: guarded D92->ED6 "
                    "only; no synthetic hit packets\n");
        }
    }
    if (verified_audio_90) {
        vf2_status audio_optin =
            vf2_session_set_verified_90_allocator(session, 1);
        if (audio_optin != VF2_OK) {
            fprintf(stderr,
                    "verified-audio-90 unavailable (%s); "
                    "keeping unverified routes disabled\n",
                    vf2_status_string(audio_optin));
        } else {
            fprintf(stderr,
                    "verified-audio-90 enabled: original frame342 "
                    "9E0A7F FAC->CB2->D90 only; no injected event\n");
        }
    }
    if (audio_evidence_path != NULL) {
        audio_evidence = fopen(audio_evidence_path, "w");
        if (audio_evidence == NULL) {
            fprintf(stderr, "cannot open audio evidence file: %s\n",
                    audio_evidence_path);
            vf2_session_close(session);
            host_close(&h);
            return EXIT_FAILURE;
        }
    }
    if (audio_pre_event_path != NULL || audio_evidence != NULL) {
        vf2_status obs;
        writer.session = session;
        obs = vf2_session_set_memory_observer(
            session, sound_writer_observe, &writer);
        if (obs != VF2_OK) {
            fprintf(stderr,
                    "audio pre-event: original i960 bus observer unavailable\n");
            if (audio_evidence != NULL) (void)fclose(audio_evidence);
            vf2_session_close(session);
            host_close(&h);
            return EXIT_FAILURE;
        }
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
                vf2_session_audio_pump_report audio_pump;
                uint32_t input = input_cycle_count != 0u
                    ? input_cycle[frame % input_cycle_count]
                    : vf2_host_input_mask(&h.input);
                uint8_t queue_before = 0u, queue_after = 0u;
                vf2_status pump_status;
                vf2_status render_status;
                h.step_once = 0;
                writer.observed_writes = 0u;
                writer.first_address = 0u;
                writer.first_command = 0u;
                writer.first_writer_pc = 0u;
                writer.first_writer_pc_proven = 0;
                writer.matched_pre_event = 0;
                writer.matched_slot = 0u;
                writer.matched_word = 0u;
                writer.matched_pc = 0u;
                if (audio_pre_event_path != NULL && !pre_event_written &&
                    vf2_session_capture_oracle_snapshot(
                        session, &pre_event) != VF2_OK) {
                    fprintf(stderr,
                            "audio pre-event: oracle checkpoint capture failed\n");
                    rc = EXIT_FAILURE;
                    break;
                }
                if (audio_evidence != NULL &&
                    vf2_session_read_work_ram(session, UINT32_C(0x00504001),
                                             &queue_before, 1u) != VF2_OK) {
                    fprintf(stderr, "audio evidence: queue-before read failed\n");
                    rc = EXIT_FAILURE;
                    break;
                }
                writer.original_frame_active =
                    audio_pre_event_path != NULL || audio_evidence != NULL;
                st = oracle
                    ? vf2_session_step_oracle(session, input)
                    : vf2_session_step(session, input);
                writer.original_frame_active = 0;
                if (st != VF2_OK) {
                    fprintf(stderr, "frame %u failed: %s\n", (unsigned)frame,
                            vf2_status_string(st));
                    rc = EXIT_FAILURE;
                    break;
                }
                if (audio_pre_event_path != NULL && !pre_event_written &&
                    writer.matched_pre_event) {
                    vf2_status snapshot_status =
                        vf2_i960_snapshot_write_file(
                            &pre_event, audio_pre_event_path);
                    if (snapshot_status != VF2_OK) {
                        fprintf(stderr,
                                "audio pre-event: snapshot write failed (%s)\n",
                                vf2_status_string(snapshot_status));
                        rc = EXIT_FAILURE;
                        break;
                    }
                    pre_event_written = 1;
                    fprintf(stderr,
                            "audio pre-event: saved original pre-frame %u "
                            "at %s; original queue writes=%u first_slot=%08x "
                            "selected_word=%08x selected_writer_pc=%08x "
                            "(not a fighter-hit proof)\n",
                            (unsigned)frame, audio_pre_event_path,
                            (unsigned)writer.observed_writes,
                            (unsigned)writer.matched_slot,
                            (unsigned)writer.matched_word,
                            (unsigned)writer.matched_pc);
                }
                if (audio_evidence != NULL &&
                    vf2_session_read_work_ram(session, UINT32_C(0x00504001),
                                             &queue_after, 1u) != VF2_OK) {
                    fprintf(stderr, "audio evidence: queue-after read failed\n");
                    rc = EXIT_FAILURE;
                    break;
                }
                /* All UART evidence comes from the normal per-frame
                 * pump, never from injected events. */
                memset(&audio_pump, 0, sizeof(audio_pump));
                pump_status = vf2_session_pump_audio(session, &audio_pump);
                if (pump_status == VF2_OK &&
                    audio_pump.bytes_moved != 0u) {
                    fprintf(stderr,
                            "frame %u: audio pump bytes=%u filed=%u "
                            "dispatched=%u refused=%u d92=%u keyon90=%u\n",
                            (unsigned)frame,
                            (unsigned)audio_pump.bytes_moved,
                            (unsigned)audio_pump.entries_filed,
                            (unsigned)audio_pump.entries_dispatched,
                            (unsigned)audio_pump.entries_refused,
                            (unsigned)audio_pump.d92_keyons_retired,
                            (unsigned)audio_pump.keyon_90_retired);
                }
                render_status = vf2_session_render_audio(
                    session, left, right, AUDIO_FRAMES_PER_VIDEO_FRAME);
                if (render_status == VF2_OK) {
                    host_push_audio(&h, left, right,
                                    AUDIO_FRAMES_PER_VIDEO_FRAME);
                }
                if (audio_evidence != NULL &&
                    !write_audio_evidence_frame(
                        audio_evidence, frame, input, queue_before,
                        queue_after, &writer, &audio_pump, pump_status,
                        render_status, left, right,
                        AUDIO_FRAMES_PER_VIDEO_FRAME)) {
                    fprintf(stderr,
                            "audio evidence: failed writing frame %u\n",
                            (unsigned)frame);
                    rc = EXIT_FAILURE;
                    break;
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
    if (audio_pre_event_path != NULL || audio_evidence != NULL) {
        (void)vf2_session_set_memory_observer(session, NULL, NULL);
    }
    if (audio_pre_event_path != NULL) {
        if (!pre_event_written) {
            fprintf(stderr,
                    "audio pre-event: no original i960 aligned sound-ring "
                    "writes observed; no snapshot file created\n");
        }
    }
    vf2_i960_snapshot_destroy(&pre_event);
    if (audio_evidence != NULL && fclose(audio_evidence) != 0) {
        fprintf(stderr, "audio evidence: failed to close output\n");
        rc = EXIT_FAILURE;
    }
    vf2_session_close(session);
    host_close(&h);
    return rc;
}
