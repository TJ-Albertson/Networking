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

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ws2ipdef.h>

#elif PLATFORM == PLATFORM_MAC || PLATFORM == PLATFORM_UNIX

#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif

#if PLATFORM == PLATFORM_WINDOWS
#pragma comment(lib, "wsock32.lib")
#endif



typedef struct Address {
    uint32_t m_address_ipv4;
    uint16_t m_port;
} Address;


Address address_set()
{
    Address address;

    address.m_address_ipv4 = 0;
    address.m_port = 0;

    return address;
}

int CreateAddress(Address* c_address, unsigned char a, unsigned char b, unsigned char c, unsigned char d, unsigned short port)
{
    /*
    unsigned int address = (a << 24) | (b << 16) | (c << 8) | d;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_port = htons(port);

    */
    c_address->m_address_ipv4 = (uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24);
    //c_address.m_address_ipv4 = address;
    c_address->m_port = port;

    return 1;
}

char* DecodePrintAddress(unsigned int address) {

    unsigned char a = (address >> 24) & 0xFF;
    unsigned char b = (address >> 16) & 0xFF;
    unsigned char c = (address >> 8) & 0xFF;
    unsigned char d = address & 0xFF;

    //printf("%d.%d.%d.%d", static_cast<int>(a), static_cast<int>(b), static_cast<int>(c), static_cast<int>(d));

    char buffer[20]; // Make sure the buffer is large enough to hold your formatted string
    sprintf(buffer, "%d.%d.%d.%d", (int)(a), (int)(b), (int)(c), (int)(d));

    return buffer;
}

const char* AddressToString(Address adress, char* buffer, int bufferSize)
{
    const uint8_t a = adress.m_address_ipv4 & 0xff;
    const uint8_t b = (adress.m_address_ipv4 >> 8) & 0xff;
    const uint8_t c = (adress.m_address_ipv4 >> 16) & 0xff;
    const uint8_t d = (adress.m_address_ipv4 >> 24) & 0xff;
    if (adress.m_port != 0)
        snprintf(buffer, bufferSize, "%d.%d.%d.%d:%d", a, b, c, d, adress.m_port);
    else
        snprintf(buffer, bufferSize, "%d.%d.%d.%d", a, b, c, d);
    return buffer;
}

#endif
