#ifndef SOCKET_H
#define SOCKET_H

#include <stdio.h>
#include <stdint.h>

#include "address.h"


typedef int SocketHandle;

/*
bool CreateSocket(Socket& socket);

void DeleteSocket(Socket& socket);

bool Open(unsigned short port);

void Close();

bool IsOpen();

bool Send(const Address& destination, const void* data, int size);

int Receive(Address& sender, void* data, int size);
*/

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

bool SendPacket(SocketHandle handle, Address destination, void* data, int size)
{
    int newBufferSize = size + sizeof(uint32_t); // 4 bytes for the 32-bit integer
    char* newDataBuffer = (char*)malloc(newBufferSize);

    if (newDataBuffer == NULL) {
        // Handle memory allocation error
        printf("Failed to allocate memory for the packet.\n");
        return false;
    }

    // Copy the 32-bit integer into the new buffer (prefix)
    uint32_t prefix = 55;
    memcpy(newDataBuffer, &prefix, sizeof(uint32_t));

    // Copy the original data after the prefix
    memcpy(newDataBuffer + sizeof(uint32_t), data, size);

    int sent_bytes = sendto(handle,
        newDataBuffer,
        newBufferSize,
        0,
        (sockaddr*)&destination.sock_address,
        sizeof(sockaddr_in));

    if (sent_bytes != newBufferSize) {
        printf("failed to send packet\n");
        return false;
    }
}

int RecievePackets(SocketHandle handle, Address& sender, const void* data, int size)
{
    // unsigned char packet_data[256];

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