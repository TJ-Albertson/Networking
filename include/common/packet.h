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

#endif
