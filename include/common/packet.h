#ifndef PACKET_H
#define PACKET_H

#include "hash.h"
#include "serializer.h"
#include "utils.h"

const int PacketBufferSize = 256; // size of packet buffer, eg. number of historical packets for which we can buffer fragments
const int MaxFragmentSize = 1024; // maximum size of a packet fragment
const int MaxFragmentsPerPacket = 256; // maximum number of fragments per-packet
const int MaxPacketSize = MaxFragmentSize * MaxFragmentsPerPacket;
const int PacketFragmentHeaderBytes = 16;

struct PacketInfo {
    bool rawFormat; // if true packets are written in "raw" format without crc32 (useful for encrypted packets).
    int prefixBytes; // prefix this number of bytes when reading and writing packets. stick your own data there.
    uint32_t protocolId; // protocol id that distinguishes your protocol from other packets sent over UDP.
    // PacketFactory* packetFactory; // create packets and determine information about packet types. required.
    const uint8_t* allowedPacketTypes; // array of allowed packet types. if a packet type is not allowed the serialize read or write will fail.
    void* context; // context for the packet serialization (optional, pass in NULL)

    PacketInfo()
    {
        rawFormat = false;
        prefixBytes = 0;
        protocolId = 0;
        // packetFactory = NULL;
        allowedPacketTypes = NULL;
        context = NULL;
    }
};

enum TestPacketTypes {
    PACKET_FRAGMENT = 0, // IMPORTANT: packet type 0 indicates a packet fragment

    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,

    TEST_PACKET_NUM_TYPES
};

PacketInfo packetInfo;

struct PacketA {
    int x, y, z;

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

    bool Serialize(Stream& stream)
    {
        serialize_int(stream, numElements, 0, MaxElements);
        for (int i = 0; i < numElements; ++i) {
            serialize_bits(stream, elements[i], 32);
        }
        return true;
    }
};

static const int MaxItems = 4096 * 4;

struct TestPacketB {
    int numItems;
    int items[MaxItems];

    void randomFill()
    {
        //numItems = random_int(0, MaxItems);
        for (int i = 0; i < numItems; ++i)
            items[i] = random_int(-100, +100);
    }

    bool Serialize(Stream& stream)
    {
        serialize_int(stream, numItems, 0, MaxItems);
        for (int i = 0; i < numItems; ++i) {
            serialize_int(stream, items[i], -100, +100);
        }
        return true;
    }
};


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

struct FragmentPacket {
    // input/output

    int fragmentSize; // set as input on serialize write. output on serialize read (inferred from size of packet)

    // serialized data

    uint32_t crc32;
    uint16_t sequence;
    int packetType;
    uint8_t fragmentId;
    uint8_t numFragments;

    uint8_t fragmentData[MaxFragmentSize];

    bool Serialize(Stream& stream)
    {
        serialize_bits(stream, crc32, 32);
        serialize_bits(stream, sequence, 16);

        packetType = 0;
        serialize_int(stream, packetType, 0, TEST_PACKET_NUM_TYPES - 1);
        if (packetType != 0)
            return true;

        serialize_bits(stream, fragmentId, 8);
        serialize_bits(stream, numFragments, 8);

        serialize_align(stream);

        if (stream.type == READ) {
            assert((GetBitsRemaining(stream) % 8) == 0);
            fragmentSize = GetBitsRemaining(stream) / 8;
            if (fragmentSize <= 0 || fragmentSize > MaxFragmentSize) {
                printf("packet fragment size is out of bounds (%d)\n", fragmentSize);
                return false;
            }
        }

        assert(fragmentSize > 0);
        assert(fragmentSize <= MaxFragmentSize);

        serialize_bytes(stream, fragmentData, fragmentSize);

        return true;
    }
};



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
