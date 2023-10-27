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


const int MaxClients = 64;

typedef struct Server {
    int m_maxClients;
    int m_numConnectedClients;
    bool m_clientConnected[MaxClients];
    Address m_clientAddress[MaxClients];
} Server;

int FindFreeClientIndex(Server server)
{
    for (int i = 0; i < server.m_maxClients; ++i) {
        if (!server.m_clientConnected[i])
            return i;
    }
    return -1;
}

int FindExistingClientIndex(Server server, Address& address)
{
    for (int i = 0; i < server.m_maxClients; ++i) {
        if (server.m_clientConnected[i] && server.m_clientAddress[i].address == address.address)
            return i;
    }
    return -1;
}


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

    /*
    uint32_t local_sequence;
    uint32_t remote_sequence;

    // on pack recieve
    local_sequence++;
    
    
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

        uint32_t protocol_id;
        memcpy(&protocol_id, buffer, 4);

        uint8_t packet_type;
        memcpy(&packet_type, buffer + 4, 1);
    
        buffer[255] = '\0'; // Null-terminate the received data.

        char* data = (char*)(buffer + 5);

        //char* string = (char*)(buffer + 4);

        float deltaTime = 0.000001f;





        BitReader reader;
        reader.scratch = 0;
        reader.scratch_bits = 0;
        reader.total_bits = 8 * sizeof(buffer);
        reader.total_bits_read = 0;
        reader.word_index = 0;
        reader.buffer = (uint32_t*)buffer;

        PacketB packet;

        packet.ReadSerialize(reader);

        PacketA packet_2;
        packet_2.ReadSerialize(reader);

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
            }

            printf("[Client %d]\n", sender.address);

            printf("numElements: %d\n", packet.numElements);
            for (int i = 0; i < packet.numElements; ++i) {
                printf("elements[%d]: %d\n", i, packet.elements[i]);
            }
            
            printf("    packet.x: %d\n", packet_2.x);
            printf("    packet.y: %d\n", packet_2.y);
            printf("    packet.z: %d\n", packet_2.z);
            
            /*
            printf("[Client %d] %s\n", sender.address, data);
            printf("    protocol_id: %d\n", protocol_id);
            printf("    packet_type: %d\n", packet_type);
            */
        }

        // Disconnect old clients
        for (auto it = clients.begin(); it != clients.end();) {
            if (it->second <= 0.0f) {
                printf("[SERVER] Client at ");
                DecodePrintAddress(it->first);
                printf(" disconnected: TIMEOUT\n", it->first);

                it = clients.erase(it); // Advance the iterator after erasing
            } else {
                ++it; // Move to the next element
            }
        }
        
        //printf("%d buffer: %s\n", prefix, data);
        // process packet
    }
}