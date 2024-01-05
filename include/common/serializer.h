#ifndef SERIALIZER_H
#define SERIALIZER_H

#include <assert.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>

#define MaxElements 32

typedef enum  {
    WRITE,
    READ
} StreamType;

typedef struct {
    StreamType type;

    uint32_t* data;
    uint64_t scratch;

    int num_bits;
    int bits_processed;

    int word_index;
    int scratch_bits;
} Stream;


bool r_stream_write_init(Stream* stream, void* data, int bytes)
{
    assert(data);
    assert((bytes % 4) == 0); // buffer size must be a multiple of four

    stream->type = WRITE;
    stream->data = (uint32_t*)data;
    stream->num_bits = bytes * 8;

    stream->bits_processed = 0;
    stream->scratch = 0;
    stream->scratch_bits = 0;
    stream->word_index = 0;

    return true;
}

bool r_stream_read_init(Stream* stream, const void* data, int bytes)
{
    assert(data);
    
    stream->type = READ;
    stream->data = (uint32_t*)data;
    stream->num_bits = bytes * 8;

    stream->bits_processed = 0;
    stream->scratch = 0;
    stream->scratch_bits = 0;
    stream->word_index = 0;

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

void r_stream_write_bits(Stream* stream, uint32_t value, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);
    assert(stream->bits_processed + bits <= stream->num_bits);

    value &= ((uint64_t)(1) << bits) - 1;

    stream->scratch |= (uint64_t)(value) << stream->scratch_bits;

    stream->scratch_bits += bits;

    if (stream->scratch_bits >= 32) 
    {
        stream->data[stream->word_index] = host_to_network((uint32_t)(stream->scratch & 0xFFFFFFFF));
        stream->scratch >>= 32;
        stream->scratch_bits -= 32;
        stream->word_index++;
    }

    stream->bits_processed += bits;
}

uint32_t r_stream_read_bits(Stream* stream, int bits)
{
    assert(bits > 0);
    assert(bits <= 32);
    assert(stream->bits_processed + bits <= stream->num_bits);

    stream->bits_processed += bits;

    assert(stream->scratch_bits >= 0 && stream->scratch_bits <= 64);

    if (stream->scratch_bits < bits) {
        stream->scratch |= (uint64_t)(network_to_host(stream->data[stream->word_index])) << stream->scratch_bits;
        stream->scratch_bits += 32;
        stream->word_index++;
    }

    assert(stream->scratch_bits >= bits);

    const uint32_t output = stream->scratch & (((uint64_t)(1) << bits) - 1);

    stream->scratch >>= bits;
    stream->scratch_bits -= bits;

    return output;
}

void FlushBits(Stream* stream)
{
    if (stream->scratch_bits != 0) {
        stream->data[stream->word_index] = host_to_network((uint32_t)(stream->scratch & 0xFFFFFFFF));
        stream->scratch >>= 32;
        stream->scratch_bits -= 32;
        stream->word_index++;
    }
}

int GetAlignBits(Stream* stream)
{
    return (8 - stream->bits_processed % 8) % 8;
}

int GetBytesProcessed(Stream* stream)
{
    return (stream->bits_processed + 7) / 8;
}

int GetBitsRemaining(Stream* stream)
{
    return stream->num_bits - stream->bits_processed;
}

bool WouldOverflow(Stream stream, int bits)
{
    assert(stream.type == READ);

    return stream.bits_processed + bits > stream.num_bits;
}




/*
*   Base serialize functions
*/
bool r_serialize_bits(Stream* stream, void* value, int bits)
{
    uint32_t* new_value = (uint32_t*)value;
    assert(bits > 0);
    assert(bits <= 32);

    if (stream->type == WRITE) {

        r_stream_write_bits(stream, *new_value, bits);
        return true;
    }

    if (stream->type == READ) {

        if (WouldOverflow(*stream, bits)) {
            return false;
        }

        uint32_t read_value = r_stream_read_bits(stream, bits);
        *(uint32_t*)value = read_value;

        return true;
    }
}

bool r_serialize_int(Stream* stream, int32_t* value, int32_t min, int32_t max)
{
    assert(min < max);
    const int bits = bits_required(min, max);

    if (stream->type == WRITE) {
        
        int i = 0;
        if (*value < min)
            i++;

        if (*value > max)
            i++;

        assert(*value >= min);
        assert(*value <= max);

        uint32_t unsigned_value = *value - min;
        r_stream_write_bits(stream, unsigned_value, bits);
        return true;
    }

    if (stream->type == READ) {

        if (WouldOverflow(*stream, bits)) {
            printf("Read Error: Would Overflow\n");
            return false;
        }

        uint32_t unsigned_value = r_stream_read_bits(stream, bits);

        *value = (int32_t)unsigned_value + min;

        if (*value < min || *value > max)
            return false;      
        //stream->bits_processed += bits;
        return true;
    }
}


// Intended for 
void WriteAlign(Stream* stream)
{
    assert(stream->type == WRITE);

    const int remainderBits = stream->bits_processed % 8;
    if (remainderBits != 0) {
        uint32_t zero = 0;
        r_stream_write_bits(stream, zero, 8 - remainderBits);
        assert((stream->bits_processed % 8) == 0);
    }
}

bool ReadAlign(Stream* stream)
{
    assert(stream->type == READ);

    const int remainderBits = stream->bits_processed % 8;
    if (remainderBits != 0) 
    {
        uint32_t value = r_stream_read_bits(stream, 8 - remainderBits);
        assert(stream->bits_processed % 8 == 0);
        if (value != 0)
            return false;
    }
    return true;
}

bool SerializeAlign(Stream* stream)
{
    if (stream->type == WRITE) {
        WriteAlign(stream);
        return true;
    }

    if (stream->type == READ) {

        const int alignBits = GetAlignBits(stream);

        

        if (WouldOverflow(*stream, alignBits)) {
            return false;
        }

        if (!ReadAlign(stream))
            return false;

        //stream->bits_processed += alignBits;
        return true;
    }
}

    #define serialize_align(stream)       \
    do {                              \
        if (!SerializeAlign(stream)) \
            return false;             \
    } while (0)




void ReadBytes(Stream* stream, uint8_t* data, int bytes)
{
    assert(GetAlignBits(stream) == 0);
    assert(stream->bits_processed + bytes * 8 <= stream->num_bits);
    assert((stream->bits_processed % 32) == 0 || (stream->bits_processed % 32) == 8 || (stream->bits_processed % 32) == 16 || (stream->bits_processed % 32) == 24);

    int headBytes = (4 - (stream->bits_processed % 32) / 8) % 4;
    if (headBytes > bytes)
        headBytes = bytes;
    for (int i = 0; i < headBytes; ++i)
        data[i] = (uint8_t)r_stream_read_bits(stream, 8);
    if (headBytes == bytes)
        return;

    assert(GetAlignBits(stream) == 0);

    int numWords = (bytes - headBytes) / 4;
    if (numWords > 0) {
        assert((stream->bits_processed % 32) == 0);
        memcpy(data + headBytes, &stream->data[stream->word_index], numWords * 4);
        stream->bits_processed += numWords * 32;
        stream->word_index += numWords;
        stream->scratch_bits = 0;
    }

    assert(GetAlignBits(stream) == 0);

    int tailStart = headBytes + numWords * 4;
    int tailBytes = bytes - tailStart;
    assert(tailBytes >= 0 && tailBytes < 4);
    for (int i = 0; i < tailBytes; ++i)
        data[tailStart + i] = (uint8_t)r_stream_read_bits(stream, 8);

    assert(GetAlignBits(stream) == 0);

    assert(headBytes + numWords * 4 + tailBytes == bytes);
}

void WriteBytes(Stream* stream, const uint8_t* data, int bytes)
{
    assert(GetAlignBits(stream) == 0);
    assert(stream->bits_processed + bytes * 8 <= stream->num_bits);
    assert((stream->bits_processed % 32) == 0 || (stream->bits_processed % 32) == 8 || (stream->bits_processed % 32) == 16 || (stream->bits_processed % 32) == 24);

    int headBytes = (4 - (stream->bits_processed % 32) / 8) % 4;
    if (headBytes > bytes)
        headBytes = bytes;
    for (int i = 0; i < headBytes; ++i)
        r_stream_write_bits(stream, data[i], 8);
    if (headBytes == bytes)
        return;

    FlushBits(stream);

    assert(GetAlignBits(stream) == 0);

    int numWords = (bytes - headBytes) / 4;
    if (numWords > 0) {
        assert((stream->bits_processed % 32) == 0);
        memcpy(&stream->data[stream->word_index], data + headBytes, numWords * 4);
        stream->bits_processed += numWords * 32;
        stream->word_index += numWords;
        stream->scratch = 0;
    }

    assert(GetAlignBits(stream) == 0);

    int tailStart = headBytes + numWords * 4;
    int tailBytes = bytes - tailStart;
    assert(tailBytes >= 0 && tailBytes < 4);
    for (int i = 0; i < tailBytes; ++i)
        r_stream_write_bits(stream, data[tailStart + i], 8);

    assert(GetAlignBits(stream) == 0);

    assert(headBytes + numWords * 4 + tailBytes == bytes);
}


bool SerializeBytes(Stream* stream, uint8_t* data, int bytes)
{
    if (stream->type == WRITE) {
        assert(data);
        assert(bytes >= 0);
        if (!SerializeAlign(stream))
            return false;
        WriteBytes(stream, data, bytes);
        return true;
    }

    if (stream->type == READ) {
        if (!SerializeAlign(stream))
            return false;
        if (WouldOverflow(*stream, bytes * 8)) {
            return false;
        }
        ReadBytes(stream, data, bytes);
        stream->bits_processed += bytes * 8;
        return true;
    }
}


bool serialize_bytes_internal(Stream* stream, uint8_t* data, int bytes)
{
    return SerializeBytes(stream, data, bytes);
}

#define serialize_bytes(stream, data, bytes)                \
    do {                                                    \
        if (!serialize_bytes_internal(stream, data, bytes)) \
            return false;                                   \
    } while (0)


bool serialize_uint64_internal(Stream* stream, uint64_t* value)
{
    uint32_t hi, lo;
    if (stream->type == WRITE) {
        lo = *value & 0xFFFFFFFF;
        hi = *value >> 32;
    }

    r_serialize_bits(stream, &lo, 32);
    r_serialize_bits(stream, &hi, 32);

    if (stream->type == READ)
        *value = ((uint64_t)(hi) << 32) | lo;

    return true;
}

bool serialize_uint64(Stream* stream, uint64_t* value)
{
    do {
        if (!serialize_uint64_internal(stream, value))
            return false;
    } while (0);
}
    


#define serialize_enum(stream, value, enum_type, num_entries)           \
    do {                                                                \
        int32_t int_value;                                              \
        if (stream->type == WRITE)                                       \
            int_value = value;                                          \
        r_serialize_int(stream, &int_value, 0, num_entries - 1);        \
        if (stream->type == READ)                                        \
            value = (enum_type)value;                                   \
    } while (0) 



#endif