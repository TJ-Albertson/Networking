#ifndef PACKET_TYPE_SWITCH_H
#define PACKET_TYPE_SWITCH_H


#include <stdio.h>

#include "serializer.h"
#include "fragment.h"


void packet_switch(int packet_type, uint8_t* buffer) {

	switch (packet_type) {
    case 0:
        printf("You selected 1.\n");
        ProcessFragmentPacket(buffer);
        break;
    case 1:
        printf("You selected 1.\n");
        ProcessPacketA(buffer);
        break;
    case 2:
        printf("You selected 2.\n");
        break;
    case 3:
        printf("You selected 3.\n");
        break;
    default:
        printf("Invalid packet type.\n");
    }
}

#endif // !PACKET_TYPE_SWITCH_H
