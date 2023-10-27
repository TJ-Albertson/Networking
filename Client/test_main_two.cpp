#include <stdint.h>
#include <iostream>

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
    int num_bits_read;
    int word_index;
    uint32_t* buffer;
} BitReader;


void WriteBits(BitWriter& bitwriter, uint32_t value, int num_bits)
{
    // Make sure we have enough room in the scratch buffer
    if (bitwriter.scratch_bits + num_bits > 64) {
        // Flush lower 32 bits to memory
        bitwriter.buffer[bitwriter.word_index++] = (uint32_t)(bitwriter.scratch & 0xFFFFFFFF);
        // Shift right by 32 and subtract 32 from scratch_bits
        bitwriter.scratch >>= 32;
        bitwriter.scratch_bits -= 32;
    }

    // Pack the bits into the scratch buffer
    bitwriter.scratch |= (uint64_t)value << bitwriter.scratch_bits;
    bitwriter.scratch_bits += num_bits;
}

// Function to flush any remaining bits to memory
void FlushBitsToMemory(BitWriter& bitwriter)
{
    if (bitwriter.scratch_bits > 0) {
        // Flush lower 32 bits to memory
        bitwriter.buffer[bitwriter.word_index++] = (uint32_t)(bitwriter.scratch & 0xFFFFFFFF);
    }
}

uint32_t ReadBits(BitReader& reader, int num_bits)
{
    uint32_t result = 0;

    while (num_bits > 0) {
        if (reader.scratch_bits == 0) {
            // Read the next word into scratch
            uint32_t word = reader.buffer[reader.word_index];
            reader.scratch |= (uint64_t)word << reader.scratch_bits;
            reader.scratch_bits += 32;
            reader.word_index++;
        }

        // Calculate the number of bits to read from scratch
        int bits_to_read = (num_bits < reader.scratch_bits) ? num_bits : reader.scratch_bits;

        // Extract the bits from scratch
        uint32_t extracted_bits = reader.scratch & ((1ULL << bits_to_read) - 1);

        // Update scratch and scratch_bits
        reader.scratch >>= bits_to_read;
        reader.scratch_bits -= bits_to_read;

        // Update the result with the extracted bits
        result |= (extracted_bits << (num_bits - bits_to_read));

        // Update the number of bits left to read
        num_bits -= bits_to_read;
    }

    reader.num_bits_read += num_bits;
    return result;
}

bool ReadOverflow(BitReader bitreader, int bits) {
    return (bitreader.num_bits_read + bits) > bitreader.total_bits;
}


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

        if (numElements > MaxElements) {
            //reader.Abort();
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
    packet_1.elements[1] = 5;
    packet_1.elements[2] = 5;
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

    
    BitReader reader;
    reader.scratch = 0;
    reader.scratch_bits = 0;
    reader.total_bits = 8 * sizeof(p_buffer);
    reader.num_bits_read = 0;
    reader.word_index = 0;
    reader.buffer = p_buffer;

    PacketB packet_2;

    packet_2.Read(reader);

    printf("packet2\n");
    printf("numElements: %d\n", packet_2.numElements);
    for (int i = 0; i < packet_2.numElements; ++i) {
        printf("elements[%d]: %d\n", i, packet_2.elements[i]);
    }
}