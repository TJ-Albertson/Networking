#pragma once

#include <iostream>
#include <map>
#include <time.h>

#include "packet_type_switch.h"
#include "socket.h"

typedef struct Client {
    // unsigned int ip_addr;
    float time;
};

// ip address and time
std::map<unsigned int, time_t> clients;

const int MaxClients = 64;

int main()
{
    InitializeSockets();

    const int port = 30001;

    SocketHandle socket;

    if (!CreateSocket(socket, port)) {
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

    while (1) {

        time(&currentTime);
        timeInfo = localtime(&currentTime);
        strftime(timeString, 20, "%H:%M:%S", timeInfo);

        Address sender;

        int bytes_read = RecievePackets(socket, sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        // Unique message recieve
        if (sender.address <= 0 || sender.address != -858993460) {

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
            serialize_bits(readStream, packet_type, 2);

            packet_switch(packet_type, readStream);
        }
    }
}