#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <algorithm>





typedef struct Buffer {
    uint8_t* data; // pointer to buffer data
    int size;      // size of buffer data (bytes)
    int index;     // index of next byte to be read/written
} Buffer;

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
    int num_bits_read;
    int word_index;
    uint32_t* buffer;
} BitReader;


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

void WriteBits(BitWriter& bw, uint32_t value, int num_bits)
{
    bw.scratch |= ((uint64_t)value) << bw.scratch_bits;
    bw.scratch_bits += num_bits;

    while (bw.scratch_bits >= 32) {
        bw.buffer[bw.word_index++] = (uint32_t)(bw.scratch & 0xFFFFFFFF);
        bw.scratch >>= 32;
        bw.scratch_bits -= 32;
    }
}

uint32_t ReadBits(BitReader& br, int num_bits)
{
    if (br.scratch_bits < num_bits) {
        br.scratch |= ((uint64_t)br.buffer[br.word_index++]) << br.scratch_bits;
        br.scratch_bits += 32;
    }

    uint32_t value = (uint32_t)(br.scratch & ((1ULL << num_bits) - 1));
    br.scratch >>= num_bits;
    br.scratch_bits -= num_bits;

    return value;
}

class WriteStream {
public:
    enum { IsWriting = 1 };
    enum { IsReading = 0 };

    WriteStream(uint8_t* buffer, int bytes)
    {
        m_writer.buffer = buffer;
        m_writer.bytes = bytes;
    }

    bool SerializeInteger(int32_t value, int32_t min, int32_t max)
    {
        assert(min < max);
        assert(value >= min);
        assert(value <= max);

        const int bits = bits_required(min, max);
        uint32_t unsigned_value = value - min;

        WriteBits(m_writer, unsigned_value, bits);

        return true;
    }

    bool SerializeBits(int32_t value, int bits)
    {
        assert(bits > 0);
        assert(value >= 0);
        assert(value < (1 << bits));

        WriteBits(m_writer, value, bits);

        return true;
    }

private:
    BitWriter m_writer;
};

#define serialize_int(stream, value, min, max)                 \
    do {                                                       \
        assert(min < max);                                     \
        int32_t int32_value;                                   \
        if (Stream::IsWriting) {                               \
            assert(value >= min);                              \
            assert(value <= max);                              \
            int32_value = (int32_t)value;                      \
        }                                                      \
        if (!stream.SerializeInteger(int32_value, min, max)) { \
            return false;                                      \
        }                                                      \
        if (Stream::IsReading) {                               \
            value = int32_value;                               \
            if (value < min || value > max) {                  \
                return false;                                  \
            }                                                  \
        }                                                      \
    } while (0)

#define serialize_bits(stream, value, bits)             \
    do {                                                \
        assert(bits > 0);                               \
        int32_t int32_value;                            \
        if (Stream::IsWriting) {                        \
            assert(value >= 0);                         \
            assert(value < (1 << bits));                \
            int32_value = (int32_t)value;               \
        }                                               \
        if (!stream.SerializeBits(int32_value, bits)) { \
            return false;                               \
        }                                               \
        if (Stream::IsReading) {                        \
            value = int32_value;                        \
            if (value < 0 || value >= (1 << bits)) {    \
                return false;                           \
            }                                           \
        }                                               \
    } while (0)

struct PacketB {
    int numElements;
    int elements[MaxElements];

    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_int(stream, numElements, 0, MaxElements);
        for (int i = 0; i < numElements; ++i) {
            serialize_bits(buffer, elements[i], 32);
        }
        return true;
    }
};


void test2() {
    int buffer_size = 256;
    uint8_t buffer[256];

    WriteStream writestream(buffer, buffer_size);

    PacketB packet;
    packet.numElements = 3;
    packet.elements[0] = 1;
    packet.elements[1] = 2;
    packet.elements[2] = 3;

    packet.Serialize(writestream);

}