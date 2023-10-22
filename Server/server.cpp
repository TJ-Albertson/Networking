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

        buffer[255] = '\0'; // Null-terminate the received data.
   
        printf("buffer: %s\n", buffer);
        // process packet
    }
}