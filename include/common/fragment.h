#ifndef FRAGMENT_H
#define FRAGMENT_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

#include "serializer.h"
#include "utils.h"
#include "packet.h"
//#include "packet_type_switch.h"





typedef struct PacketData {
    int size;
    uint8_t* data;
} PacketData;

typedef struct PacketBufferEntry {
    uint32_t sequence : 16; // packet sequence number
    uint32_t numFragments : 8; // number of fragments for this packet
    uint32_t receivedFragments : 8; // number of received fragments so far
    int fragmentSize[MaxFragmentsPerPacket]; // size of fragment n in bytes
    uint8_t* fragmentData[MaxFragmentsPerPacket]; // pointer to data for fragment n
} PacketBufferEntry;

typedef struct PacketBuffer {
    uint16_t currentSequence; // sequence number of most recent packet in buffer

    int numBufferedFragments; // total number of fragments stored in the packet buffer (across *all* packets)

    bool valid[PacketBufferSize]; // true if there is a valid buffered packet entry at this index

    PacketBufferEntry entries[PacketBufferSize]; // buffered packets in range [ current_sequence - PacketBufferSize + 1, current_sequence ] (modulo 65536)
} PacketBuffer;

static PacketBuffer packetBuffer;

/*
    Advance the current sequence for the packet buffer forward.
    This function removes old packet entries and frees their fragments.z
*/
void AdvancePacketBuffer(PacketBuffer* p_buffer, uint16_t sequence)
{
    if (!sequence_greater_than(sequence, p_buffer->currentSequence))
        return;

    const uint16_t oldestSequence = sequence - PacketBufferSize + 1;

    for (int i = 0; i < PacketBufferSize; ++i) {
        if (p_buffer->valid[i]) {
            if (sequence_less_than(p_buffer->entries[i].sequence, oldestSequence)) {
                printf("remove old packet entry %d\n", p_buffer->entries[i].sequence);

                for (int j = 0; j < (int)p_buffer->entries[i].numFragments; ++j) {
                    if (p_buffer->entries[i].fragmentData[j]) {
                        free(p_buffer->entries[i].fragmentData[j]);
                        assert(p_buffer->numBufferedFragments > 0);
                        p_buffer->numBufferedFragments--;
                    }
                }
            }

            memset(&p_buffer->entries[i], 0, sizeof(PacketBufferEntry));

            p_buffer->valid[i] = false;
        }
    }

    p_buffer->currentSequence = sequence;
}

/*
    Process packet fragment on receiver side.

    Stores each fragment ready to receive the whole packet once all fragments for that packet are received.

    If any fragment is dropped, fragments are not resent, the whole packet is dropped.

    NOTE: This function is fairly complicated because it must handle all possible cases
    of maliciously constructed packets attempting to overflow and corrupt the packet buffer!
*/
bool ProcessFragment(PacketBuffer* p_buffer, const uint8_t* fragmentData, int fragmentSize, uint16_t packetSequence, int fragmentId, int numFragmentsInPacket)
{
    assert(fragmentData);

    // fragment size is <= zero? discard the fragment.
    if (fragmentSize <= 0) {
        printf("fragment size is <= zero\n");
        return false;
    }
        
    // fragment size exceeds max fragment size? discard the fragment.
    if (fragmentSize > MaxFragmentSize) {
        printf("fragment size exceeds max fragment size\n");
        return false;
    }
        

    // num fragments outside of range? discard the fragment
    if (numFragmentsInPacket <= 0 || numFragmentsInPacket > MaxFragmentsPerPacket) {
        printf("num fragments outside of range\n");
        return false;
    }

    // fragment index out of range? discard the fragment
    if (fragmentId < 0 || fragmentId >= numFragmentsInPacket) {
        printf("fragment index out of range\n");
        return false;
    }

    // if this is not the last fragment in the packet and fragment size is not equal to MaxFragmentSize, discard the fragment
    if (fragmentId != numFragmentsInPacket - 1 && fragmentSize != MaxFragmentSize) {
        printf("not the last fragment in the packet and fragment size[%d] is not equal to MaxFragmentSize[%d]\n", fragmentSize, MaxFragmentSize);
        return false;
    }

    // packet sequence number wildly out of range from the current sequence? discard the fragment
    if (sequence_difference(packetSequence, p_buffer->currentSequence) > 1024) {
        printf("packet sequence number wildly out of range from the current sequence\n");
        return false;
    }

    // if the entry exists, but has a different sequence number, discard the fragment
    const int index = packetSequence % PacketBufferSize;
    if (p_buffer->valid[index] && p_buffer->entries[index].sequence != packetSequence) {
        printf("entry exists, but has a different sequence number\n");
        return false;
    }

    // if the entry does not exist, add an entry for this sequence # and set total fragments
    if (!p_buffer->valid[index]) {
        AdvancePacketBuffer(p_buffer, packetSequence);
        p_buffer->entries[index].sequence = packetSequence;
        p_buffer->entries[index].numFragments = numFragmentsInPacket;
        assert(p_buffer->entries[index].receivedFragments == 0); // IMPORTANT: Should have already been cleared to zeros in "Advance"
        p_buffer->valid[index] = true;
    }

    // at this point the entry must exist and have the same sequence number as the fragment
    assert(p_buffer->valid[index]);
    assert(p_buffer->entries[index].sequence == packetSequence);

    // if the total number fragments is different for this packet vs. the entry, discard the fragment
    if (numFragmentsInPacket != (int)p_buffer->entries[index].numFragments) {
        printf("total number fragments is different for this packet vs. the entry\n");
        return false;
    }

    // if this fragment has already been received, ignore it because it must have come from a duplicate packet
    assert(fragmentId < numFragmentsInPacket);
    assert(fragmentId < MaxFragmentsPerPacket);
    assert(numFragmentsInPacket <= MaxFragmentsPerPacket);

    if (p_buffer->entries[index].fragmentSize[fragmentId]) {
        printf("fragment has already been received\n");
        return false;
    }
        

    // add the fragment to the packet buffer
    printf("added fragment %d of packet %d to buffer\n", fragmentId, packetSequence);

    assert(fragmentSize > 0);
    assert(fragmentSize <= MaxFragmentSize);

    p_buffer->entries[index].fragmentSize[fragmentId] = fragmentSize;

    p_buffer->entries[index].fragmentData[fragmentId] = (uint8_t*)malloc(fragmentSize);

    memcpy(p_buffer->entries[index].fragmentData[fragmentId], fragmentData, fragmentSize);
    p_buffer->entries[index].receivedFragments++;

    assert(p_buffer->entries[index].receivedFragments <= p_buffer->entries[index].numFragments);

    p_buffer->numBufferedFragments++;

    return true;
}

void packet_buffer_recieve_packets(PacketBuffer* p_buffer, int* numPackets, PacketData packetData[])
{
    *numPackets = 0;

    const uint16_t oldestSequence = p_buffer->currentSequence - PacketBufferSize + 1;

    for (int i = 0; i < PacketBufferSize; ++i) {
        const uint16_t sequence = (uint16_t)((oldestSequence + i) & 0xFF);

        const int index = sequence % PacketBufferSize;

        if (p_buffer->valid[index] && p_buffer->entries[index].sequence == sequence) {

            // have all fragments arrived for this packet?
            if (p_buffer->entries[index].receivedFragments != p_buffer->entries[index].numFragments)
                continue;

            printf("received all fragments for packet %d [%d]\n", sequence, p_buffer->entries[index].numFragments);

            // what's the total size of this packet?
            int packetSize = 0;
            for (int j = 0; j < (int)p_buffer->entries[index].numFragments; ++j) {
                packetSize += p_buffer->entries[index].fragmentSize[j];
            }

            assert(packetSize > 0);
            assert(packetSize <= MaxPacketSize);

            // allocate a packet to return to the caller
            PacketData packet = packetData[*numPackets++];

            packet.size = packetSize;
            packet.data = (uint8_t*)malloc(packetSize);

            // reconstruct the packet from the fragments
            printf("reassembling packet %d from fragments (%d bytes)\n", sequence, packetSize);

            uint8_t* dst = packet.data;
            for (int j = 0; j < (int)p_buffer->entries[index].numFragments; ++j) 
            {
                memcpy(dst, p_buffer->entries[index].fragmentData[j], p_buffer->entries[index].fragmentSize[j]);
                dst += p_buffer->entries[index].fragmentSize[j];
            }

            // free all fragments
            for (int j = 0; j < (int)p_buffer->entries[index].numFragments; ++j) 
            {
                free(p_buffer->entries[index].fragmentData[j]);
                p_buffer->numBufferedFragments--;
            }

            // clear the packet buffer entry
            memset(&p_buffer->entries[index], 0, sizeof(PacketBufferEntry));

            p_buffer->valid[index] = false;
        }
    }
}



bool SplitPacketIntoFragments(uint16_t sequence, const uint8_t* packetData, int packetSize, int* numFragments, PacketData fragmentPackets[])
{
    numFragments = 0;

    assert(*packetData);
    assert(packetSize > 0);
    assert(packetSize < MaxPacketSize);

    *numFragments = (packetSize / MaxFragmentSize) + ((packetSize % MaxFragmentSize) != 0 ? 1 : 0);

    assert(*numFragments > 0);
    assert(*numFragments <= MaxFragmentsPerPacket);

    const uint8_t* src = packetData;

    printf("splitting packet into %d fragments\n", *numFragments);

    for (int i = 0; i < *numFragments; ++i) 
    {
        const int fragmentSize = (i == *numFragments - 1) ? ((int)((intptr_t)(*packetData + packetSize) - (intptr_t)(src))) : MaxFragmentSize;

        static const int MaxFragmentPacketSize = MaxFragmentSize + PacketFragmentHeaderBytes;

        fragmentPackets[i].data = (uint8_t*)malloc(MaxFragmentPacketSize);


        printf("fragmentSize: %d\n", fragmentSize);


        Stream writer;

        if (!r_stream_write_init(&writer, fragmentPackets[i].data, MaxFragmentPacketSize)) {
            printf("Failed to init read stream\n");
        }

        // uint32_t protocolId = host_to_network(packetInfo.protocolId);
        // uint32_t crc32 = calculate_crc32((uint8_t*)&protocolId, 4, 0);

        FragmentPacket fragmentPacket;
       
        fragmentPacket.fragmentSize = fragmentSize;
        fragmentPacket.crc32 = 0;
        //fragmentPacket.crc32 = crc32;

        fragmentPacket.sequence = sequence;
        fragmentPacket.fragmentId = (uint8_t)i;
        fragmentPacket.numFragments = (uint8_t)numFragments;
        memcpy(fragmentPacket.fragmentData, src, fragmentSize);


        if (!r_serialize_fragment(&writer, &fragmentPacket)) {
            numFragments = 0;
            for (int j = 0; j < i; ++j) {
                free(fragmentPackets[i].data);
                fragmentPackets[i].data = NULL;
                fragmentPackets[i].size = 0;
            }
            return false;
        }

        FlushBits(&writer);

        
        uint32_t protocolId = host_to_network(packetInfo.protocolId);
        uint32_t crc32 = calculate_crc32((uint8_t*)&protocolId, 4, 0);
        crc32 = calculate_crc32(fragmentPackets[i].data, GetBytesProcessed(&writer), crc32);
        *((uint32_t*)fragmentPackets[i].data) = host_to_network(crc32);
        
        

        printf("fragment packet %d: %d bytes\n", i, GetBytesProcessed(&writer));

        fragmentPackets[i].size = GetBytesProcessed(&writer);

        src += fragmentSize;
    }

    assert(src == packetData + packetSize);

    return true;
}



bool ProcessFragmentPacket(Stream* stream) {

    FragmentPacket fragmentPacket;
    
    Stream reader;

    r_stream_read_init(&reader, stream->data, stream->num_bits / 8);

    if (!r_serialize_fragment(&reader, &fragmentPacket)) {
        printf("error: fragment packet failed to serialize\n");
        return false;
    }

    //uint8_t data[200];
    if (!ProcessFragment(&packetBuffer, (uint8_t*)stream->data + 9/*PacketFragmentHeaderBytes*/, fragmentPacket.fragmentSize, fragmentPacket.sequence, fragmentPacket.fragmentId, fragmentPacket.numFragments)) {
        printf("error: failed to process fragment\n");
        return false;
    }

    int numPackets = 0;
    PacketData packets[PacketBufferSize];
    // Check to see if any packets have been completed
    packet_buffer_recieve_packets(&packetBuffer, &numPackets, packets);

    for (int j = 0; j < numPackets; ++j) {

        uint8_t* data = packets[j].data;
        int size = packets[j].size;

        Stream readStream;
        r_stream_read_init(&readStream, data, size);

        uint32_t packet_type = 0;
        r_serialize_bits(&readStream, &packet_type, 2);

        //packet_switch(packet_type, readStream);
    }
}

#endif
