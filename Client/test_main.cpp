#include "stream.hpp"

int main()
{
    uint8_t buffer[1024];

    // Create a packet
    PacketB packet1;
    packet1.numElements = 5;
    for (int i = 0; i < packet1.numElements; ++i) {
        //packet1.elements[i] = i+1;
    }
    packet1.elements[0] = 5;
    packet1.elements[1] = 7;
    packet1.elements[2] = 5;
    packet1.elements[3] = 6;
    packet1.elements[4] = 1;


    // Serialize the packet to a buffer
    WriteStream writeStream(buffer, sizeof(buffer));
    packet1.Serialize(writeStream);

    // Create another packet and deserialize the buffer to the packet
    PacketB packet2;
    ReadStream readStream(buffer, sizeof(buffer));
    packet2.Serialize(readStream);

    // Print out the results
    printf("numElements: %d\n", packet2.numElements);
    for (int i = 0; i < packet2.numElements; ++i) {
        printf("elements[%d]: %d\n", i, packet2.elements[i]);
    }

    return 0;
}
