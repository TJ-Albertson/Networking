#ifndef CHUNK_H
#define CHUNK_H

class ChunkSender {
    bool sending; // true if we are currently sending a chunk. can only send one chunk at a time
    uint16_t chunkId; // the chunk id. starts at 0 and increases as each chunk is successfully sent and acked.
    int chunkSize; // the size of the chunk that is being sent in bytes
    int numSlices; // the number of slices in the current chunk being sent
    int currentSliceId; // the current slice id to be considered next time we send a slice packet. iteration starts here.
    int numAckedSlices; // number of slices acked by the receiver. when num slices acked = num slices, the send is completed.
    bool acked[MaxSlicesPerChunk]; // acked flag for each slice of the chunk. chunk send completes when all slices are acked. acked slices are skipped when iterating for next slice to send.
    double timeLastSent[MaxSlicesPerChunk]; // time the slice of the chunk was last sent. avoids redundant behavior
    uint8_t chunkData[MaxChunkSize]; // chunk data being sent.

public:
    ChunkSender()
    {
        memset(this, 0, sizeof(ChunkSender));
    }

    void SendChunk(const uint8_t* data, int size)
    {
        assert(data);
        assert(size > 0);
        assert(size <= MaxChunkSize);
        assert(!IsSending());

        sending = true;
        chunkSize = size;
        currentSliceId = 0;
        numAckedSlices = 0;

        numSlices = (size + SliceSize - 1) / SliceSize;

        assert(numSlices > 0);
        assert(numSlices <= MaxSlicesPerChunk);
        assert((numSlices - 1) * SliceSize < chunkSize);
        assert(numSlices * SliceSize >= chunkSize);

        memset(acked, 0, sizeof(acked));
        memset(timeLastSent, 0, sizeof(timeLastSent));
        memcpy(chunkData, data, size);

        printf("sending chunk %d in %d slices (%d bytes)\n", chunkId, numSlices, chunkSize);
    }

    bool IsSending()
    {
        return sending;
    }

    SlicePacket* GenerateSlicePacket(double t)
    {
        if (!sending)
            return NULL;

        SlicePacket* packet = NULL;

        for (int i = 0; i < numSlices; ++i) {
            const int sliceId = (currentSliceId + i) % numSlices;

            if (acked[sliceId])
                continue;

            if (timeLastSent[sliceId] + SliceMinimumResendTime < t) {
                packet = (SlicePacket*)packetFactory.CreatePacket(SLICE_PACKET);
                packet->chunkId = chunkId;
                packet->sliceId = sliceId;
                packet->numSlices = numSlices;
                packet->sliceBytes = (sliceId == numSlices - 1) ? (SliceSize - (SliceSize * numSlices - chunkSize)) : SliceSize;
                memcpy(packet->data, chunkData + sliceId * SliceSize, packet->sliceBytes);
                printf("sent slice %d of chunk %d (%d bytes)\n", sliceId, chunkId, packet->sliceBytes);
                break;
            }
        }

        currentSliceId = (currentSliceId + 1) % numSlices;

        return packet;
    }

    bool ProcessAckPacket(AckPacket* packet)
    {
        assert(packet);

        if (!sending)
            return false;

        if (packet->chunkId != chunkId)
            return false;

        if (packet->numSlices != numSlices)
            return false;

        for (int i = 0; i < numSlices; ++i) {
            if (acked[i] == false && packet->acked[i]) {
                acked[i] = true;
                numAckedSlices++;
                assert(numAckedSlices >= 0);
                assert(numAckedSlices <= numSlices);
                printf("acked slice %d of chunk %d [%d/%d]\n", i, chunkId, numAckedSlices, numSlices);
                if (numAckedSlices == numSlices) {
                    printf("all slices of chunk %d acked, send completed\n", chunkId);
                    sending = false;
                    chunkId++;
                }
            }
        }

        return true;
    }
};

class ChunkReceiver {
    bool receiving; // true if we are currently receiving a chunk.
    bool readyToRead; // true if a chunk has been received and is ready for the caller to read.
    bool forceAckPreviousChunk; // if this flag is set then we need to send a complete ack for the previous chunk id (sender has not yet received an ack with all slices received)
    int previousChunkNumSlices; // number of slices in the previous chunk received. used for force ack of previous chunk.
    uint16_t chunkId; // id of the chunk that is currently being received, or
    int chunkSize; // the size of the chunk that has been received. only known once the last slice has been received!
    int numSlices; // the number of slices in the current chunk being sent
    int numReceivedSlices; // number of slices received for the current chunk. when num slices receive = num slices, the receive is complete.
    double timeLastAckSent; // time last ack was sent. used to rate limit acks to some maximum number of acks per-second.
    bool received[MaxSlicesPerChunk]; // received flag for each slice of the chunk. chunk receive completes when all slices are received.
    uint8_t chunkData[MaxChunkSize]; // chunk data being received.

public:
    ChunkReceiver()
    {
        memset(this, 0, sizeof(ChunkReceiver));
    }

    bool ProcessSlicePacket(SlicePacket* packet)
    {
        assert(packet);

        // caller has to ead the chunk out of the recieve buffer before we can receive the next one
        if (readyToRead)
            return false;

        if (!receiving && packet->chunkId == uint16_t(chunkId - 1) && previousChunkNumSlices != 0) {
            // otherwise the sender gets stuck if the last ack packet is dropped due to packet loss
            forceAckPreviousChunk = true;
        }

        if (!receiving && packet->chunkId == chunkId) {
            printf("started receiving chunk %d\n", chunkId);

            assert(!readyToRead);

            receiving = true;
            forceAckPreviousChunk = false;
            numReceivedSlices = 0;
            chunkSize = 0;

            numSlices = packet->numSlices;
            assert(numSlices > 0);
            assert(numSlices <= MaxSlicesPerChunk);

            memset(received, 0, sizeof(received));
        }

        if (packet->chunkId != chunkId)
            return false;

        if (packet->numSlices != numSlices)
            return false;

        assert(packet->sliceId >= 0);
        assert(packet->sliceId <= numSlices);

        if (!received[packet->sliceId]) {
            received[packet->sliceId] = true;

            assert(packet->sliceBytes > 0);
            assert(packet->sliceBytes <= SliceSize);

            memcpy(chunkData + packet->sliceId * SliceSize, packet->data, packet->sliceBytes);

            numReceivedSlices++;

            assert(numReceivedSlices > 0);
            assert(numReceivedSlices <= numSlices);

            printf("received slice %d of chunk %d [%d/%d]\n", packet->sliceId, chunkId, numReceivedSlices, numSlices);

            if (packet->sliceId == numSlices - 1) {
                chunkSize = (numSlices - 1) * SliceSize + packet->sliceBytes;
                printf("received chunk size is %d\n", chunkSize);
            }

            if (numReceivedSlices == numSlices) {
                printf("received all slices for chunk %d\n", chunkId);
                receiving = false;
                readyToRead = true;
                previousChunkNumSlices = numSlices;
                chunkId++;
            }
        }

        return true;
    }

    AckPacket* GenerateAckPacket(double t)
    {
        if (timeLastAckSent + MinimumTimeBetweenAcks > t)
            return NULL;

        if (forceAckPreviousChunk && previousChunkNumSlices != 0) {
            timeLastAckSent = t;
            forceAckPreviousChunk = false;

            AckPacket* packet = (AckPacket*)packetFactory.CreatePacket(ACK_PACKET);
            packet->chunkId = uint16_t(chunkId - 1);
            packet->numSlices = previousChunkNumSlices;
            assert(previousChunkNumSlices > 0);
            assert(previousChunkNumSlices <= MaxSlicesPerChunk);
            for (int i = 0; i < previousChunkNumSlices; ++i)
                packet->acked[i] = true;

            return packet;
        }

        if (receiving) {
            timeLastAckSent = t;

            AckPacket* packet = (AckPacket*)packetFactory.CreatePacket(ACK_PACKET);
            packet->chunkId = chunkId;
            packet->numSlices = numSlices;
            for (int i = 0; i < numSlices; ++i)
                packet->acked[i] = received[i];

            return packet;
        }

        return NULL;
    }

    const uint8_t* ReadChunk(int& resultChunkSize)
    {
        if (!readyToRead)
            return NULL;
        readyToRead = false;
        resultChunkSize = chunkSize;
        return chunkData;
    }
};

#endif 