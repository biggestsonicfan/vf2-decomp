#include "vf2/hash.h"

#include <stdio.h>
#include <string.h>

typedef struct sha1_context {
    uint32_t state[5];
    uint64_t total_bytes;
    uint8_t buffer[64];
    size_t buffer_size;
} sha1_context;

static uint32_t rotate_left(uint32_t value, unsigned amount)
{
    return (value << amount) | (value >> (32u - amount));
}

static uint32_t read_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24u) |
           ((uint32_t)data[1] << 16u) |
           ((uint32_t)data[2] << 8u) |
           (uint32_t)data[3];
}

static void write_be32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)(value >> 24u);
    data[1] = (uint8_t)(value >> 16u);
    data[2] = (uint8_t)(value >> 8u);
    data[3] = (uint8_t)value;
}

static void sha1_transform(sha1_context *context, const uint8_t block[64])
{
    uint32_t words[80];
    uint32_t a = 0u;
    uint32_t b = 0u;
    uint32_t c = 0u;
    uint32_t d = 0u;
    uint32_t e = 0u;
    unsigned index = 0u;

    for (index = 0u; index < 16u; ++index) {
        words[index] = read_be32(block + (index * 4u));
    }

    for (index = 16u; index < 80u; ++index) {
        words[index] = rotate_left(
            words[index - 3u] ^
            words[index - 8u] ^
            words[index - 14u] ^
            words[index - 16u],
            1u
        );
    }

    a = context->state[0];
    b = context->state[1];
    c = context->state[2];
    d = context->state[3];
    e = context->state[4];

    for (index = 0u; index < 80u; ++index) {
        uint32_t function = 0u;
        uint32_t constant = 0u;
        uint32_t temporary = 0u;

        if (index < 20u) {
            function = (b & c) | ((~b) & d);
            constant = 0x5a827999u;
        } else if (index < 40u) {
            function = b ^ c ^ d;
            constant = 0x6ed9eba1u;
        } else if (index < 60u) {
            function = (b & c) | (b & d) | (c & d);
            constant = 0x8f1bbcdcu;
        } else {
            function = b ^ c ^ d;
            constant = 0xca62c1d6u;
        }

        temporary = rotate_left(a, 5u) +
                    function +
                    e +
                    constant +
                    words[index];

        e = d;
        d = c;
        c = rotate_left(b, 30u);
        b = a;
        a = temporary;
    }

    context->state[0] += a;
    context->state[1] += b;
    context->state[2] += c;
    context->state[3] += d;
    context->state[4] += e;
}

static void sha1_init(sha1_context *context)
{
    context->state[0] = 0x67452301u;
    context->state[1] = 0xefcdab89u;
    context->state[2] = 0x98badcfeu;
    context->state[3] = 0x10325476u;
    context->state[4] = 0xc3d2e1f0u;
    context->total_bytes = 0u;
    context->buffer_size = 0u;
}

static void sha1_update(
    sha1_context *context,
    const uint8_t *data,
    size_t size
)
{
    size_t consumed = 0u;

    context->total_bytes += (uint64_t)size;

    while (consumed < size) {
        size_t available = 64u - context->buffer_size;
        size_t remaining = size - consumed;
        size_t copy_size = available < remaining ? available : remaining;

        memcpy(
            context->buffer + context->buffer_size,
            data + consumed,
            copy_size
        );

        context->buffer_size += copy_size;
        consumed += copy_size;

        if (context->buffer_size == 64u) {
            sha1_transform(context, context->buffer);
            context->buffer_size = 0u;
        }
    }
}

static void sha1_final(
    sha1_context *context,
    uint8_t digest[VF2_SHA1_SIZE]
)
{
    uint64_t total_bits = context->total_bytes * 8u;
    unsigned index = 0u;

    context->buffer[context->buffer_size++] = 0x80u;

    if (context->buffer_size > 56u) {
        while (context->buffer_size < 64u) {
            context->buffer[context->buffer_size++] = 0u;
        }
        sha1_transform(context, context->buffer);
        context->buffer_size = 0u;
    }

    while (context->buffer_size < 56u) {
        context->buffer[context->buffer_size++] = 0u;
    }

    for (index = 0u; index < 8u; ++index) {
        context->buffer[63u - index] =
            (uint8_t)(total_bits >> (index * 8u));
    }

    sha1_transform(context, context->buffer);

    for (index = 0u; index < 5u; ++index) {
        write_be32(digest + (index * 4u), context->state[index]);
    }
}

uint32_t vf2_crc32(const void *data, size_t size)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint32_t crc = 0xffffffffu;
    size_t index = 0u;

    for (index = 0u; index < size; ++index) {
        unsigned bit = 0u;
        crc ^= bytes[index];

        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
            crc = (crc >> 1u) ^ (0xedb88320u & mask);
        }
    }

    return ~crc;
}

void vf2_sha1(
    const void *data,
    size_t size,
    uint8_t digest[VF2_SHA1_SIZE]
)
{
    sha1_context context;

    sha1_init(&context);
    sha1_update(&context, (const uint8_t *)data, size);
    sha1_final(&context, digest);
}

void vf2_sha1_to_hex(
    const uint8_t digest[VF2_SHA1_SIZE],
    char output[VF2_SHA1_HEX_SIZE]
)
{
    static const char digits[] = "0123456789abcdef";
    size_t index = 0u;

    for (index = 0u; index < VF2_SHA1_SIZE; ++index) {
        output[index * 2u] = digits[digest[index] >> 4u];
        output[index * 2u + 1u] = digits[digest[index] & 0x0fu];
    }

    output[VF2_SHA1_HEX_SIZE - 1u] = '\0';
}

vf2_status vf2_hash_file(
    const char *path,
    size_t *size_out,
    uint32_t *crc32_out,
    uint8_t sha1_out[VF2_SHA1_SIZE]
)
{
    FILE *file = NULL;
    uint8_t buffer[65536];
    size_t bytes_read = 0u;
    size_t total = 0u;
    uint32_t crc = 0xffffffffu;
    sha1_context sha1;

    if (path == NULL || size_out == NULL ||
        crc32_out == NULL || sha1_out == NULL) {
        return VF2_ERROR_INVALID_ARGUMENT;
    }

    file = fopen(path, "rb");
    if (file == NULL) {
        return VF2_ERROR_IO;
    }

    sha1_init(&sha1);

    while ((bytes_read = fread(buffer, 1u, sizeof(buffer), file)) > 0u) {
        size_t index = 0u;

        total += bytes_read;
        sha1_update(&sha1, buffer, bytes_read);

        for (index = 0u; index < bytes_read; ++index) {
            unsigned bit = 0u;
            crc ^= buffer[index];

            for (bit = 0u; bit < 8u; ++bit) {
                uint32_t mask = (uint32_t)-(int32_t)(crc & 1u);
                crc = (crc >> 1u) ^ (0xedb88320u & mask);
            }
        }
    }

    if (ferror(file) != 0) {
        fclose(file);
        return VF2_ERROR_IO;
    }

    if (fclose(file) != 0) {
        return VF2_ERROR_IO;
    }

    sha1_final(&sha1, sha1_out);

    *size_out = total;
    *crc32_out = ~crc;
    return VF2_OK;
}
