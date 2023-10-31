#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdint.h>

#include "address.h"
#include "packet.h"
#include "utils.h"

typedef int SocketHandle;

bool InitializeSockets()
{
#if PLATFORM == PLATFORM_WINDOWS
    WSADATA WsaData;
    return WSAStartup(MAKEWORD(2, 2),
               &WsaData)
        == NO_ERROR;
#else
    return true;
#endif
}

void ShutdownSockets()
{
#if PLATFORM == PLATFORM_WINDOWS
    WSACleanup();
#endif
}

bool CreateSocket(SocketHandle& handle, unsigned short port)
{
    handle = socket(AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP);

    if (handle <= 0) {
        printf("failed to create socket\n");
        return false;
    }

    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(handle,
            (const sockaddr*)&address,
            sizeof(sockaddr_in))
        < 0) {
        printf("failed to bind socket\n");
        return false;
    }

#if PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX

    int nonBlocking = 1;
    if (fcntl(handle,
            F_SETFL,
            O_NONBLOCK,
            nonBlocking)
        == -1) {
        printf("failed to set non-blocking\n");
        return false;
    }

#elif PLATFORM == PLATFORM_WINDOWS

    DWORD nonBlocking = 1;
    if (ioctlsocket(handle,
            FIONBIO,
            &nonBlocking)
        != 0) {
        printf("failed to set non-blocking\n");
        return false;
    }
#endif

return true;
}

void DestroySocket(SocketHandle& socket)
{
#if PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
    close(socket);
#elif PLATFORM == PLATFORM_WINDOWS
    closesocket(socket);
#endif
}


const int MAX_PACKET_SIZE = 256;
const int HEADER_SIZE = 4;
const int PROTOCOL_ID = 57;



bool SendPacket(SocketHandle handle, Address destination, void* data, int size)
{
    unsigned char buffer[256];

    if (size > MAX_PACKET_SIZE - HEADER_SIZE) {
        printf("Data too large\n");
        return false;
    }


    uint32_t network_protocolId = host_to_network(packetInfo.protocolId);
    uint32_t crc32 = calculate_crc32((const uint8_t*)&network_protocolId, 4, 0);

    Stream writeStream;
    InitWriteStream(writeStream, buffer, 256);

    serialize_bits(writeStream, crc32, 32);

    serialize_bytes(writeStream, (uint8_t*)data, size);

    FlushBits(writeStream);


    if (bytesWritten > MaxFragmentSize) {
        int numFragments;
        PacketData fragmentPackets[MaxFragmentsPerPacket];
        SplitPacketIntoFragments(sequence, buffer, bytesWritten, numFragments, fragmentPackets);

        for (int j = 0; j < numFragments; ++j)
            //  send fragments

            SendPacket(fragmentPackets[j].data, fragmentPackets[j].size);

        //  //packetBuffer.ProcessPacket(fragmentPackets[j].data, fragmentPackets[j].size);
    } else {
        printf("sending packet %d as a regular packet\n", sequence);

        // packetBuffer.ProcessPacket(buffer, bytesWritten);
    }

    int sent_bytes = sendto(handle,
        (const char*)buffer,
        256,
        0,
        (sockaddr*)&destination.sock_address,
        sizeof(sockaddr_in));

    if (sent_bytes != 256) {
        printf("failed to send packet\n");
        return false;
    }
}

int RecievePackets(SocketHandle handle, Address& sender, const void* data, int size)
{
    unsigned int max_packet_size = size;

#if PLATFORM == PLATFORM_WINDOWS
    typedef int socklen_t;
#endif

    sockaddr_in from = sender.sock_address;
    socklen_t fromLength = sizeof(from);

    int bytes = recvfrom(handle,
        (char*)data,
        max_packet_size,
        0,
        (sockaddr*)&from,
        &fromLength);

    unsigned int from_address = ntohl(from.sin_addr.s_addr);
    unsigned int from_port = ntohs(from.sin_port);

    sender.address = from_address;

    return 1;
    // process received packet
}

#endif