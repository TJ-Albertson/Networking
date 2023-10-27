#include "stream.hpp"
#include <iostream>
#include <bitset>

// Function to print an integer in binary
void printBinary(int n)
{
    const int numBits = 32;

    if (n == 0) {
        // Print leading zeros if the number is zero
        for (int i = 0; i < numBits; i++) {
            printf("0");
        }
        return;
    }

    int binary[numBits];
    int i = 0;

    // Convert the decimal number to binary
    while (n > 0) {
        binary[i] = n % 2;
        n = n / 2;
        i++;
    }

    // Print leading zeros to ensure a fixed number of bits
    for (int j = i; j < numBits; j++) {
        printf("0");
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
    packet1.elements[1] = 5;
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


     for (int i = 0; i < 20; i++) {
        std::bitset<8> binary(buffer[i]);
        std::cout << binary;
        if ((i + 1) % 4 == 0) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;

    uint32_t* intBuffer = (uint32_t*)buffer;

    for (int i = 0; i < 5; i++) {
        printf("Int Value %d: %u\n", i + 1, intBuffer[i]);
    }
    
        std::cout << std::endl;


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
