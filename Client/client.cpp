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
            
            continue;
        }


        if (input[0] == 'a' && input[1] == '\n') {
            uint8_t buffer[30];

            PacketA packet;
            packet.x = 238;
            packet.y = 12;
            packet.z = 5;

            Stream writeStream;
            InitWriteStream(writeStream, buffer, 256);

            uint32_t packetType = 1;
            serialize_int(writeStream, packetType, 0, 2);

            packet.Serialize(writeStream);

            FlushBits(writeStream);

            SendPacket(socket, address, (void*)buffer, 30);
            continue;
        }

        if (input[0] == 'b' && input[1] == '\n') {
           // SendPacketB();
        }

        // You can do something with the input here, like sending it
        // For demonstration, we'll just print it.
        SendPacket(socket, address, (void*)input, sizeof(input));
    }

    DestroySocket(socket);
    ShutdownSockets();
}