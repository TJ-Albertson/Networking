#include "client_server.h"

int main()
{
    InitializeSockets();

    const int port = 30000;

    SocketHandle socketHandle;

    if (!CreateSocket(socketHandle, port)) {
        printf("failed to create socket!\n");
        return false;
    }

    // send a packet
    const char data[] = "gello world!";

    Address address;

    CreateAddress(address, 127, 0, 0, 1, 30001);

    char input[100];

    Socket socket;
    socket.m_port = port;
    socket.m_socket = socketHandle;


    Client client;
    CreateClient(client, socket);

    while (1) { // Infinite loop
        printf("Enter a string (press Enter to send or 'q' to quit): ");
       

        if (fgets(input, sizeof(input), stdin) == NULL) {
            // Handle error or EOF
            break;
        }
        printf("yo\n");
        // Check if the input is just a newline character (Enter key) and break the loop
        if (input[0] == '\n') {
            break;
        }

        // Check if the input is 'q' to quit
        if (input[0] == 'q' && input[1] == '\n') {
            
            continue;
        }


        if (input[0] == 'a' && input[1] == '\n') {
            uint8_t buffer[32];

            PacketA packet;
            packet.x = 238;
            packet.y = 12;
            packet.z = 5;

            Stream writeStream;
            r_stream_write_init(writeStream, buffer, 32);

            uint32_t packetType = 1;
            serialize_int(writeStream, packetType, 0, 2);

            packet.Serialize(writeStream);

            FlushBits(writeStream);

            int align_bits = GetAlignBits(writeStream);

            int numBytes = (writeStream.m_bitsProcessed + align_bits) / 8;

            SendPacket(socket, address, (void*)buffer, numBytes);
            continue;
        }

        if (input[0] == 'b' && input[1] == '\n') {
           

            uint8_t buffer[2000];

            TestPacketB testpacket;

            testpacket.numItems = 1600;
            testpacket.randomFill();

            printf("testpacket.items[89]: %d\n", testpacket.items[89]);
            printf("testpacket.items[129]: %d\n", testpacket.items[129]);
            printf("testpacket.items[1108]: %d\n", testpacket.items[1108]);

            Stream writeStream;
            r_stream_write_init(writeStream, buffer, 2000);

            uint32_t packetType = 2;
            serialize_int(writeStream, packetType, 0, 2);

            testpacket.Serialize(writeStream);

            WriteAlign(writeStream);

            FlushBits(writeStream);

            int align_bits = GetAlignBits(writeStream);

            int numBytes = (writeStream.m_bitsProcessed + align_bits) / 8;

            SendPacket(socket, address, (void*)buffer, numBytes);
            continue;
        }

        // You can do something with the input here, like sending it
        // For demonstration, we'll just print it.
        SendPacket(socket, address, (void*)input, sizeof(input));
    }

    DestroySocket(socket.m_socket);
    ShutdownSockets();
}