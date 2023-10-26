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
        x = ReadInteger(buffer);
        y = ReadInteger(buffer);
        z = ReadInteger(buffer);
    }
};

void test() {
    Buffer buffer;

    uint8_t data_buffer[256];
    buffer.data = data_buffer;

    buffer.size = 256;
    int index = 0;
    
    PacketA packet;

    packet.x = 14;
    packet.y = 27;
    packet.z = 75;

    packet.Write(buffer);

    PacketA packet2;
    
    packet2.Read(buffer);

}


#endif
