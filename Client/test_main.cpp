#include "client_server.h"
#include <time.h>

int main()
{
    InitializeSockets();

    const int port = 30000;

    SocketHandle socket;

    if (!CreateSocket(socket, port)) {
        printf("failed to create socket!\n");
        return false;
    }

    Address address;

    CreateAddress(address, 127, 0, 0, 1, 30001);

    char input[100];

    Socket sock;
    sock.m_port = port;
    sock.m_socket = socket;

    Client client;
    CreateClient(client, sock);

    time_t currentTime;
    time(&currentTime);

    uint8_t buffer[MaxPacketSize];

    ClientConnect(client, address, currentTime);

    while (1) {

        time(&currentTime);

        ClientSendPackets(client, currentTime);

        Address sender;

        int bytes_read = RecievePackets(socket, sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        // Unique message recieve
        if (sender.ipv4 <= 0 || sender.ipv4 != -858993460) {

            

           // printf("bytes_read: %d\n", bytes_read);

            Stream readStream;
            InitReadStream(readStream, buffer, bytes_read);

            uint32_t read_crc32 = 0;
            SerializeBits(readStream, read_crc32, 32);

            uint32_t network_protocolId = host_to_network(packetInfo.protocolId);
            uint32_t crc32 = calculate_crc32((const uint8_t*)&network_protocolId, 4, 0);
            uint32_t zero = 0;

            crc32 = calculate_crc32((const uint8_t*)&zero, 4, crc32);
            // important to offset buffer as to not include og crc32
            crc32 = calculate_crc32(buffer + 4, bytes_read - 4, crc32);

            if (crc32 != read_crc32) {
                printf("corrupt packet. expected crc32 %x, got %x\n", crc32, read_crc32);
                continue;
            }

            uint32_t packet_type = 0;
            serialize_bits(readStream, packet_type, 2);

            if (packet_type == 3) {

                uint32_t client_server_type = 0;
                serialize_int(readStream, client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

                ClientReceivePackets(client, currentTime, sender, readStream, client_server_type);
            }

            ClientCheckForTimeOut(client, currentTime);

            //packet_switch(packet_type, readStream);
        }
    }

    DestroySocket(socket);
    ShutdownSockets();
}