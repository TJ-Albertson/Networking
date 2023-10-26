#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <algorithm>

typedef struct Buffer {
    uint8_t* data; // pointer to buffer data
    int size; // size of buffer data (bytes)
    int index; // index of next byte to be read/written
} Buffer;


typedef struct {
    uint64_t scratch;
    int scratch_bits;
    int word_index;
    uint32_t* buffer;
} BitPacker;

void WriteBits(BitPacker* bp, uint32_t value, int num_bits)
{
    bp->scratch |= ((uint64_t)value) << bp->scratch_bits;
    bp->scratch_bits += num_bits;

    while (bp->scratch_bits >= 32) {
        bp->buffer[bp->word_index++] = (uint32_t)(bp->scratch & 0xFFFFFFFF);
        bp->scratch >>= 32;
        bp->scratch_bits -= 32;
    }
}

void BitPacker_Flush(BitPacker* bp)
{
    if (bp->scratch_bits > 0) {
        bp->buffer[bp->word_index++] = (uint32_t)(bp->scratch & 0xFFFFFFFF);
        bp->scratch >>= 32;
        bp->scratch_bits = 0;
    }
}


void BitWriter() {
    BitPacker bitpacker;
    bitpacker.scratch = 0;
    bitpacker.scratch_bits = 0;
    bitpacker.word_index = 0;

    WriteBits(&bitpacker, 5, 3);
    WriteBits(&bitpacker, 25, 10);
    WriteBits(&bitpacker, 50, 24);

    BitPacker_Flush(&bitpacker);

}


bool Write_SerializeInteger(int32_t value, int32_t min, int32_t max)
{
    assert(min < max);
    assert(value >= min);
    assert(value <= max);
    const int bits = bits_required(min, max);
    uint32_t unsigned_value = value - min;

    m_writer.WriteBits(unsigned_value, bits);
    return true;
}


class WriteStream {
public:
    enum { IsWriting = 1 };
    enum { IsReading = 0 };

    WriteStream(uint8_t* buffer, int bytes)
        : m_writer(buffer, bytes)
    {
    }

    bool SerializeInteger(int32_t value, int32_t min, int32_t max)
    {
        assert(min < max);
        assert(value >= min);
        assert(value <= max);
        const int bits = bits_required(min, max);
        uint32_t unsigned_value = value - min;
        m_writer.WriteBits(unsigned_value, bits);
        return true;
    }

    // ...

private:
    BitWriter m_writer;
};

class ReadStream {
public:
    enum { IsWriting = 0 };
    enum { IsReading = 1 };

    ReadStream(const uint8_t* buffer, int bytes)
        : m_reader(buffer, bytes)
    {
    }

    bool SerializeInteger(int32_t& value, int32_t min, int32_t max)
    {
        assert(min < max);
        const int bits = bits_required(min, max);
        if (m_reader.WouldReadPastEnd(bits)) {
            return false;
        }
        uint32_t unsigned_value = m_reader.ReadBits(bits);
        value = (int32_t)unsigned_value + min;
        return true;
    }

    // ...

private:
    BitReader m_reader;
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


struct PacketA {
    int x, y, z;

    template <typename Stream> bool Serialize(Stream& stream)
    {
        serialize_bits(stream, x, 32);
        serialize_bits(stream, y, 32);
        serialize_bits(stream, z, 32);
        return true;
    }
};