#include "vf2/host.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "vf2/platform.h"

typedef struct button_name {
    const char *name;
    uint32_t mask;
} button_name;

static const button_name k_buttons[] = {
    {"UP", VF2_PLATFORM_BUTTON_UP},
    {"DOWN", VF2_PLATFORM_BUTTON_DOWN},
    {"LEFT", VF2_PLATFORM_BUTTON_LEFT},
    {"RIGHT", VF2_PLATFORM_BUTTON_RIGHT},
    {"PUNCH", VF2_PLATFORM_BUTTON_PUNCH},
    {"KICK", VF2_PLATFORM_BUTTON_KICK},
    {"GUARD", VF2_PLATFORM_BUTTON_GUARD},
    {"START", VF2_PLATFORM_BUTTON_START},
    {"COIN", VF2_PLATFORM_BUTTON_COIN},
    {"SERVICE", VF2_PLATFORM_BUTTON_SERVICE},
    {"P2_UP", VF2_PLATFORM_BUTTON_P2_UP},
    {"P2_DOWN", VF2_PLATFORM_BUTTON_P2_DOWN},
    {"P2_LEFT", VF2_PLATFORM_BUTTON_P2_LEFT},
    {"P2_RIGHT", VF2_PLATFORM_BUTTON_P2_RIGHT},
    {"P2_PUNCH", VF2_PLATFORM_BUTTON_P2_PUNCH},
    {"P2_KICK", VF2_PLATFORM_BUTTON_P2_KICK},
    {"P2_GUARD", VF2_PLATFORM_BUTTON_P2_GUARD},
    {"TEST", VF2_PLATFORM_BUTTON_TEST}
};
#define BUTTON_COUNT (sizeof(k_buttons) / sizeof(k_buttons[0]))

static int ieq(const char *a, const char *b)
{
    while (*a != '\0' && *b != '\0') {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}

uint32_t vf2_host_button_from_name(const char *name)
{
    if (name == NULL) return 0u;
    for (size_t i = 0u; i < BUTTON_COUNT; ++i) {
        if (ieq(name, k_buttons[i].name)) return k_buttons[i].mask;
    }
    return 0u;
}

vf2_status vf2_host_bindings_set(
    vf2_host_bindings *b, const char *source, const char *button
)
{
    uint32_t mask = vf2_host_button_from_name(button);

    if (b == NULL || source == NULL || source[0] == '\0' || mask == 0u ||
        strlen(source) >= sizeof(b->entries[0].source)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    /* A source maps to one button; several sources may map to the same one. */
    for (size_t i = 0u; i < b->count; ++i) {
        if (ieq(b->entries[i].source, source)) {
            b->entries[i].mask = mask;
            return VF2_OK;
        }
    }
    if (b->count >= VF2_HOST_MAX_BINDINGS) return VF2_ERROR_OUT_OF_MEMORY;
    strcpy(b->entries[b->count].source, source);
    b->entries[b->count].mask = mask;
    ++b->count;
    return VF2_OK;
}

void vf2_host_bindings_defaults(vf2_host_bindings *b)
{
    static const char *const map[][2] = {
        {"Up", "UP"}, {"Down", "DOWN"}, {"Left", "LEFT"}, {"Right", "RIGHT"},
        {"Z", "PUNCH"}, {"X", "KICK"}, {"C", "GUARD"},
        {"Return", "START"}, {"5", "COIN"}, {"F2", "SERVICE"}, {"F1", "TEST"},
        {"I", "P2_UP"}, {"K", "P2_DOWN"}, {"J", "P2_LEFT"}, {"L", "P2_RIGHT"},
        {"A", "P2_PUNCH"}, {"S", "P2_KICK"}, {"D", "P2_GUARD"},
        {"pad:dpup", "UP"}, {"pad:dpdown", "DOWN"},
        {"pad:dpleft", "LEFT"}, {"pad:dpright", "RIGHT"},
        {"pad:a", "PUNCH"}, {"pad:b", "KICK"}, {"pad:x", "GUARD"},
        {"pad:start", "START"}, {"pad:back", "COIN"}
    };
    memset(b, 0, sizeof(*b));
    for (size_t i = 0u; i < sizeof(map) / sizeof(map[0]); ++i) {
        (void)vf2_host_bindings_set(b, map[i][0], map[i][1]);
    }
}

static char *trim(char *s)
{
    char *end;
    while (*s != '\0' && isspace((unsigned char)*s)) ++s;
    end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1])) *--end = '\0';
    return s;
}

vf2_status vf2_host_bindings_parse(
    vf2_host_bindings *bindings, const char *text, size_t *bad_line
)
{
    vf2_host_bindings work;
    size_t line_no = 0u;
    const char *p = text;

    if (bindings == NULL || text == NULL) return VF2_ERROR_INVALID_ARGUMENT;
    work = *bindings; /* all or nothing */
    while (*p != '\0') {
        char line[160];
        size_t n = 0u;
        char *eq, *src, *btn, *c;
        ++line_no;
        while (p[n] != '\0' && p[n] != '\n') ++n;
        if (n >= sizeof(line)) {
            if (bad_line != NULL) *bad_line = line_no;
            return VF2_ERROR_INVALID_ARGUMENT;
        }
        memcpy(line, p, n);
        line[n] = '\0';
        p += n;
        if (*p == '\n') ++p;
        for (c = line; *c != '\0'; ++c) {
            if (*c == '#' || *c == ';') { *c = '\0'; break; }
        }
        src = trim(line);
        if (*src == '\0') continue;
        eq = strchr(src, '=');
        if (eq == NULL) {
            if (bad_line != NULL) *bad_line = line_no;
            return VF2_ERROR_INVALID_ARGUMENT;
        }
        *eq = '\0';
        btn = trim(eq + 1);
        src = trim(src);
        if (vf2_host_bindings_set(&work, src, btn) != VF2_OK) {
            if (bad_line != NULL) *bad_line = line_no;
            return VF2_ERROR_INVALID_ARGUMENT;
        }
    }
    *bindings = work;
    return VF2_OK;
}

uint32_t vf2_host_bindings_lookup(
    const vf2_host_bindings *b, const char *source
)
{
    if (b == NULL || source == NULL) return 0u;
    for (size_t i = 0u; i < b->count; ++i) {
        if (ieq(b->entries[i].source, source)) return b->entries[i].mask;
    }
    return 0u;
}

void vf2_host_input_init(vf2_host_input *in, const vf2_host_bindings *b)
{
    memset(in, 0, sizeof(*in));
    in->bindings = b;
}

void vf2_host_input_event(vf2_host_input *in, const char *source, int down)
{
    uint32_t mask = vf2_host_bindings_lookup(in->bindings, source);
    for (size_t bit = 0u; bit < 18u; ++bit) {
        if ((mask & (UINT32_C(1) << bit)) == 0u) continue;
        if (down != 0) {
            ++in->held_count[bit];
        } else if (in->held_count[bit] != 0u) {
            --in->held_count[bit];
        }
    }
}

uint32_t vf2_host_input_mask(const vf2_host_input *in)
{
    uint32_t mask = 0u;
    for (size_t bit = 0u; bit < 18u; ++bit) {
        if (in->held_count[bit] != 0u) mask |= UINT32_C(1) << bit;
    }
    return mask;
}

vf2_status vf2_host_audio_ring_init(vf2_host_audio_ring *r, size_t frames)
{
    if (r == NULL || frames == 0u || frames > (SIZE_MAX / 4u)) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }
    memset(r, 0, sizeof(*r));
    r->data = (int16_t *)calloc(frames * 2u, sizeof(int16_t));
    if (r->data == NULL) return VF2_ERROR_OUT_OF_MEMORY;
    r->capacity = frames;
    return VF2_OK;
}

void vf2_host_audio_ring_destroy(vf2_host_audio_ring *r)
{
    if (r == NULL) return;
    free(r->data);
    memset(r, 0, sizeof(*r));
}

size_t vf2_host_audio_ring_write(
    vf2_host_audio_ring *r, const int16_t *l, const int16_t *rt, size_t frames
)
{
    size_t room, n;
    if (r == NULL || r->data == NULL || l == NULL || rt == NULL) return 0u;
    room = r->capacity - r->count;
    n = frames < room ? frames : room;
    r->dropped += frames - n;
    for (size_t i = 0u; i < n; ++i) {
        size_t pos = (r->read + r->count + i) % r->capacity;
        r->data[pos * 2u] = l[i];
        r->data[pos * 2u + 1u] = rt[i];
    }
    r->count += n;
    return n;
}

void vf2_host_audio_ring_read(
    vf2_host_audio_ring *r, int16_t *out, size_t frames
)
{
    size_t n;
    if (r == NULL || out == NULL) return;
    n = frames < r->count ? frames : r->count;
    for (size_t i = 0u; i < n; ++i) {
        size_t pos = (r->read + i) % r->capacity;
        out[i * 2u] = r->data[pos * 2u];
        out[i * 2u + 1u] = r->data[pos * 2u + 1u];
    }
    r->read = (r->read + n) % r->capacity;
    r->count -= n;
    if (n < frames) {
        ++r->underruns;
        memset(out + n * 2u, 0, (frames - n) * 2u * sizeof(int16_t));
    }
}

vf2_host_rect vf2_host_fit(int ww, int wh, int sw, int sh, int integer)
{
    vf2_host_rect r = {0, 0, 0, 0};
    long long w, h;

    if (ww <= 0 || wh <= 0 || sw <= 0 || sh <= 0) return r;
    if (integer != 0) {
        int k = ww / sw < wh / sh ? ww / sw : wh / sh;
        if (k < 1) k = 1;
        w = (long long)sw * k;
        h = (long long)sh * k;
    } else if ((long long)ww * sh <= (long long)wh * sw) {
        w = ww;
        h = ((long long)ww * sh) / sw;
    } else {
        h = wh;
        w = ((long long)wh * sw) / sh;
    }
    r.w = (int)w;
    r.h = (int)h;
    r.x = (ww - r.w) / 2;
    r.y = (wh - r.h) / 2;
    return r;
}
