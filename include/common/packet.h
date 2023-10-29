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

struct PacketBuffer {
    PacketBuffer() { memset(this, 0, sizeof(PacketBuffer)); }

    uint16_t currentSequence; // sequence number of most recent packet in buffer

    int numBufferedFragments; // total number of fragments stored in the packet buffer (across *all* packets)

    bool valid[PacketBufferSize]; // true if there is a valid buffered packet entry at this index

    PacketBufferEntry entries[PacketBufferSize]; // buffered packets in range [ current_sequence - PacketBufferSize + 1, current_sequence ] (modulo 65536)

    /*
        Advance the current sequence for the packet buffer forward.
        This function removes old packet entries and frees their fragments.z
    */

    void Advance(uint16_t sequence)
    {
        if (!protocol2::sequence_greater_than(sequence, currentSequence))
            return;

        const uint16_t oldestSequence = sequence - PacketBufferSize + 1;

        for (int i = 0; i < PacketBufferSize; ++i) {
            if (valid[i]) {
                if (protocol2::sequence_less_than(entries[i].sequence, oldestSequence)) {
                    printf("remove old packet entry %d\n", entries[i].sequence);

                    for (int j = 0; j < (int)entries[i].numFragments; ++j) {
                        if (entries[i].fragmentData[j]) {
                            delete[] entries[i].fragmentData[j];
                            assert(numBufferedFragments > 0);
                            numBufferedFragments--;
                        }
                    }
                }

                memset(&entries[i], 0, sizeof(PacketBufferEntry));

                valid[i] = false;
            }
        }

        currentSequence = sequence;
    }

    /*
        Process packet fragment on receiver side.

        Stores each fragment ready to receive the whole packet once all fragments for that packet are received.

        If any fragment is dropped, fragments are not resent, the whole packet is dropped.

        NOTE: This function is fairly complicated because it must handle all possible cases
        of maliciously constructed packets attempting to overflow and corrupt the packet buffer!
    */

    bool ProcessFragment(const uint8_t* fragmentData, int fragmentSize, uint16_t packetSequence, int fragmentId, int numFragmentsInPacket)
    {
        assert(fragmentData);

        // fragment size is <= zero? discard the fragment.

        if (fragmentSize <= 0)
            return false;

        // fragment size exceeds max fragment size? discard the fragment.

        if (fragmentSize > MaxFragmentSize)
            return false;

        // num fragments outside of range? discard the fragment

        if (numFragmentsInPacket <= 0 || numFragmentsInPacket > MaxFragmentsPerPacket)
            return false;

        // fragment index out of range? discard the fragment

        if (fragmentId < 0 || fragmentId >= numFragmentsInPacket)
            return false;

        // if this is not the last fragment in the packet and fragment size is not equal to MaxFragmentSize, discard the fragment

        if (fragmentId != numFragmentsInPacket - 1 && fragmentSize != MaxFragmentSize)
            return false;

        // packet sequence number wildly out of range from the current sequence? discard the fragment

        if (protocol2::sequence_difference(packetSequence, currentSequence) > 1024)
            return false;

        // if the entry exists, but has a different sequence number, discard the fragment

        const int index = packetSequence % PacketBufferSize;

        if (valid[index] && entries[index].sequence != packetSequence)
            return false;

        // if the entry does not exist, add an entry for this sequence # and set total fragments

        if (!valid[index]) {
            Advance(packetSequence);
            entries[index].sequence = packetSequence;
            entries[index].numFragments = numFragmentsInPacket;
            assert(entries[index].receivedFragments == 0); // IMPORTANT: Should have already been cleared to zeros in "Advance"
            valid[index] = true;
        }

        // at this point the entry must exist and have the same sequence number as the fragment

        assert(valid[index]);
        assert(entries[index].sequence == packetSequence);

        // if the total number fragments is different for this packet vs. the entry, discard the fragment

        if (numFragmentsInPacket != (int)entries[index].numFragments)
            return false;

        // if this fragment has already been received, ignore it because it must have come from a duplicate packet

        assert(fragmentId < numFragmentsInPacket);
        assert(fragmentId < MaxFragmentsPerPacket);
        assert(numFragmentsInPacket <= MaxFragmentsPerPacket);

        if (entries[index].fragmentSize[fragmentId])
            return false;

        // add the fragment to the packet buffer

        printf("added fragment %d of packet %d to buffer\n", fragmentId, packetSequence);

        assert(fragmentSize > 0);
        assert(fragmentSize <= MaxFragmentSize);

        entries[index].fragmentSize[fragmentId] = fragmentSize;
        entries[index].fragmentData[fragmentId] = new uint8_t[fragmentSize];
        memcpy(entries[index].fragmentData[fragmentId], fragmentData, fragmentSize);
        entries[index].receivedFragments++;

        assert(entries[index].receivedFragments <= entries[index].numFragments);

        numBufferedFragments++;

        return true;
    }

    bool ProcessPacket(const uint8_t* data, int size)
    {
        protocol2::ReadStream stream(data, size);

        FragmentPacket fragmentPacket;

        if (!fragmentPacket.SerializeInternal(stream)) {
            printf("error: fragment packet failed to serialize\n");
            return false;
        }

        uint32_t protocolId = protocol2::host_to_network(ProtocolId);
        uint32_t crc32 = protocol2::calculate_crc32((const uint8_t*)&protocolId, 4);
        uint32_t zero = 0;
        crc32 = protocol2::calculate_crc32((const uint8_t*)&zero, 4, crc32);
        crc32 = protocol2::calculate_crc32(data + 4, size - 4, crc32);

        if (crc32 != fragmentPacket.crc32) {
            printf("corrupt packet: expected crc32 %x, got %x\n", crc32, fragmentPacket.crc32);
            return false;
        }

        if (fragmentPacket.packetType == 0) {
            return ProcessFragment(data + PacketFragmentHeaderBytes, fragmentPacket.fragmentSize, fragmentPacket.sequence, fragmentPacket.fragmentId, fragmentPacket.numFragments);
        } else {
            return ProcessFragment(data, size, fragmentPacket.sequence, 0, 1);
        }

        return true;
    }

    void ReceivePackets(int& numPackets, PacketData packetData[])
    {
        numPackets = 0;

        const uint16_t oldestSequence = currentSequence - PacketBufferSize + 1;

        for (int i = 0; i < PacketBufferSize; ++i) {
            const uint16_t sequence = uint16_t((oldestSequence + i) & 0xFF);

            const int index = sequence % PacketBufferSize;

            if (valid[index] && entries[index].sequence == sequence) {
                // have all fragments arrived for this packet?

                if (entries[index].receivedFragments != entries[index].numFragments)
                    continue;

                printf("received all fragments for packet %d [%d]\n", sequence, entries[index].numFragments);

                // what's the total size of this packet?

                int packetSize = 0;
                for (int j = 0; j < (int)entries[index].numFragments; ++j) {
                    packetSize += entries[index].fragmentSize[j];
                }

                assert(packetSize > 0);
                assert(packetSize <= MaxPacketSize);

                // allocate a packet to return to the caller

                PacketData& packet = packetData[numPackets++];

                packet.size = packetSize;
                packet.data = new uint8_t[packetSize];

                // reconstruct the packet from the fragments

                printf("reassembling packet %d from fragments (%d bytes)\n", sequence, packetSize);

                uint8_t* dst = packet.data;
                for (int j = 0; j < (int)entries[index].numFragments; ++j) {
                    memcpy(dst, entries[index].fragmentData[i], entries[index].fragmentSize[i]);
                    dst += entries[index].fragmentSize[i];
                }

                // free all fragments

                for (int j = 0; j < (int)entries[index].numFragments; ++j) {
                    delete[] entries[index].fragmentData[j];
                    numBufferedFragments--;
                }

                // clear the packet buffer entry

                memset(&entries[index], 0, sizeof(PacketBufferEntry));

                valid[index] = false;
            }
        }
    }
};

#endif
