#include "socket.h"
#include <iostream>

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

        uint32_t prefix;
        memcpy(&prefix, buffer, sizeof(uint32_t));
    
        buffer[255] = '\0'; // Null-terminate the received data.

        char* data = (char*)(buffer + sizeof(uint32_t));
   
        printf("buffer: %s\n", data);
        printf("prefix: %d\n", prefix);
        // process packet
    }
}