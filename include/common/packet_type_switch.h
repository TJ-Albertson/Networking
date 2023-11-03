#ifndef PACKET_TYPE_SWITCH_H
#define PACKET_TYPE_SWITCH_H


#include <stdio.h>

#include "serializer.h"
#include "fragment.h"


void packet_switch(int packet_type, Stream& readStream) {

	switch (packet_type) {
    case 0:
        printf("Fragmented packet recieved.\n");
        ProcessFragmentPacket(readStream);
        break;
    case 1:
        printf("Packet Type A recieved\n");
        
        PacketA packet;

        packet.Serialize(readStream);

        printf("packet.x: %d\n", packet.x);
        printf("packet.y: %d\n", packet.y);
        printf("packet.z: %d\n", packet.z);

        break;
    case 2:
        printf("TestPacketB recieved\n");
        
        TestPacketB testpacket;

        testpacket.Serialize(readStream);

        printf("testpacket.numItems %d\v", testpacket.numItems);

        printf("testpacket.items[89]: %d\n", testpacket.items[89]);
        printf("testpacket.items[129]: %d\n", testpacket.items[129]);
        printf("testpacket.items[1108]: %d\n", testpacket.items[1108]);

        break;
    case 3:
        printf("You selected 3.\n");
        break;
    default:
        printf("Invalid packet type.\n");
    }
}

#endif // !PACKET_TYPE_SWITCH_H
