
#include <iostream>
#include <map>
#include <time.h>

#include "packet_type_switch.h"
#include "client_server.h"


// ip address and time
std::map<unsigned int, time_t> clients;

int main()
{
    InitializeSockets();

    const int port = 30001;

    SocketHandle socketHandle;

    if (!CreateSocket(socketHandle, port)) {
        printf("[SERVER] Failed to create socket!\n");
        return false;
    }

    struct tm* timeInfo;
    time_t currentTime;
    char timeString[20];

    time(&currentTime);
    timeInfo = localtime(&currentTime);
    strftime(timeString, 20, "%H:%M:%S", timeInfo);

    uint16_t sequence = 0;
    PacketData packets[PacketBufferSize];
    uint8_t buffer[MaxPacketSize];

    Socket socket;
    socket.m_port = port;
    socket.m_socket = socketHandle;

    Server server;
    CreateServer(server, socket);

    while (1) {

        time(&currentTime);
        timeInfo = localtime(&currentTime);
        strftime(timeString, 20, "%H:%M:%S", timeInfo);

        ServerSendPacketsAll(server, currentTime);

        Address sender;

        int bytes_read = ReceivePackets(socket, sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        // Unique message recieve
        if (sender.m_address_ipv4 <= 0 || sender.m_address_ipv4 != -858993460) {

            printf("bytes_read: %d\n", bytes_read);

            Stream readStream;
            InitReadStream(readStream, buffer, bytes_read);

            uint32_t read_crc32 = 0;
            SerializeBits(readStream, read_crc32, 32);


            uint32_t network_protocolId = host_to_network( packetInfo.protocolId );
            uint32_t crc32 = calculate_crc32((const uint8_t*)&network_protocolId, 4, 0);
            uint32_t zero = 0;
            
            crc32 = calculate_crc32((const uint8_t*) &zero, 4, crc32);
            //important to offset buffer as to not include og crc32
            crc32 = calculate_crc32(buffer + 4, bytes_read - 4, crc32);


            if (crc32 != read_crc32) {
                printf("corrupt packet. expected crc32 %x, got %x\n", crc32, read_crc32);
                continue;
            }


            uint32_t packet_type = 0;
            serialize_int(readStream, packet_type, 0, 3);


            if (packet_type == 3) {

               
                uint32_t client_server_type = 0;
                serialize_int(readStream, client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

                ServerReceivePackets(server, currentTime, sender, readStream, client_server_type);
               // continue;
            }

            ServerCheckForTimeOut(server, currentTime);

            //packet_switch(packet_type, readStream);
        }
    }
}