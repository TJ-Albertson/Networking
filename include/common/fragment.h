#ifndef FRAGMENT_H
#define FRAGMENT_H


#include <assert.h>
#include <stdint.h>
#include "serializer.h"

const uint32_t ProtocolId = 0x55667788;

const int PacketFragmentHeaderBytes = 16;
const int MaxFragmentSize = 1024; 

enum TestPacketTypes {
    PACKET_FRAGMENT = 0, // IMPORTANT: packet type 0 indicates a packet fragment

    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,

    TEST_PACKET_NUM_TYPES
};

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


    bool ReadSerialize(BitReader& reader)
    {
        read_serialize_bits(reader, crc32, 32);
        read_serialize_bits(reader, sequence, 16);

        packetType = 0;
        read_serialize_int(reader, packetType, 0, TEST_PACKET_NUM_TYPES - 1);
        if (packetType != 0)
            return true;

        read_serialize_bits(reader, fragmentId, 8);
        read_serialize_bits(reader, numFragments, 8);

        read_serialize_align(reader);


        assert((stream.GetBitsRemaining() % 8) == 0);
        fragmentSize = stream.GetBitsRemaining() / 8;
        if (fragmentSize <= 0 || fragmentSize > MaxFragmentSize) {
            printf("packet fragment size is out of bounds (%d)\n", fragmentSize);
            return false;
        }


        assert(fragmentSize > 0);
        assert(fragmentSize <= MaxFragmentSize);

        read_serialize_bytes(reader, fragmentData, fragmentSize);

        return true;
    }

    bool WriteSerialize(BitReader& writer)
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

        if (Stream::IsReading) {
            assert((stream.GetBitsRemaining() % 8) == 0);
            fragmentSize = stream.GetBitsRemaining() / 8;
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

#endif
