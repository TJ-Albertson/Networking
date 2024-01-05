#ifndef PACKET_H
#define PACKET_H

#include "hash.h"
#include "serializer.h"
#include "utils.h"

#define PacketBufferSize 256 // size of packet buffer, eg. number of historical packets for which we can buffer fragments
#define MaxFragmentSize 1024 // maximum size of a packet fragment
#define MaxFragmentsPerPacket 256 // maximum number of fragments per-packet
#define MaxPacketSize (MaxFragmentSize * MaxFragmentsPerPacket)
#define PacketFragmentHeaderBytes 16

typedef unsigned int uint;

typedef struct PacketInfo {
    bool rawFormat; // if true packets are written in "raw" format without crc32 (useful for encrypted packets).
    int prefixBytes; // prefix this number of bytes when reading and writing packets. stick your own data there.
    uint32_t protocolId; // protocol id that distinguishes your protocol from other packets sent over UDP.
    // PacketFactory* packetFactory; // create packets and determine information about packet types. required.
    const uint8_t* allowedPacketTypes; // array of allowed packet types. if a packet type is not allowed the serialize read or write will fail.
    void* context; // context for the packet serialization (optional, pass in NULL)
} PacketInfo;

enum TestPacketTypes {
    PACKET_FRAGMENT = 0, // IMPORTANT: packet type 0 indicates a packet fragment

    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,

    TEST_PACKET_NUM_TYPES
};

PacketInfo packetInfo;

typedef struct {
    int x, y, z;
} PacketA;

bool r_serialize_packet_a(Stream* stream, PacketA* packet)
{
    r_serialize_bits(stream, &packet->x, 32);
    r_serialize_bits(stream, &packet->y, 32);
    r_serialize_bits(stream, &packet->z, 32);
    return true;
}


typedef struct PacketB {
    int numElements;
    int elements[MaxElements];
} PacketB;

bool r_serialize_packet_b(Stream* stream, PacketB* packet)
{
    r_serialize_int(stream, &packet->numElements, 0, MaxElements);
    for (int i = 0; i < packet->numElements; ++i) {
        r_serialize_bits(stream, &packet->elements[i], 32);
    }
    return true;
}

#define MaxItems (4096 * 4)

typedef struct TestPacketB {
    int numItems;
    int items[MaxItems];
} TestPacketB;

bool r_serialize_test_packet_b(Stream* stream, TestPacketB* packet)
{
    r_serialize_int(stream, &packet->numItems, 0, MaxItems);
    for (int i = 0; i < packet->numItems; ++i) {
        r_serialize_int(stream, &packet->items[i], -100, +100);
    }
    return true;
}

void r_test_packet_b_fill(TestPacketB* packet)
{
    // numItems = random_int(0, MaxItems);
    for (int i = 0; i < packet->numItems; ++i)
        packet->items[i] = random_int(-100, +100);
}


/*
[protocol id] (32 bits)   // not actually sent, but used to calc crc32
[crc32] (32 bits)
[sequence] (16 bits)
[packet type = 0] (2 bits)
[fragment id] (8 bits)
[num fragments] (8 bits)
[pad zero bits to nearest byte index]
<fragment data>

*/

typedef struct FragmentPacket {
    // input/output

    int fragmentSize; // set as input on serialize write. output on serialize read (inferred from size of packet)

    // serialized data
    uint32_t crc32;
    uint16_t sequence;
    int packetType;
    uint8_t fragmentId;
    uint8_t numFragments;

    uint8_t fragmentData[MaxFragmentSize];
} FragmentPacket;

bool r_serialize_fragment(Stream* stream, FragmentPacket* packet)
{
    r_serialize_bits(stream, &packet->crc32, 32);
    r_serialize_bits(stream, &packet->sequence, 16);

    packet->packetType = 0;
    r_serialize_int(stream, &packet->packetType, 0, TEST_PACKET_NUM_TYPES - 1);
    if (packet->packetType != 0)
        return true;

    r_serialize_bits(stream, &packet->fragmentId, 8);
    r_serialize_bits(stream, &packet->numFragments, 8);

    serialize_align(stream);

    if (stream->type == READ) {
        assert((GetBitsRemaining(stream) % 8) == 0);
        packet->fragmentSize = GetBitsRemaining(stream) / 8;
        if (packet->fragmentSize <= 0 || packet->fragmentSize > MaxFragmentSize) {
            printf("packet fragment size is out of bounds (%d)\n", packet->fragmentSize);
            return false;
        }
    }

    assert(packet->fragmentSize > 0);
    assert(packet->fragmentSize <= MaxFragmentSize);

    serialize_bytes(stream, packet->fragmentData, packet->fragmentSize);

    return true;
}



/*

[protocol id] (32 bits)   // not actually sent, but used to calc crc32
[crc32] (32 bits)

[packet type = 0] (2 bits)

[sequence] (16 bits)
[fragment id] (8 bits)
[num fragments] (8 bits)
[pad zero bits to nearest byte index]
<fragment data>

*/

/*
void SendPacket()
{

    // Create packet with crc32, sequence, packet type, and data

    // if total packet size larger than [size] then fragment packet
    uint8_t buffer[MaxPacketSize];

    if (bytesWritten > MaxFragmentSize) {
        int numFragments;
        PacketData fragmentPackets[MaxFragmentsPerPacket];
        SplitPacketIntoFragments(sequence, buffer, bytesWritten, numFragments, fragmentPackets);

        for (int j = 0; j < numFragments; ++j)
            //  send fragments

            SendPacket(fragmentPackets[j].data, fragmentPackets[j].size);

        //  //packetBuffer.ProcessPacket(fragmentPackets[j].data, fragmentPackets[j].size);
    } else {
        printf("sending packet %d as a regular packet\n", sequence);

        // packetBuffer.ProcessPacket(buffer, bytesWritten);
    }
}

*/



#endif
