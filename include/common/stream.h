#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <algorithm>


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

typedef struct Buffer {
    uint8_t* data; // pointer to buffer data
    int size; // size of buffer data (bytes)
    int index; // index of next byte to be read/written
} Buffer;

typedef struct BitWriter {
    Buffer buffer;
    int bytes;
} BitWriter;

struct PacketA {
    int x, y, z;

    template <typename Stream>
    bool Serialize(Stream& stream)
    {
        serialize_bits(stream, x, 32);
        serialize_bits(stream, y, 32);
        serialize_bits(stream, z, 32);
        return true;
    }
};




class BitWriter {
public:
    BitWriter(uint8_t* buffer, int bytes)
        : buffer_(buffer)
        , bytes_(bytes)
        , byteIndex_(0)
        , bitIndex_(0)
    {
    }

    void WriteBits(uint32_t value, int numBits)
    {
        if (byteIndex_ >= bytes_ || numBits <= 0) {
            // Handle buffer overflow or invalid input
            return;
        }

        while (numBits > 0) {
            // Calculate the number of bits that can be written in the current byte
            int bitsToWrite = std::min(8 - bitIndex_, numBits);

            // Mask the bits to be written and shift them to the correct position
            uint8_t bits = (value & ((1 << bitsToWrite) - 1)) << bitIndex_;

            // Write the bits to the current byte
            buffer_[byteIndex_] |= bits;

            // Update indices and remaining bits to write
            bitIndex_ += bitsToWrite;
            if (bitIndex_ >= 8) {
                bitIndex_ = 0;
                byteIndex_++;
            }
            value >>= bitsToWrite;
            numBits -= bitsToWrite;
        }
    }

private:
    uint8_t* buffer_;
    int bytes_;
    int byteIndex_;
    int bitIndex_;
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
        const int bits = bits_required(min, max);
        uint32_t unsigned_value = value - min;
        m_writer.WriteBits(unsigned_value, bits);
        return true;
    }

    // ...

private:
    BitWriter m_writer;
};



template <typename Stream> bool serialize_float_internal(Stream& stream, float& value)
{
    union FloatInt {
        float float_value;
        uint32_t int_value;
    };
    FloatInt tmp;
    if (Stream::IsWriting) {
        tmp.float_value = value;
    }
    bool result = stream.SerializeBits(tmp.int_value, 32);
    if (Stream::IsReading) {
        value = tmp.float_value;
    }
    return result;
}

#define serialize_float(stream, value)                  \
    do {                                                \
        if (!serialize_float_internal(stream, value)) { \
            return false;                               \
        }                                               \
}                                                       \
while (0)



class MyObject {
public:

private:
    int a;  
    bool b;
    float c;
};

template <typename Stream>
bool serialize(Stream& stream, MyObject& object)
{
    serialize_int(stream, object.a, 0, 100);
    serialize_bool(stream, object.b);
    serialize_float(stream, object.c);
    return true;
}

void write(MyObject& object)
{
    WriteStream stream;
    serialize(stream, object);
    send_to_network(stream);
}

void read(MyObject& object)
{
    ReadStream stream(receive_from_network());
    serialize(stream, object);
}