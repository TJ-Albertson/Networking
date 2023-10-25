#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define MaxElements 32

typedef struct Buffer {
    uint8_t* data;  // pointer to buffer data
    int size;       // size of buffer data (bytes)
    int index;      // index of next byte to be read/written
} Buffer;

typedef struct BitReader {
    Buffer buffer;
    int bytes;
} BitReader;

typedef struct BitWriter {
    Buffer buffer;
    int bytes;
} BitWriter;


void WriteBits(Buffer& buffer, uint32_t value)
{
    assert(buffer.index + 4 <= buffer.size);
#ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = bswap(value);
#else // #ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = value;
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
}

void WriteInteger(Buffer& buffer, uint32_t value)
{
    assert(buffer.index + 4 <= buffer.size);
#ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = bswap(value);
#else // #ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = value;
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
}

void WriteShort(Buffer& buffer, uint16_t value) {
    assert(buffer.index + 4 <= buffer.size);
#ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = bswap(value);
#else // #ifdef BIG_ENDIAN
    *((uint16_t*)(buffer.data + buffer.index)) = value;
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
}

void WriteChar(Buffer& buffer, uint8_t value) {
    assert(buffer.index + 4 <= buffer.size);
#ifdef BIG_ENDIAN
    *((uint32_t*)(buffer.data + buffer.index)) = bswap(value);
#else // #ifdef BIG_ENDIAN
    *((uint8_t*)(buffer.data + buffer.index)) = value;
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
}


uint32_t ReadInteger(Buffer& buffer)
{
    assert(buffer.index + 4 <= buffer.size);
    uint32_t value;
#ifdef BIG_ENDIAN
    value = bswap(*((uint32_t*)(buffer.data + buffer.index)));
#else // #ifdef BIG_ENDIAN
    value = *((uint32_t*)(buffer.data + buffer.index));
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
    return value;
}

uint16_t ReadShort(Buffer& buffer) {
    assert(buffer.index + 4 <= buffer.size);
    uint16_t value;
#ifdef BIG_ENDIAN
    value = bswap(*((uint16_t*)(buffer.data + buffer.index)));
#else // #ifdef BIG_ENDIAN
    value = *((uint16_t*)(buffer.data + buffer.index));
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
    return value;
}

uint8_t ReadByte(Buffer& buffer) {
    assert(buffer.index + 4 <= buffer.size);
    uint8_t value;
#ifdef BIG_ENDIAN
    value = bswap(*((uint8_t*)(buffer.data + buffer.index)));
#else // #ifdef BIG_ENDIAN
    value = *((uint8_t*)(buffer.data + buffer.index));
#endif // #ifdef BIG_ENDIAN
    buffer.index += 4;
    return value;
}

struct PacketA {
    int x, y, z;

    void Write(Buffer& buffer)
    {
        WriteInteger(buffer, x);
        WriteInteger(buffer, y);
        WriteInteger(buffer, z);
    }

    void Read(Buffer& buffer)
    {
        ReadInteger(buffer, x);
        ReadInteger(buffer, y);
        ReadInteger(buffer, z);
    }
};

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



struct PacketB {
    int numElements;
    int elements[MaxElements];

    void Write(Buffer& buffer)
    {
        WriteInteger(buffer, numElements);
        for (int i = 0; i < numElements; ++i)
            WriteInteger(buffer, elements[i]);
    }

    void Read(Buffer& buffer)
    {
        ReadInteger(buffer, numElements);
        for (int i = 0; i < numElements; ++i)
            ReadInteger(buffer, elements[i]);
    }
};

struct PacketC {
    bool x;
    short y;
    int z;

    void Write(Buffer& buffer)
    {
        WriteChar(buffer, x);
        WriteShort(buffer, y);
        WriteInteger(buffer, z);
    }

    void Read(Buffer& buffer)
    {
        ReadByte(buffer, x);
        ReadShort(buffer, y);
        ReadInteger(buffer, z);
    }



#endif
