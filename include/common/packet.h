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

enum TestPacketTypes {
    PACKET_FRAGMENT = 0, // IMPORTANT: packet type 0 indicates a packet fragment

    TEST_PACKET_A,
    TEST_PACKET_B,
    TEST_PACKET_C,

    TEST_PACKET_NUM_TYPES
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

struct PacketHeader {
    uint32_t crc32;
    uint16_t sequence;
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
};

/*
    Advance the current sequence for the packet buffer forward.
    This function removes old packet entries and frees their fragments.z
*/

void AdvancePacketBuffer(PacketBuffer& p_buffer, uint16_t sequence)
{
    if (!sequence_greater_than(sequence, p_buffer.currentSequence))
        return;

    const uint16_t oldestSequence = sequence - PacketBufferSize + 1;

    for (int i = 0; i < PacketBufferSize; ++i) {
        if (p_buffer.valid[i]) {
            if (sequence_less_than(p_buffer.entries[i].sequence, oldestSequence)) {
                printf("remove old packet entry %d\n", p_buffer.entries[i].sequence);

                for (int j = 0; j < (int)p_buffer.entries[i].numFragments; ++j) {
                    if (p_buffer.entries[i].fragmentData[j]) {
                        delete[] p_buffer.entries[i].fragmentData[j];
                        assert(p_buffer.numBufferedFragments > 0);
                        p_buffer.numBufferedFragments--;
                    }
                }
            }

            memset(&p_buffer.entries[i], 0, sizeof(PacketBufferEntry));

            p_buffer.valid[i] = false;
        }
    }

    p_buffer.currentSequence = sequence;
}

/*
    Process packet fragment on receiver side.

    Stores each fragment ready to receive the whole packet once all fragments for that packet are received.

    If any fragment is dropped, fragments are not resent, the whole packet is dropped.

    NOTE: This function is fairly complicated because it must handle all possible cases
    of maliciously constructed packets attempting to overflow and corrupt the packet buffer!
*/
bool ProcessFragment(PacketBuffer& p_buffer, const uint8_t* fragmentData, int fragmentSize, uint16_t packetSequence, int fragmentId, int numFragmentsInPacket)
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
    if (sequence_difference(packetSequence, p_buffer.currentSequence) > 1024)
        return false;

    // if the entry exists, but has a different sequence number, discard the fragment
    const int index = packetSequence % PacketBufferSize;

    if (p_buffer.valid[index] && p_buffer.entries[index].sequence != packetSequence)
        return false;

    // if the entry does not exist, add an entry for this sequence # and set total fragments
    if (!p_buffer.valid[index]) {
        AdvancePacketBuffer(p_buffer, packetSequence);
        p_buffer.entries[index].sequence = packetSequence;
        p_buffer.entries[index].numFragments = numFragmentsInPacket;
        assert(p_buffer.entries[index].receivedFragments == 0); // IMPORTANT: Should have already been cleared to zeros in "Advance"
        p_buffer.valid[index] = true;
    }

    // at this point the entry must exist and have the same sequence number as the fragment
    assert(p_buffer.valid[index]);
    assert(p_buffer.entries[index].sequence == packetSequence);

    // if the total number fragments is different for this packet vs. the entry, discard the fragment
    if (numFragmentsInPacket != (int)p_buffer.entries[index].numFragments)
        return false;

    // if this fragment has already been received, ignore it because it must have come from a duplicate packet
    assert(fragmentId < numFragmentsInPacket);
    assert(fragmentId < MaxFragmentsPerPacket);
    assert(numFragmentsInPacket <= MaxFragmentsPerPacket);

    if (p_buffer.entries[index].fragmentSize[fragmentId])
        return false;

    // add the fragment to the packet buffer
    printf("added fragment %d of packet %d to buffer\n", fragmentId, packetSequence);

    assert(fragmentSize > 0);
    assert(fragmentSize <= MaxFragmentSize);

    p_buffer.entries[index].fragmentSize[fragmentId] = fragmentSize;
    p_buffer.entries[index].fragmentData[fragmentId] = new uint8_t[fragmentSize];
    memcpy(p_buffer.entries[index].fragmentData[fragmentId], fragmentData, fragmentSize);
    p_buffer.entries[index].receivedFragments++;

    assert(p_buffer.entries[index].receivedFragments <= p_buffer.entries[index].numFragments);

    p_buffer.numBufferedFragments++;

    return true;
}

bool ProcessPacket(PacketBuffer& p_buffer, const uint8_t* data, int size)
{

    Stream reader;

    if (!InitReadStream(reader, data, size)) {
        printf("Failed to init read stream\n");
    }

    FragmentPacket fragmentPacket;

    if (!fragmentPacket.Serialize(reader)) {
        printf("error: fragment packet failed to serialize\n");
        return false;
    }

    uint32_t protocolId = host_to_network(ProtocolId);
    uint32_t crc32 = calculate_crc32((const uint8_t*)&protocolId, 4, 0);
    uint32_t zero = 0;
    crc32 = calculate_crc32((const uint8_t*)&zero, 4, crc32);
    crc32 = calculate_crc32(data + 4, size - 4, crc32);

    if (crc32 != fragmentPacket.crc32) {
        printf("corrupt packet: expected crc32 %x, got %x\n", crc32, fragmentPacket.crc32);
        return false;
    }

    if (fragmentPacket.packetType == 0) {
        return ProcessFragment(p_buffer, data + PacketFragmentHeaderBytes, fragmentPacket.fragmentSize, fragmentPacket.sequence, fragmentPacket.fragmentId, fragmentPacket.numFragments);
    } else {
        return ProcessFragment(p_buffer, data, size, fragmentPacket.sequence, 0, 1);
    }

    return true;
}

void ReceivePackets(PacketBuffer& p_buffer, int& numPackets, PacketData packetData[])
{
    numPackets = 0;

    const uint16_t oldestSequence = p_buffer.currentSequence - PacketBufferSize + 1;

    for (int i = 0; i < PacketBufferSize; ++i) {
        const uint16_t sequence = uint16_t((oldestSequence + i) & 0xFF);

        const int index = sequence % PacketBufferSize;

        if (p_buffer.valid[index] && p_buffer.entries[index].sequence == sequence) {

            // have all fragments arrived for this packet?
            if (p_buffer.entries[index].receivedFragments != p_buffer.entries[index].numFragments)
                continue;

            printf("received all fragments for packet %d [%d]\n", sequence, p_buffer.entries[index].numFragments);

            // what's the total size of this packet?
            int packetSize = 0;
            for (int j = 0; j < (int)p_buffer.entries[index].numFragments; ++j) {
                packetSize += p_buffer.entries[index].fragmentSize[j];
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
            for (int j = 0; j < (int)p_buffer.entries[index].numFragments; ++j) {
                memcpy(dst, p_buffer.entries[index].fragmentData[i], p_buffer.entries[index].fragmentSize[i]);
                dst += p_buffer.entries[index].fragmentSize[i];
            }

            // free all fragments
            for (int j = 0; j < (int)p_buffer.entries[index].numFragments; ++j) {
                delete[] p_buffer.entries[index].fragmentData[j];
                p_buffer.numBufferedFragments--;
            }

            // clear the packet buffer entry
            memset(&p_buffer.entries[index], 0, sizeof(PacketBufferEntry));

            p_buffer.valid[index] = false;
        }
    }
}

static PacketBuffer packetBuffer;

bool SplitPacketIntoFragments(uint16_t sequence, const uint8_t* packetData, int packetSize, int& numFragments, PacketData fragmentPackets[])
{
    numFragments = 0;

    assert(packetData);
    assert(packetSize > 0);
    assert(packetSize < MaxPacketSize);

   
    numFragments = (packetSize / MaxFragmentSize) + ((packetSize % MaxFragmentSize) != 0 ? 1 : 0);

    assert(numFragments > 0);
    assert(numFragments <= MaxFragmentsPerPacket);

    const uint8_t* src = packetData;

    printf("splitting packet into %d fragments\n", numFragments);

    for (int i = 0; i < numFragments; ++i) {
        const int fragmentSize = (i == numFragments - 1) ? ((int)(intptr_t(packetData + packetSize) - intptr_t(src))) : MaxFragmentSize;

        static const int MaxFragmentPacketSize = MaxFragmentSize + PacketFragmentHeaderBytes;

        fragmentPackets[i].data = new uint8_t[MaxFragmentPacketSize];

        Stream writer;

        if (!InitWriteStream(writer, fragmentPackets[i].data, MaxFragmentPacketSize)) {
            printf("Failed to init read stream\n");
        }

        FragmentPacket fragmentPacket;
        fragmentPacket.fragmentSize = fragmentSize;
        fragmentPacket.crc32 = 0;
        fragmentPacket.sequence = sequence;
        fragmentPacket.fragmentId = (uint8_t)i;
        fragmentPacket.numFragments = (uint8_t)numFragments;
        memcpy(fragmentPacket.fragmentData, src, fragmentSize);

        if (!fragmentPacket.Serialize(writer)) {
            numFragments = 0;
            for (int j = 0; j < i; ++j) {
                delete fragmentPackets[i].data;
                fragmentPackets[i].data = NULL;
                fragmentPackets[i].size = 0;
            }
            return false;
        }

        FlushBits(writer);

        uint32_t protocolId = host_to_network(ProtocolId);
        uint32_t crc32 = calculate_crc32((uint8_t*)&protocolId, 4, 0);
        crc32 = calculate_crc32(fragmentPackets[i].data, GetBytesProcessed(writer), crc32);

        *((uint32_t*)fragmentPackets[i].data) = host_to_network(crc32);

        printf("fragment packet %d: %d bytes\n", i, GetBytesProcessed(writer));

        fragmentPackets[i].size = GetBytesProcessed(writer);

        src += fragmentSize;
    }

    assert(src == packetData + packetSize);

    return true;
}

int WritePacket(const PacketInfo& info,
    Packet* packet,
    uint8_t* buffer,
    int bufferSize,
    Object* header)
{
    assert(packet);
    assert(buffer);
    assert(bufferSize > 0);
    assert(info.protocolId);
    assert(info.packetFactory);

    const int numPacketTypes = info.packetFactory->GetNumPacketTypes();

    Stream writeStream;
    InitWriteStream(writeStream, buffer, bufferSize);

    // stream.SetContext(info.context);

    for (int i = 0; i < info.prefixBytes; ++i) {
        uint32_t zero = 0;
        SerializeBits(writeStream, zero, 8);
    }

    uint32_t crc32 = 0;

    if (!info.rawFormat)
        SerializeBits(writeStream, crc32, 32);

    if (header) {
        if (!header->SerializeInternal(stream))
            return 0;
    }

    int packetType = packet->GetType();

    assert(numPacketTypes > 0);

    if (numPacketTypes > 1) {
        SerializeInteger(writeStream, packetType, 0, numPacketTypes - 1);
    }

    if (!packet->SerializeInternal(stream))
        return 0;

    stream.SerializeCheck("end of packet");

    stream.Flush();

    if (!info.rawFormat) {
        uint32_t network_protocolId = host_to_network(info.protocolId);
        crc32 = calculate_crc32((uint8_t*)&network_protocolId, 4, 0);
        crc32 = calculate_crc32(buffer + info.prefixBytes, stream.GetBytesProcessed() - info.prefixBytes, crc32);
        *((uint32_t*)(buffer + info.prefixBytes)) = host_to_network(crc32);
    }

    if (stream.GetError())
        return 0;

    return stream.GetBytesProcessed();
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

void ReadPacket(
    const uint8_t* buffer,
    int bufferSize,
    Object* header,
    int* errorCode)
{
    assert(buffer);
    assert(bufferSize > 0);

    Stream readStream;
    InitReadStream(readStream, buffer, bufferSize);

    // stream.SetContext(info.context);

    for (int i = 0; i < packetInfo.prefixBytes; ++i) {
        uint32_t dummy = 0;
        SerializeBits(readStream, dummy, 8);
    }

    uint32_t read_crc32 = 0;
    if (!packetInfo.rawFormat) {
        SerializeBits(readStream, read_crc32, 32);

        uint32_t network_protocolId = host_to_network(packetInfo.protocolId);
        uint32_t crc32 = calculate_crc32((const uint8_t*)&network_protocolId, 4, 0);
        uint32_t zero = 0;

        crc32 = calculate_crc32((const uint8_t*)&zero, 4, crc32);
        crc32 = calculate_crc32(buffer + packetInfo.prefixBytes + 4, bufferSize - 4 - packetInfo.prefixBytes, crc32);

        if (crc32 != read_crc32) {
            printf("corrupt packet. expected crc32 %x, got %x\n", crc32, read_crc32);
            return;
        }
    }

    uint16_t sequence = 0;
    SerializeBits(readStream, sequence, 16);

    int packetType = 0;

    const int numPacketTypes = TEST_PACKET_NUM_TYPES;

    assert(numPacketTypes > 0);

    if (numPacketTypes > 1) {
        if (!SerializeInteger(readStream, packetType, 0, numPacketTypes - 1)) {
            printf("Invalid PacketType: %d\n", packetType);
            return;
        }
    }

    if (packetInfo.allowedPacketTypes) {
        if (!packetInfo.allowedPacketTypes[packetType]) {
            if (errorCode)
                printf("PacketType not allowed: %d\n", packetType);
            return;
        }
    }

#if PROTOCOL2_SERIALIZE_CHECKS
    if (!stream.SerializeCheck("end of packet")) {
        if (errorCode)
            *errorCode = PROTOCOL2_ERROR_SERIALIZE_CHECK_FAILED;
        goto cleanup;
    }
#endif // #if PROTOCOL2_SERIALIZE_CHECKS

    return;
}

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

void RecievePackets()
{

    uint8_t buffer[MaxPacketSize];

    int numPackets = 0;
    PacketData packets[PacketBufferSize];
    // packetBuffer.ReceivePackets(numPackets, packets);
    uint16_t local_sequence = 0;
    uint16_t remote_sequence = 0;

    while (1) {

        packetBuffer.ProcessPacket(buffer, bytesWritten);

        int numPackets = 0;
        PacketData packets[PacketBufferSize];
        packetBuffer.ReceivePackets(numPackets, packets);

        uint8_t buffer[MaxPacketSize];
        int bufferSize;
        Header header;

        ReadPacket(buffer, bufferSize, header);

        if (header.crc != packetInfo.crc) {
        }

        remote.sequence = header.sequence;
    }






}

#endif
