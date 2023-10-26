#include "stream.hpp"

// Function to print an integer in binary
void printBinary(int n)
{
    if (n == 0) {
        printf("0");
        return;
    }

    int binary[32];
    int i = 0;

    // Convert the decimal number to binary
    while (n > 0) {
        binary[i] = n % 2;
        n = n / 2;
        i++;
    }

    // Print the binary representation
    for (int j = i - 1; j >= 0; j--) {
        printf("%d", binary[j]);
    }
}

int main()
{
    uint8_t buffer[1024];

    // Create a packet
    PacketB packet1;
    packet1.numElements = 5;
    for (int i = 0; i < packet1.numElements; ++i) {
        //packet1.elements[i] = i+1;
    }
    packet1.elements[0] = 3;
    packet1.elements[1] = 7;
    packet1.elements[2] = 5;
    packet1.elements[3] = 6;
    packet1.elements[4] = 1;

    printf("packet1\n");
    printf("numElements: %d\n", packet1.numElements);
    for (int i = 0; i < packet1.numElements; ++i) {
        printf("elements[%d]: %d\n", i, packet1.elements[i]);
        printBinary(packet1.elements[i]);
        printf("\n");
    }

    printf("\n\n");


    // Serialize the packet to a buffer
    WriteStream writeStream(buffer, sizeof(buffer));
    packet1.Serialize(writeStream);



    // Create another packet and deserialize the buffer to the packet
    PacketB packet2;
    ReadStream readStream(buffer, sizeof(buffer));
    packet2.Serialize(readStream);

    // Print out the results
    printf("packet2\n");
    printf("numElements: %d\n", packet2.numElements);
    for (int i = 0; i < packet2.numElements; ++i) {
        printf("elements[%d]: %d\n", i, packet2.elements[i]);
        printBinary(packet2.elements[i]);
        printf("\n");
    }

    return 0;
}
