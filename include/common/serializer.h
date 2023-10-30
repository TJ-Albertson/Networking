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

    stream.type == WRITE;
    stream.m_data == (const uint32_t*)data;
    stream.m_numBits == bytes * 8;

    stream.m_bitsProcessed = 0;
    stream.m_scratch = 0;
    stream.m_scratchBits = 0;
    stream.m_wordIndex = 0;

    return true;
}

bool InitReadStream(Stream& stream, const void* data, int bytes)
{
    assert(data);
    
    stream.type == READ;
    stream.m_data == (const uint32_t*)data;
    stream.m_numBits == bytes * 8;

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

void s_WriteBits(Stream& stream, uint32_t value, int bits)
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

uint32_t s_ReadBits(Stream& stream, int bits)
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

void s_FlushBits(Stream& stream)
{
    if (stream.m_scratchBits != 0) {
        stream.m_data[stream.m_wordIndex] = host_to_network(uint32_t(stream.m_scratch & 0xFFFFFFFF));
        stream.m_scratch >>= 32;
        stream.m_scratchBits -= 32;
        stream.m_wordIndex++;
    }
}

int GetBytesProcessed(Stream& stream)
{
    return (stream.m_bitsProcessed + 7) / 8;
}

int GetBitsRemaining(Stream& stream)
{
    return stream.m_numBits - stream.m_bitsProcessed;
}

bool s_WouldOverflow(Stream stream, int bits)
{
    assert(stream.type == READ);

    return stream.m_bitsProcessed + bits > stream.m_numBits;
}

bool SerializeBits(Stream& stream, uint32_t& value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    if (stream.type == WRITE) {

        s_WriteBits(stream, value, bits);
        return true;
    }

    if (stream.type == READ) {

        if (s_WouldOverflow(stream, bits)) {
            return false;
        }

        uint32_t read_value = s_ReadBits(stream, bits);
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
        if (SerializeBits(stream, uint32_value, bits)) \
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
        s_WriteBits(stream, unsigned_value, bits);
        return true;
    }

    if (stream.type == READ) {

        if (s_WouldOverflow(stream, bits)) {
            printf("Read Error: Would Overflow\n");
            return false;
        }

        uint32_t unsigned_value = s_ReadBits(stream, bits);
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
        s_WriteBits(stream, zero, 8 - remainderBits);
        assert((stream.m_bitsProcessed % 8) == 0);
    }
}

bool SerializeAlign(Stream& stream)
{
    if (stream.type == WRITE) {
        WriteAlign(stream);
        return true;
    }

    if (stream.type == READ) {

        const int alignBits = m_reader.GetAlignBits();

        if (s_WouldOverflow(stream, alignBits)) {
            return false;
        }

        if (!m_reader.ReadAlign())
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





void WriteBits(BitWriter& writer, uint32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);
    //assert(writer.m_bitsWritten + bits <= writer.m_numBits);

    writer.scratch |= (uint64_t)value << writer.scratch_bits;
    writer.scratch_bits += bits;

    while (writer.scratch_bits >= 32) {
        writer.buffer[writer.word_index++] = (uint32_t)writer.scratch;
        writer.scratch >>= 32;
        writer.scratch_bits -= 32;
    }

    writer.m_bitsWritten += bits;
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






bool ReadSerializeBits(BitReader& reader, uint32_t& value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    if (WouldOverflow(reader, bits))
        return false;

    value = ReadBits(reader, bits);

    return true;
}

bool WriteSerializeBits(BitWriter& writer, uint32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);

    WriteBits(writer, value, bits);
    return true;
}

#define read_serialize_bits(reader, value, bits)       \
    do {                                               \
        assert(bits > 0);                              \
        assert(bits <= 32);                            \
        uint32_t uint32_value;                         \
        if (!ReadSerializeBits(reader, uint32_value, bits)) \
            return false;                              \
        value = uint32_value;                          \
    } while (0)

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



/*

bool ReadAlign(BitReader& reader)
{
    const int remainderBits = reader.total_bits_read % 8;
    if (remainderBits != 0) {
        uint32_t value = ReadBits(reader, 8 - remainderBits);

        assert(reader.total_bits_read % 8 == 0);
        if (value != 0)
            return false;
    }
    return true;
}

int GetAlignBits(BitReader& reader)
{
    return (8 - reader.total_bits_read % 8) % 8;
}
//
bool ReadSerializeAlign()
{
    const int alignBits = m_reader.GetAlignBits();
    if (m_reader.WouldOverflow(alignBits)) {
        m_error = PROTOCOL2_ERROR_STREAM_OVERFLOW;
        return false;
    }
    if (!m_reader.ReadAlign())
        return false;
    m_bitsRead += alignBits;
    return true;
}


  #define read_serialize_align(write)  \
    do {                              \
        if (!stream.SerializeAlign()) \
            return false;             \
    } while (0)

void WriteAlign(BitWriter& writer)
{
    const int remainderBits = writer. % 8;
    if (remainderBits != 0) {
        uint32_t zero = 0;
        WriteBits(zero, 8 - remainderBits);
        assert((m_bitsWritten % 8) == 0);
    }
}

bool WriteSerializeAlign()
{
    m_writer.WriteAlign();
    return true;
}

*/
  #define write_serialize_align(write)       \
    do {                              \
        if (!stream.SerializeAlign()) \
            return false;             \
    } while (0)

#endif