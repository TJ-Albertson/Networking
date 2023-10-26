#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <algorithm>

typedef struct Buffer {
    uint8_t* data; // pointer to buffer data
    int size;      // size of buffer data (bytes)
    int index;     // index of next byte to be read/written
} Buffer;



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

class BitWriter {
public:
    BitWriter(uint8_t* buffer, int bytes)
        : buffer(buffer)
        , m_bytes(bytes)
        , word_index(0)
        , scratch(0)
        , scratch_bits(0)
    {
    }
    

    void WriteBits(uint32_t value, int bits)
    {
        assert(bits <= 32);
        scratch |= uint64_t(value) << scratch_bits;
        scratch_bits += bits;
        while (scratch_bits >= 32) {
            assert(word_index < m_bytes / 4);
            buffer[word_index] = uint32_t(scratch & 0xFFFFFFFF);
            word_index++;
            scratch >>= 32;
            scratch_bits -= 32;
        }
    }

    void Flush()
    {
        if (scratch_bits != 0) {
            assert(word_index < m_bytes / 4);
            buffer[word_index] = uint32_t(scratch & 0xFFFFFFFF);
            word_index++;
        }
    }
    

private:
    uint8_t* buffer;
    int m_bytes;
    int word_index = 0;
    uint64_t scratch;
    int scratch_bits;
};

class BitReader {
public:
    BitReader(const uint8_t* buffer, int bytes)
        : m_buffer(buffer)
        , m_bytes(bytes)
        , total_bits(bytes * 8)
        , num_bits_read(0)
        , word_index(0)
        , scratch(0)
        , scratch_bits(0)
    {
    }

    uint32_t ReadBits(int bits)
    {
        assert(bits <= 32);
        if (scratch_bits < bits) {
            assert(word_index < m_bytes / 4);
            scratch |= uint64_t(m_buffer[word_index]) << scratch_bits;
            scratch_bits += 32;
            word_index++;
        }
        uint32_t value = uint32_t(scratch & ((uint64_t(1) << bits) - 1));
        scratch >>= bits;
        scratch_bits -= bits;
        num_bits_read += bits;
        return value;
    }

    bool WouldReadPastEnd(int bits) {
        if (num_bits_read + bits > total_bits) {
            return true;
        }
        return false;
    }

public:
    const uint8_t* m_buffer;
    int m_bytes;
    int total_bits;
    int num_bits_read;
    int word_index;
    uint64_t scratch;
    int scratch_bits;
};

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

        const int bits = BITS_REQUIRED(0, MaxElements);
        uint32_t unsigned_value = value - min;

        m_writer.WriteBits(unsigned_value, bits);

        return true;
    }

    bool SerializeBits(int32_t value, int bits)
    {
        assert(bits > 0);
        assert(value >= 0);
        //assert(value < INT32_MAX);

        m_writer.WriteBits(value, bits);
        return true;
    }

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

        const int bits = BITS_REQUIRED(0, MaxElements);


        if (m_reader.WouldReadPastEnd(bits)) {

            return false;
        }


        uint32_t unsigned_value = m_reader.ReadBits(bits);
        value = (int32_t)unsigned_value + min;
        return true;
    }

    bool SerializeBits(int32_t& value, int bits)
    {
        assert(bits > 0);

        if (m_reader.WouldReadPastEnd(bits))
            return false;

        value = m_reader.ReadBits(bits);

        return true;
    }

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

/*
#define serialize_bits(stream, value, bits)             \
    do {                                                \
        assert(bits > 0);                               \
        int32_t int32_value;                            \
        if (Stream::IsWriting) {                        \
            assert(value >= 0);                         \
            assert(value < UINT32_MAX);                 \
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
*/

/*
#define serialize_bits(stream, value, bits)         \
    do {                                            \
        if (Stream::IsWriting) {                    \
            WriteBits(stream.m_writer, value, bits); \
        } else if (Stream::IsReading) {             \
            value = ReadBits(stream.m_reader, bits); \
        }                                           \
    } while (0)

    */
    
/*
#define serialize_bits(stream, value, bits)                           \
    if (Stream::IsWriting) {                                           \
        stream.SerializeBits(value, bits); \
    } else {                                                          \
        stream.SerializeBits(value, bits);  \
    }
    */

#define serialize_bits(stream, value, bits)                 \
    do {                                                       \
        if (!stream.SerializeBits(value, bits)) {           \
            return false;                                      \
        }                                                      \
    } while (0)


struct PacketB {
    int numElements;
    int elements[MaxElements];

    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_int(stream, numElements, 0, MaxElements);
        for (int i = 0; i < numElements; ++i) {
            serialize_bits(stream, elements[i], 32);
        }
        return true;
    }
};

