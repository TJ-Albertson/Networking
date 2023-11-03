#include <iostream>
#include <map>
#include <time.h>

#include "socket.h"
#include "ui.h"

const uint32_t protocol_id = 57;

typedef struct Client {
    //unsigned int ip_addr;
    float time;
};

// ip address and time
std::map<unsigned int, time_t> clients;


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

    if (!InitializeUi()) {
        printf("Failed to initialize UI\n");
        return 0;
    }

    struct tm* timeInfo;
    time_t currentTime;
    char timeString[20];

    time(&currentTime);
    timeInfo = localtime(&currentTime);
    strftime(timeString, 20, "%H:%M:%S", timeInfo);

    console.AddLog("[%s INFO]: Server started listening on port %d\n", timeString, port);


    uint16_t sequence = 0;
    PacketData packets[PacketBufferSize];
    uint8_t buffer[MaxPacketSize];


    while (!glfwWindowShouldClose(window)) {

        time(&currentTime);
        timeInfo = localtime(&currentTime);
        strftime(timeString, 20, "%H:%M:%S", timeInfo);
        

        UiLoop();

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



        
        Stream readStream;
        InitReadStream(readStream, buffer, 256);
        
        PacketA packet;

        packet.Serialize(readStream);

        PacketB packetdr;
        packetdr.Serialize(readStream);

       
        // Increase time for all clients
        for (auto& pair : clients) {
            pair.second -= deltaTime;
        }
    
        if (sender.ipv4 <= 0 || sender.ipv4 != -858993460) {
            // Check ip address
            if (clients.find(sender.ipv4) != clients.end()) {
                // Refresh time
                clients[sender.ipv4] = currentTime;
            } else {
                char* decode_addr = DecodePrintAddress(sender.ipv4);
                console.AddLog("[%s INFO]: Client at %s connected", timeString, decode_addr);      
                clients[sender.ipv4] = currentTime;
            }

            console.AddLog("[%s INFO]: Client %d Message,", timeString, sender.ipv4);
            
            console.AddLog("  packet.x: %d", packet.x);
            console.AddLog("  packet.y: %d", packet.y);
            console.AddLog("  packet.z: %d", packet.z);
            

             console.AddLog("packetdr.numElements: %d\n", packetdr.numElements);
            console.AddLog("packetdr.elements[0]: %d\n", packetdr.elements[0]);
            console.AddLog("packetdr.elements[1]: %d\n", packetdr.elements[1]);
            console.AddLog("packetdr.elements[2]: %d\n", packetdr.elements[2]);
            console.AddLog("packetdr.elements[3]: %d\n", packetdr.elements[3]);
            console.AddLog("packetdr.elements[4]: %d\n", packetdr.elements[4]);

            /*
            printf("[Client %d] %s\n", sender.address, data);
            printf("    protocol_id: %d\n", protocol_id);
            printf("    packet_type: %d\n", packet_type);
            */
        }

        // Disconnect old clients
        for (auto it = clients.begin(); it != clients.end();) {

            if ( difftime(currentTime, it->second) > 10.0f ) {

                char* decode_addr = DecodePrintAddress(it->first);
                console.AddLog("[%s INFO]: Client at %s disconnected: TIMEOUT", timeString, decode_addr);

                it = clients.erase(it); // Advance the iterator after erasing
            } else {
                ++it; // Move to the next element
            }
        }
        
        //printf("%d buffer: %s\n", prefix, data);
        // process packet
    }

     CleanUpUi();
}