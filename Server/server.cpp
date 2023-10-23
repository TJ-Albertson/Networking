#include "socket.h"
#include <iostream>

#include <map>

const uint32_t protocol_id = 57;

typedef struct Client {
    //unsigned int ip_addr;
    float time;
};

// ip address and time
std::map<unsigned int, float> clients;

int main()
{
    InitializeSockets();

    const int port = 30001;

    SocketHandle socket;

    if (!CreateSocket(socket, port)) {
        printf("[SERVER] Failed to create socket!\n");
        return false;
    }

    printf("[SERVER] Server started listening on port %d\n", port);


    uint32_t local_sequence;
    uint32_t remote_sequence;

    // on pack recieve
    local_sequence++;
    
    /*
    if ( packet.sequence > remote_sequence)
    {
        remote_sequence = packet.sequence;
    }

    void SendPacket() {
        Packet packet;

        packet.sequence = local_sequence;
        packet.ack = remote_sequence;

        uint32_t ack_bitfield = GetAcks(queue);
    }

    uint32_t GetAcks(Queue queue)
    {
        uint32_t ack_bitfield;
    
        for (int i; i < MAX_QUEUE_SIZE; i++) {
            ack_bitfield >> queue(i);
        }
    }
    
    */

    while (true) {
        Address sender;

        unsigned char buffer[256];

        int bytes_read = RecievePackets(socket, sender,
            buffer,
            sizeof(buffer));

        if (!bytes_read)
            break;

        uint32_t prefix;
        memcpy(&prefix, buffer, sizeof(uint32_t));
    
        buffer[255] = '\0'; // Null-terminate the received data.

        char* data = (char*)(buffer + sizeof(uint32_t));



        float deltaTime = 0.000001f;

        // Increase time for all clients
        for (auto& pair : clients) {
            pair.second -= deltaTime;
        }
    
        if (sender.address <= 0 || sender.address != -858993460) {
            // Check ip address
            if (clients.find(sender.address) != clients.end()) {
                // Refresh time
                clients[sender.address] = 10.0f;
            } else {
                printf("[SERVER] Client at ");
                DecodePrintAddress(sender.address);
                printf(" connected\n", sender.address);

                clients[sender.address] = 10.0f;
                //memset(buffer, 0, sizeof(buffer));
            }

             printf("[Client %d] %d %s\n", sender.address, prefix, data);
        }

        // Disconnect old clients
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->second <= 0.0f) {
                printf("[SERVER] Client at ");
                DecodePrintAddress(it->first);
                printf(" disconnected\n", it->first);

                it = clients.erase(it); // Advance the iterator after erasing
            } else {
                ++it; // Move to the next element
            }
        }
        
        //printf("%d buffer: %s\n", prefix, data);
        // process packet
    }
}