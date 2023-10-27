#include <stdint.h>
#include <iostream>
#include <assert.h>

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

void WriteBits(BitWriter& bitwriter, uint32_t value, int bits)
{
    bitwriter.scratch |= (uint64_t)value << bitwriter.scratch_bits;
    bitwriter.scratch_bits += bits;

    while (bitwriter.scratch_bits >= 32) {
        bitwriter.buffer[bitwriter.word_index++] = (uint32_t)bitwriter.scratch;
        bitwriter.scratch >>= 32;
        bitwriter.scratch_bits -= 32;
    }
}

// Function to flush any remaining bits to memory
void FlushBitsToMemory(BitWriter& bitwriter)
{
    if (bitwriter.scratch_bits > 0) {
        // Flush lower 32 bits to memory
        bitwriter.buffer[bitwriter.word_index++] = (uint32_t)(bitwriter.scratch & 0xFFFFFFFF);
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

bool ReadOverflow(BitReader bitreader, int bits) {
    return (bitreader.total_bits_read + bits) > bitreader.total_bits;
}


bool ReadSerializeBits(BitReader& reader, int32_t& value, int bits)
{
    assert(bits > 0);

    if (ReadOverflow(reader, bits))
        return false;

    value = ReadBits(reader, bits);

    return true;
}

bool WriteSerializeBits(BitWriter& writer, int32_t value, int bits)
{
    assert(bits > 0);
    assert(value >= 0);

    WriteBits(writer, value, bits);
    return true;
}

#define read_serialize_bits(reader, value, bits)        \
       if (!ReadSerializeBits(reader, value, bits)) {   \
            return false;                               \
       }                                                \

#define write_serialize_bits(reader, value, bits)       \
       if (!WriteSerializeBits(reader, value, bits)) {  \
            return false;                               \
       }                                                \


struct PacketA {
    int x, y, z;

    bool ReadSerialize(BitReader& reader)
    {
        read_serialize_bits(reader, x, 32);
        read_serialize_bits(reader, y, 32);
        read_serialize_bits(reader, z, 32);
        return true;
    }

    bool WriteSerialize(BitWriter& writer)
    {
        write_serialize_bits(writer, x, 32);
        write_serialize_bits(writer, y, 32);
        write_serialize_bits(writer, z, 32);
        return true;
    }
};

struct PacketB {
    int numElements;
    int elements[MaxElements];

    void Write(BitWriter& writer)
    {
        WriteBits(writer, numElements, MaxElementBits);
        for (int i = 0; i < numElements; ++i) {
            WriteBits(writer, elements[i], 32);
        }
    }

    void Read(BitReader& reader)
    {
        numElements = ReadBits(reader, MaxElementBits);

        if (numElements > numElements) {
            printf("numElements > numElements\n");
            return;
        }

        for (int i = 0; i < numElements; ++i) {
            if (ReadOverflow(reader, 32))
                break;

            elements[i] = ReadBits(reader, 32);
        }
    }
};

int main() {

    PacketB packet_1;
    packet_1.numElements = 5;
    
    packet_1.elements[0] = 3;
    packet_1.elements[1] = 8;
    packet_1.elements[2] = 7;
    packet_1.elements[3] = 6;
    packet_1.elements[4] = 1;

    uint32_t p_buffer[256];

    BitWriter writer;
    writer.buffer = p_buffer;
    writer.scratch = 0;
    writer.scratch_bits = 0;
    writer.word_index = 0;

    packet_1.Write(writer);

    FlushBitsToMemory(writer);

    uint8_t first_6_bits = p_buffer[0] & 0x3F;
    // printf("first_6_bits: %u\n", first_6_bits);

    for (int i = 0; i < 5; i++) {
       // printf("Int Value %d: %u\n", i + 1, p_buffer[i] >> 6);
    }
    
    BitReader reader;
    reader.scratch = 0;
    reader.scratch_bits = 0;
    reader.total_bits = 8 * sizeof(p_buffer);
    reader.total_bits_read = 0;
    reader.word_index = 0;
    reader.buffer = p_buffer;

    PacketB packet_2;

    packet_2.Read(reader);

    printf("packet_2\n");
    printf("numElements: %d\n", packet_2.numElements);
    for (int i = 0; i < packet_2.numElements; ++i) {
        printf("elements[%d]: %d\n", i, packet_2.elements[i]);
    }



    

    PacketA packet_a1;
    packet_a1.x = 15;
    packet_a1.y = 28;
    packet_a1.z = 32;


    uint32_t pa_buffer[256];

    BitWriter writer2;
    writer2.buffer = pa_buffer;
    writer2.scratch = 0;
    writer2.scratch_bits = 0;
    writer2.word_index = 0;

    packet_a1.WriteSerialize(writer2);


    BitReader reader2;
    reader2.scratch = 0;
    reader2.scratch_bits = 0;
    reader2.total_bits = 8 * sizeof(pa_buffer);
    reader2.total_bits_read = 0;
    reader2.word_index = 0;
    reader2.buffer = pa_buffer;

    PacketA packet_a2;

    packet_a2.ReadSerialize(reader2);

    printf("\n");
    printf("packet_a2\n");
    printf("packet_a2.x: %d\n", packet_a2.x);
    printf("packet_a2.y: %d\n", packet_a2.y);
    printf("packet_a2.z: %d\n", packet_a2.z);
}