#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdint.h>

#include "address.h"
#include "packet.h"
#include "utils.h"
#include "fragment.h"

typedef int SocketHandle;

typedef struct Socket {
    uint16_t m_port;
    SocketHandle m_socket;
} Socket;

bool r_sockets_initialize()
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

void r_sockets_shutdown()
{
#if PLATFORM == PLATFORM_WINDOWS
    WSACleanup();
#endif
}

bool r_socket_create(SocketHandle* handle, unsigned short port)
{
    *handle = socket(AF_INET,
        SOCK_DGRAM,
        IPPROTO_UDP);

    if (*handle <= 0) {
        printf("failed to create socket\n");
        return false;
    }

    SOCKADDR_IN address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons((unsigned short)port);

    if (bind(*handle,
            (const SOCKADDR*)&address,
            sizeof(SOCKADDR_IN))
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
    if (ioctlsocket(*handle,
            FIONBIO,
            &nonBlocking)
        != 0) {
        printf("failed to set non-blocking\n");
        return false;
    }

#endif

    return true;
}

void r_socket_destroy(SocketHandle* socket)
{
#if PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX
    close(socket);
#elif PLATFORM == PLATFORM_WINDOWS
    closesocket(*socket);
#endif
}

const int MAX_PACKET_SIZE = 256;
const int HEADER_SIZE = 4;

// SEND packets on socket TO address
bool SendPacket(Socket socket, const Address destination, const void* packetData, int size)
{
    uint8_t buffer[2000];

    SOCKADDR_IN socket_address;
    memset(&socket_address, 0, sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_addr.s_addr = destination.m_address_ipv4;
    socket_address.sin_port = htons((unsigned short)destination.m_port);
    
    if (size > MaxFragmentSize) {

        int numFragments;
        PacketData fragmentPackets[MaxFragmentsPerPacket];
        int sequence = 0;
        SplitPacketIntoFragments(sequence, (uint8_t*)packetData, size, &numFragments, fragmentPackets);

        for (int j = 0; j < numFragments; ++j) {

            int sent_bytes = sendto(socket.m_socket,
                (const char*)fragmentPackets[j].data,
                fragmentPackets[j].size,
                0,
                (SOCKADDR*)&socket_address,
                sizeof(SOCKADDR_IN));

        }
            
    } else {
         printf("sending packet as a regular packet\n");

         int remainder = size % 4;
         if (remainder != 0) {
            size = size + (4 - remainder);
         }

         int size_plus_crc = size + 4;

         Stream writeStream;
         r_stream_write_init(&writeStream, buffer, size_plus_crc);

         uint32_t crc32 = 0;
         r_serialize_bits(&writeStream, &crc32, 32);
         serialize_bytes(&writeStream, (uint8_t*)packetData, size);

         FlushBits(&writeStream);

         int align_bits = GetAlignBits(&writeStream);

         int numBytes = (writeStream.bits_processed + align_bits) / 8;

         printf("numBytes: %d\n", numBytes);

         // do crc32 after the fact to include data
         uint32_t network_protocolId = host_to_network(packetInfo.protocolId);
         crc32 = calculate_crc32((uint8_t*)&network_protocolId, 4, 0);
         crc32 = calculate_crc32(buffer, numBytes, crc32);
         *((uint32_t*)(buffer)) = host_to_network(crc32);

         int sent_bytes = sendto(socket.m_socket,
            (const char*)buffer,
            numBytes,
            0,
             (SOCKADDR*)&socket_address,
            sizeof(SOCKADDR_IN));
    }

    /*
    if (sent_bytes != 256) {
        printf("failed to send packet\n");
        return false;
    }
    */
}

// RECEIVE packets on socket FROM address
int ReceivePackets(Socket socket, Address* sender, void* packetData, int size)
{
    unsigned int max_packet_size = size;

#if PLATFORM == PLATFORM_WINDOWS
    typedef int socklen_t;
#endif

    SOCKADDR_STORAGE SOCKADDR_from;
    socklen_t fromLength = sizeof(SOCKADDR_from);
    

    int bytes = recvfrom(socket.m_socket,
        (char*)packetData,
        max_packet_size,
        0,
        (SOCKADDR*)&SOCKADDR_from,
        &fromLength);

        const SOCKADDR_IN* addr_ipv4 = (const SOCKADDR_IN*)&SOCKADDR_from;
        sender->m_address_ipv4 = addr_ipv4->sin_addr.s_addr;
        sender->m_port = ntohs(addr_ipv4->sin_port);



    return bytes;
    // process received packet
}

#endif