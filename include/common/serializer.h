#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <assert.h>
#include <stdint.h>

template <uint32_t x>
struct PopCount {
    enum { a = x - ((x >> 1) & 0x55555555),
        b = (((a >> 2) & 0x33333333) + (a & 0x33333333)),
        c = (((b >> 4) + b) & 0x0f0f0f0f),
        d = c + (c >> 8),
        e = d + (d >> 16),
        result = e & 0x0000003f };
};

template <uint32_t x>
struct Log2 {
    enum { a = x | (x >> 1),
        b = a | (a >> 2),
        c = b | (b >> 4),
        d = c | (c >> 8),
        e = d | (d >> 16),
        f = e >> 1,
        result = PopCount<f>::result };
};

template <int64_t min, int64_t max>
struct BitsRequired {
    static const uint32_t result = (min == max) ? 0 : (Log2<uint32_t(max - min)>::result + 1);
};

#define BITS_REQUIRED(min, max) BitsRequired<min, max>::result

const int MaxElements = 32;
const int MaxElementBits = BITS_REQUIRED(0, MaxElements);

typedef struct BitWriter {
    uint64_t scratch;
    int scratch_bits;
    int word_index;
    uint32_t* buffer;
} BitWriter;

typedef struct BitReader {
    uint64_t scratch;
    int scratch_bits;
    int total_bits;
    int total_bits_read;
    int word_index;
    uint32_t* buffer;
} BitReader;

void WriteBits(BitWriter& writer, uint32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    writer.scratch |= (uint64_t)value << writer.scratch_bits;
    writer.scratch_bits += bits;

    while (writer.scratch_bits >= 32) {
        writer.buffer[writer.word_index++] = (uint32_t)writer.scratch;
        writer.scratch >>= 32;
        writer.scratch_bits -= 32;
    }
}

// Function to flush any remaining bits to memory
void FlushBitsToMemory(BitWriter& writer)
{
    if (writer.scratch_bits > 0) {
        // Flush lower 32 bits to memory
        writer.buffer[writer.word_index++] = (uint32_t)(writer.scratch & 0xFFFFFFFF);
    }
}

uint32_t ReadBits(BitReader& reader, int num_bits_to_read)
{
    uint32_t result = 0;

    while (num_bits_to_read > 0) {
        if (reader.scratch_bits == 0 || num_bits_to_read > reader.scratch_bits) {
            // Read the next word into scratch
            uint32_t word = reader.buffer[reader.word_index];
            reader.scratch |= (uint64_t)word << reader.scratch_bits;
            reader.scratch_bits += 32;
            reader.word_index++;
        }

        // Calculate the number of bits to read from scratch
        int bits_to_read = (num_bits_to_read < reader.scratch_bits) ? num_bits_to_read : reader.scratch_bits;

        // Extract the bits from scratch
        uint32_t extracted_bits = reader.scratch & ((1ULL << bits_to_read) - 1);

        // Update scratch and scratch_bits
        reader.scratch >>= bits_to_read;
        reader.scratch_bits -= bits_to_read;

        // Update the result with the extracted bits
        result |= (extracted_bits << (num_bits_to_read - bits_to_read));

        // Update the number of bits left to read
        num_bits_to_read -= bits_to_read;
    }

    reader.total_bits_read += num_bits_to_read;
    return result;
}

bool WouldOverflow(BitReader reader, int bits)
{
    return reader.total_bits_read + bits > reader.total_bits;
}

bool ReadSerializeBits(BitReader& reader, int32_t& value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    if (WouldOverflow(reader, bits))
        return false;

    value = ReadBits(reader, bits);

    return true;
}

bool WriteSerializeBits(BitWriter& writer, int32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    WriteBits(writer, value, bits);
    return true;
}

#define read_serialize_bits(reader, value, bits)   \
    if (!ReadSerializeBits(reader, value, bits)) { \
        return false;                              \
    }

#define write_serialize_bits(reader, value, bits)   \
    if (!WriteSerializeBits(reader, value, bits)) { \
        return false;                               \
    }

bool ReadSerializeInteger(BitReader& reader, int32_t& value, int32_t min, int32_t max)
{
    assert(min < max);

    const int bits = BITS_REQUIRED(0, MaxElements);

    if (WouldOverflow(reader, bits)) {
        return false;
    }

    uint32_t unsigned_value = ReadBits(reader, bits);
    value = (int32_t)unsigned_value + min;
    return true;
}

#define read_serialize_int(reader, value, min, max)                 \
    do {                                                            \
        assert(min < max);                                          \
        int32_t int32_value;                                        \
        if (!ReadSerializeInteger(reader, int32_value, min, max)) { \
            return false;                                           \
        }                                                           \
        value = int32_value;                                        \
        if (value < min || value > max) {                           \
            return false;                                           \
        }                                                           \
    } while (0)

bool WriteSerializeInteger(BitWriter& writer, int32_t value, int32_t min, int32_t max)
{
    assert(min < max);
    assert(value >= min);
    assert(value <= max);

    const int bits = BITS_REQUIRED(0, MaxElements);
    uint32_t unsigned_value = value - min;

    WriteBits(writer, unsigned_value, bits);

    return true;
}

#define write_serialize_int(writer, value, min, max)                 \
    do {                                                             \
        assert(min < max);                                           \
        int32_t int32_value;                                         \
                                                                     \
        assert(value >= min);                                        \
        assert(value <= max);                                        \
        int32_value = (int32_t)value;                                \
                                                                     \
        if (!WriteSerializeInteger(writer, int32_value, min, max)) { \
            return false;                                            \
        }                                                            \
    } while (0)

#endif