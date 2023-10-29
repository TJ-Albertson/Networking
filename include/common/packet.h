#ifndef PACKET_H
#define PACKET_H

#include "serializer.h"

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

    bool ReadSerialize(BitReader& reader)
    {
        read_serialize_int(reader, numElements, 0, MaxElements);
        for (int i = 0; i < numElements; ++i) {
            read_serialize_bits(reader, elements[i], 32);
        }
        return true;
    }

    bool WriteSerialize(BitWriter& writer)
    {
        write_serialize_int(writer, numElements, 0, MaxElements);
        for (int i = 0; i < numElements; ++i) {
            write_serialize_bits(writer, elements[i], 32);
        }
        return true;
    }
};


const int PacketBufferSize = 256; // size of packet buffer, eg. number of historical packets for which we can buffer fragments
const int MaxFragmentSize = 1024; // maximum size of a packet fragment
const int MaxFragmentsPerPacket = 256; // maximum number of fragments per-packet

const int MaxPacketSize = MaxFragmentSize * MaxFragmentsPerPacket;

struct PacketData {
    int size;
    uint8_t* data;
};

struct PacketBufferEntry {
    uint32_t sequence : 16; // packet sequence number
    uint32_t numFragments : 8; // number of fragments for this packet
    uint32_t receivedFragments : 8; // number of received fragments so far
    int fragmentSize[MaxFragmentsPerPacket]; // size of fragment n in bytes
    uint8_t* fragmentData[MaxFragmentsPerPacket]; // pointer to data for fragment n
};



#endif
