#include "socket.h"
#include <iostream>


void printStringFromBuffer(const unsigned char* buffer, int length)
{
    // Make sure the buffer is null-terminated.
    if (length > 0) {
        if (buffer[length - 1] != '\0') {
            std::cerr << "Warning: The buffer is not null-terminated." << std::endl;
        }
    }

    // Print the string.
    std::cout << "Received: " << buffer << std::endl;
}

int main()
{
    InitializeSockets();

    const int port = 30001;

    SocketHandle socket;

    if (!CreateSocket(socket, port)) {
        printf("failed to create socket!\n");
        return false;
    }

    while (true) {
        Address sender;

        unsigned char buffer[256];

        int bytes_read = RecievePackets(socket, sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        buffer[bytes_read] = '\0'; // Null-terminate the received data.
    
        // Assuming the data is text, you can simply print it to the console.
        //printf("Received data: %s\n", buffer);

        printf("%s\n", buffer);
        //std::cout << "Received data: " << buffer << std::endl;
        // process packet
    }
}