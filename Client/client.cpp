#include "socket.h"

int main()
{
    InitializeSockets();

    const int port = 30000;

    SocketHandle socket;

    if (!CreateSocket(socket, port)) {
        printf("failed to create socket!\n");
        return false;
    }

    // send a packet
    const char data[] = "gello world!";

    Address address;

    CreateAddress(address, 127, 0, 0, 1, 30001);

    char input[100];

    while (1) { // Infinite loop
        printf("Enter a string (press Enter to send or 'q' to quit): ");

        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Handle error or EOF
            break;
        }
        
        // Check if the input is just a newline character (Enter key) and break the loop
        if (input[0] == '\n') {
            break;
        }

        // Check if the input is 'q' to quit
        if (input[0] == 'q' && input[1] == '\n') {
            break;
        }

        // You can do something with the input here, like sending it
        // For demonstration, we'll just print it.
        SendPacket(socket, address, (void*)input, sizeof(input));
    }

    DestroySocket(socket);
    ShutdownSockets();
}