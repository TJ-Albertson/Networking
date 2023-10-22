#ifndef ADDRESS_H
#define ADDRESS_H

#define PLATFORM_WINDOWS 1
#define PLATFORM_MAC 2
#define PLATFORM_UNIX 3

#if defined(_WIN32)
#define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
#define PLATFORM PLATFORM_MAC
#else
#define PLATFORM PLATFORM_UNIX
#endif

#if PLATFORM == PLATFORM_WINDOWS

#include <winsock2.h>

#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif

#if PLATFORM == PLATFORM_WINDOWS
#pragma comment(lib, "wsock32.lib")
#endif

typedef struct Address {
    unsigned int address;
    unsigned short port;
    sockaddr_in sock_address;
} Address;


int CreateAddress(Address& c_address, unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port)
{
    unsigned int address = (a << 24) | (b << 16) | (c << 8) | d;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_port = htons(port);

    c_address.address = address;
    c_address.port = port;
    c_address.sock_address = addr;

    return 1;
}

#endif