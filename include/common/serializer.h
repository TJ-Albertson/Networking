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
    uint32_t* buffer;
    uint64_t scratch;

    int m_numBits;
    int m_bitsWritten;
    int scratch_bits;
    int word_index;
} BitWriter;

typedef struct BitReader {
    uint32_t* buffer;
    uint64_t scratch;

    int scratch_bits;
    int total_bits;
    int total_bits_read;
    int word_index;
} BitReader;

enum StreamType {
    WRITE,
    READ
};

typedef struct reader {
    const uint32_t* m_data;
    uint64_t m_scratch;

    int m_numBits;

    int m_bitsRead;

    int m_wordIndex;
    int m_scratchBits;
};

typedef struct writer {
    uint32_t* m_data;
    uint64_t m_scratch;

    int m_numBits;

    int m_bitsWritten;

    int m_wordIndex;
    int m_scratchBits;
};

typedef struct Stream {
    StreamType type;

    uint32_t* m_data;
    uint64_t m_scratch;

    int m_numBits;
    int m_bitsProcessed;

    int m_wordIndex;
    int m_scratchBits;
} Stream;


bool InitWriteStream(Stream& stream, void* data, int bytes)
{
    assert(data);
    assert((bytes % 4) == 0); // buffer size must be a multiple of four

    stream.type = WRITE;
    stream.m_data = (uint32_t*)data;
    stream.m_numBits = bytes * 8;

    stream.m_bitsProcessed = 0;
    stream.m_scratch = 0;
    stream.m_scratchBits = 0;
    stream.m_wordIndex = 0;

    return true;
}

bool InitReadStream(Stream& stream, const void* data, int bytes)
{
    assert(data);
    
    stream.type = READ;
    stream.m_data = (uint32_t*)data;
    stream.m_numBits = bytes * 8;

    stream.m_bitsProcessed = 0;
    stream.m_scratch = 0;
    stream.m_scratchBits = 0;
    stream.m_wordIndex = 0;

    return true;
}


inline uint32_t host_to_network(uint32_t value)
{
#if PROTOCOL2_BIG_ENDIAN
    return bswap(value);
#else // #if PROTOCOL2_BIG_ENDIAN
    return value;
#endif // #if PROTOCOL2_BIG_ENDIAN
}

inline uint32_t network_to_host(uint32_t value)
{
#if PROTOCOL2_BIG_ENDIAN
    return bswap(value);
#else // #if PROTOCOL2_BIG_ENDIAN
    return value;
#endif // #if PROTOCOL2_BIG_ENDIAN
}

inline int bits_required(uint32_t min, uint32_t max)
{
    return (min == max) ? 0 : log2(max - min) + 1;
}

void WriteBits(Stream& stream, uint32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);
    assert(stream.m_bitsProcessed + bits <= stream.m_numBits);

    value &= (uint64_t(1) << bits) - 1;

    stream.m_scratch |= uint64_t(value) << stream.m_scratchBits;

    stream.m_scratchBits += bits;

    if (stream.m_scratchBits >= 32) {
        stream.m_data[stream.m_wordIndex] = host_to_network(uint32_t(stream.m_scratch & 0xFFFFFFFF));
        stream.m_scratch >>= 32;
        stream.m_scratchBits -= 32;
        stream.m_wordIndex++;
    }

    stream.m_bitsProcessed += bits;
}

uint32_t ReadBits(Stream& stream, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);
    assert(stream.m_bitsProcessed + bits <= stream.m_numBits);

    stream.m_bitsProcessed += bits;

    assert(stream.m_scratchBits >= 0 && stream.m_scratchBits <= 64);

    if (stream.m_scratchBits < bits) {
        stream.m_scratch |= uint64_t(network_to_host(stream.m_data[stream.m_wordIndex])) << stream.m_scratchBits;
        stream.m_scratchBits += 32;
        stream.m_wordIndex++;
    }

    assert(stream.m_scratchBits >= bits);

    const uint32_t output = stream.m_scratch & ((uint64_t(1) << bits) - 1);

    stream.m_scratch >>= bits;
    stream.m_scratchBits -= bits;

    return output;
}

void FlushBits(Stream& stream)
{
    if (stream.m_scratchBits != 0) {
        stream.m_data[stream.m_wordIndex] = host_to_network(uint32_t(stream.m_scratch & 0xFFFFFFFF));
        stream.m_scratch >>= 32;
        stream.m_scratchBits -= 32;
        stream.m_wordIndex++;
    }
}

int GetAlignBits(Stream& stream)
{
    assert(stream.type == READ);
    return (8 - stream.m_bitsProcessed % 8) % 8;
}

int GetBytesProcessed(Stream& stream)
{
    return (stream.m_bitsProcessed + 7) / 8;
}

int GetBitsRemaining(Stream& stream)
{
    return stream.m_numBits - stream.m_bitsProcessed;
}

bool WouldOverflow(Stream stream, int bits)
{
    assert(stream.type == READ);

    return stream.m_bitsProcessed + bits > stream.m_numBits;
}

bool SerializeBits(Stream& stream, uint32_t& value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    if (stream.type == WRITE) {

        WriteBits(stream, value, bits);
        return true;
    }

    if (stream.type == READ) {

        if (WouldOverflow(stream, bits)) {
            return false;
        }

        uint32_t read_value = ReadBits(stream, bits);
        value = read_value;
        stream.m_bitsProcessed += bits;
        return true;
    }
}

#define serialize_bits(stream, value, bits)            \
    do {                                               \
        assert(bits > 0);                              \
        assert(bits <= 32);                            \
        uint32_t uint32_value;                         \
        if (stream.type == WRITE)                      \
            uint32_value = (uint32_t)value;            \
        if (!SerializeBits(stream, uint32_value, bits)) \
            return false;                              \
        if (stream.type == READ)                       \
            value = uint32_value;                      \
    } while (0)


bool SerializeInteger(Stream& stream, int32_t& value, int32_t min, int32_t max)
{
    assert(min < max);
    const int bits = bits_required(min, max);

    if (stream.type == WRITE) {
        
        assert(value >= min);
        assert(value <= max);

        uint32_t unsigned_value = value - min;
        WriteBits(stream, unsigned_value, bits);
        return true;
    }

    if (stream.type == READ) {

        if (WouldOverflow(stream, bits)) {
            printf("Read Error: Would Overflow\n");
            return false;
        }

        uint32_t unsigned_value = ReadBits(stream, bits);
        value = (int32_t)unsigned_value + min;
        stream.m_bitsProcessed += bits;
        return true;
    }
}

#define serialize_int(stream, value, min, max)                   \
    do {                                                        \
        assert(min < max);                                      \
        int32_t int32_value;                                    \
        if (stream.type == WRITE) {                             \
            assert(int64_t(value) >= int64_t(min));             \
            assert(int64_t(value) <= int64_t(max));             \
            int32_value = (int32_t)value;                       \
        }                                                       \
        if (!SerializeInteger(stream, int32_value, min, max))   \
            return false;                                       \
        if (stream.type == READ) {                              \
            value = int32_value;                                \
            if (value < min || value > max)                     \
                return false;                                   \
        }                                                       \
    } while (0)

// Intended for 
void WriteAlign(Stream& stream)
{
    assert(stream.type == WRITE);

    const int remainderBits = stream.m_bitsProcessed % 8;
    if (remainderBits != 0) {
        uint32_t zero = 0;
        WriteBits(stream, zero, 8 - remainderBits);
        assert((stream.m_bitsProcessed % 8) == 0);
    }
}

bool ReadAlign(Stream& stream)
{
    assert(stream.type == READ);

    const int remainderBits = stream.m_bitsProcessed % 8;
    if (remainderBits != 0) {
        uint32_t value = ReadBits(stream, 8 - remainderBits);
        assert(stream.m_bitsProcessed % 8 == 0);
        if (value != 0)
            return false;
    }
    return true;
}

bool SerializeAlign(Stream& stream)
{
    if (stream.type == WRITE) {
        WriteAlign(stream);
        return true;
    }

    if (stream.type == READ) {

        const int alignBits = GetAlignBits(stream);

        if (WouldOverflow(stream, alignBits)) {
            return false;
        }

        if (!ReadAlign(stream))
            return false;

        stream.m_bitsProcessed += alignBits;
        return true;
    }
}

    #define serialize_align(stream)       \
    do {                              \
        if (!SerializeAlign(stream)) \
            return false;             \
    } while (0)




void ReadBytes(Stream& stream, uint8_t* data, int bytes)
{
    assert(GetAlignBits(stream) == 0);
    assert(stream.m_bitsProcessed + bytes * 8 <= stream.m_numBits);
    assert((stream.m_bitsProcessed % 32) == 0 || (stream.m_bitsProcessed % 32) == 8 || (stream.m_bitsProcessed % 32) == 16 || (stream.m_bitsProcessed % 32) == 24);

    int headBytes = (4 - (stream.m_bitsProcessed % 32) / 8) % 4;
    if (headBytes > bytes)
        headBytes = bytes;
    for (int i = 0; i < headBytes; ++i)
        data[i] = (uint8_t)ReadBits(stream, 8);
    if (headBytes == bytes)
        return;

    assert(GetAlignBits(stream) == 0);

    int numWords = (bytes - headBytes) / 4;
    if (numWords > 0) {
        assert((stream.m_bitsProcessed % 32) == 0);
        memcpy(data + headBytes, &stream.m_data[stream.m_wordIndex], numWords * 4);
        stream.m_bitsProcessed += numWords * 32;
        stream.m_wordIndex += numWords;
        stream.m_scratchBits = 0;
    }

    assert(GetAlignBits(stream) == 0);

    int tailStart = headBytes + numWords * 4;
    int tailBytes = bytes - tailStart;
    assert(tailBytes >= 0 && tailBytes < 4);
    for (int i = 0; i < tailBytes; ++i)
        data[tailStart + i] = (uint8_t)ReadBits(stream, 8);

    assert(GetAlignBits(stream) == 0);

    assert(headBytes + numWords * 4 + tailBytes == bytes);
}

void WriteBytes(Stream& stream, const uint8_t* data, int bytes)
{
    assert(GetAlignBits(stream) == 0);
    assert(stream.m_bitsProcessed + bytes * 8 <= stream.m_numBits);
    assert((stream.m_bitsProcessed % 32) == 0 || (stream.m_bitsProcessed % 32) == 8 || (stream.m_bitsProcessed % 32) == 16 || (stream.m_bitsProcessed % 32) == 24);

    int headBytes = (4 - (stream.m_bitsProcessed % 32) / 8) % 4;
    if (headBytes > bytes)
        headBytes = bytes;
    for (int i = 0; i < headBytes; ++i)
        WriteBits(stream, data[i], 8);
    if (headBytes == bytes)
        return;

    FlushBits(stream);

    assert(GetAlignBits(stream) == 0);

    int numWords = (bytes - headBytes) / 4;
    if (numWords > 0) {
        assert((stream.m_bitsProcessed % 32) == 0);
        memcpy(&stream.m_data[stream.m_wordIndex], data + headBytes, numWords * 4);
        stream.m_bitsProcessed += numWords * 32;
        stream.m_wordIndex += numWords;
        stream.m_scratch = 0;
    }

    assert(GetAlignBits(stream) == 0);

    int tailStart = headBytes + numWords * 4;
    int tailBytes = bytes - tailStart;
    assert(tailBytes >= 0 && tailBytes < 4);
    for (int i = 0; i < tailBytes; ++i)
        WriteBits(stream, data[tailStart + i], 8);

    assert(GetAlignBits(stream) == 0);

    assert(headBytes + numWords * 4 + tailBytes == bytes);
}


bool SerializeBytes(Stream& stream, uint8_t* data, int bytes)
{
    if (stream.type == WRITE) {
        assert(data);
        assert(bytes >= 0);
        if (!SerializeAlign(stream))
            return false;
        WriteBytes(stream, data, bytes);
        return true;
    }

    if (stream.type == READ) {
        if (!SerializeAlign(stream))
            return false;
        if (WouldOverflow(stream, bytes * 8)) {
            return false;
        }
        ReadBytes(stream, data, bytes);
        stream.m_bitsProcessed += bytes * 8;
        return true;
    }
}


bool serialize_bytes_internal(Stream& stream, uint8_t* data, int bytes)
{
    return SerializeBytes(stream, data, bytes);
}

    #define serialize_bytes(stream, data, bytes)                           \
    do {                                                               \
        if (!serialize_bytes_internal(stream, data, bytes)) \
            return false;                                              \
    } while (0)


#endif