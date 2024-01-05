#include "client_server.h"
#include <time.h>

int main()
{
    r_sockets_initialize();

    const int port = 30000;

    SocketHandle socketHandle;

    if (!r_socket_create(&socketHandle, port)) {
        printf("failed to create socket!\n");
        return false;
    }

    Address serverAddress;

    CreateAddress(&serverAddress, 127, 0, 0, 1, 30001);

    char input[100];

    Socket socket;
    socket.m_port = port;
    socket.m_socket = socketHandle;

    Client client;
    CreateClient(&client, socket);

    time_t currentTime;
    time(&currentTime);

    uint8_t buffer[MaxPacketSize];

    ClientConnect(&client, serverAddress, currentTime);

    while (1) {

        time(&currentTime);

        ClientSendPackets(&client, currentTime);

        Address sender;

        int bytes_read = ReceivePackets(socket, &sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        

        // Unique message recieve
        if (sender.m_address_ipv4 <= 0 || sender.m_address_ipv4 != -858993460) {

            if (bytes_read <= 0) {
                printf("bytes_read <= 0\n");
                continue;
            }

            printf("bytes_read: %d\n", bytes_read);

            Stream readStream;
            r_stream_read_init(&readStream, buffer, bytes_read);

            uint32_t read_crc32 = 0;
            r_serialize_bits(&readStream, &read_crc32, 32);

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


            int32_t packet_type = 0;
            r_serialize_int(&readStream, &packet_type, 0, 3);

            if (packet_type == 3) {

                int32_t client_server_type = 0;
                r_serialize_int(&readStream, &client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

                ClientReceivePackets(&client, currentTime, sender, &readStream, client_server_type);
            }

            //retir(packet_type, readStream);
        }

        ClientCheckForTimeOut(&client, currentTime);
    }

    r_socket_destroy(&socket.m_socket);
    r_sockets_shutdown();
}