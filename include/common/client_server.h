#ifndef CLIENT_SERVER_H
#define CLIENT_SERVER_H

#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <inttypes.h>

#include "serializer.h"
#include "address.h"
#include "hash.h"
#include "socket.h"

//const uint32_t ProtocolId = 0x12341651;
//const int MaxPacketSize = 1200;
#define MaxClients 32
#define ServerPort 50000
#define ClientPort 60000

#define ChallengeHashSize 1024
#define ChallengeSendRate 0.1
#define ChallengeTimeOut 10.0

#define ConnectionRequestSendRate 0.1
#define ConnectionChallengeSendRate 0.1
#define ConnectionResponseSendRate 0.1
#define ConnectionConfirmSendRate 0.1
#define ConnectionKeepAliveSendRate 1.0

#define ConnectionRequestTimeOut 5.0
#define ChallengeResponseTimeOut 5.0
#define KeepAliveTimeOut 10.0
#define ClientSaltTimeout 1.0

uint64_t GenerateSalt()
{
    return (((uint64_t)(rand()) << 0) & 0x000000000000FFFFull) | (((uint64_t)(rand()) << 16) & 0x00000000FFFF0000ull) | (((uint64_t)(rand()) << 32) & 0x0000FFFF00000000ull) | (((uint64_t)(rand()) << 48) & 0xFFFF000000000000ull);
}

typedef enum {
    PACKET_CONNECTION_REQUEST,      // client requests a connection.
    PACKET_CONNECTION_DENIED,       // server denies client connection request.
    PACKET_CONNECTION_CHALLENGE,    // server response to client connection request.
    PACKET_CONNECTION_RESPONSE,     // client response to server connection challenge.
    PACKET_CONNECTION_KEEP_ALIVE,   // keep alive packet sent at some low rate (once per-second) to keep the connection alive
    PACKET_CONNECTION_DISCONNECT,   // courtesy packet to indicate that the other side has disconnected. better than a timeout
    CLIENT_SERVER_NUM_PACKETS
} PacketTypes;

void printPackType(PacketTypes type)
{
    switch (type) {
    case PACKET_CONNECTION_REQUEST:
        printf("PACKET_CONNECTION_REQUEST\n");
        break;
    case PACKET_CONNECTION_DENIED:
        printf("PACKET_CONNECTION_DENIED\n");
        break;
    case PACKET_CONNECTION_CHALLENGE:
        printf("PACKET_CONNECTION_CHALLENGE\n");
        break;
    case PACKET_CONNECTION_RESPONSE:
        printf("PACKET_CONNECTION_RESPONSE\n");
        break;
    case PACKET_CONNECTION_KEEP_ALIVE:
        printf("PACKET_CONNECTION_KEEP_ALIVE\n");
        break;
    case PACKET_CONNECTION_DISCONNECT:
        printf("PACKET_CONNECTION_DISCONNECT\n");
        break;
    case CLIENT_SERVER_NUM_PACKETS:
        printf("CLIENT_SERVER_NUM_PACKETS\n");
        break;
    default:
        printf("Unknown PacketType\n");
    }
}


/*
* CLIENT/SERVER PACKETS
*/
typedef struct ConnectionRequestPacket {
    int packet_type;// = 3;
    int client_server_type;// = 0;

    uint64_t client_salt;
    uint8_t data[256];
} ConnectionRequestPacket;

bool SerializeConnectionRequestPacket(Stream* stream, ConnectionRequestPacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    if (stream->type == READ && GetBitsRemaining(stream) < 256 * 8)
        return false;
    serialize_bytes(stream, packet->data, 256);
    return true;
}





typedef enum  {
    CONNECTION_DENIED_SERVER_FULL,
    CONNECTION_DENIED_ALREADY_CONNECTED,
    CONNECTION_DENIED_NUM_VALUES
} ConnectionDeniedReason;

typedef struct ConnectionDeniedPacket {
    int packet_type; // = 3;
    int client_server_type; // = 1;

    uint64_t client_salt;
    ConnectionDeniedReason reason;
} ConnectionDeniedPacket;

bool SerializeConnectionDeniedPacket(Stream* stream, ConnectionDeniedPacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    serialize_enum(stream, packet->reason, ConnectionDeniedReason, CONNECTION_DENIED_NUM_VALUES);
    return true;
}


typedef struct ConnectionChallengePacket {
    int packet_type; // = 3;
    int client_server_type; // = 2;

    uint64_t client_salt;
    uint64_t challenge_salt;
} ConnectionChallengePacket;

bool SerializeConnectionChallengePacket(Stream* stream, ConnectionChallengePacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    serialize_uint64(stream, &packet->challenge_salt);
    return true;
}

typedef struct ConnectionResponsePacket {
    int packet_type; // = 3;
    int client_server_type; // = 3;

    uint64_t client_salt;
    uint64_t challenge_salt;
} ConnectionResponsePacket;

bool SerializeConnectionResponsePacket(Stream* stream, ConnectionResponsePacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    serialize_uint64(stream, &packet->challenge_salt);
    return true;
}

typedef struct ConnectionKeepAlivePacket {
    int packet_type;// = 3;
    int client_server_type; // = 4;

    uint64_t client_salt;
    uint64_t challenge_salt;
} ConnectionKeepAlivePacket;


bool SerializeConnectionKeepAlivePacket(Stream* stream, ConnectionKeepAlivePacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    serialize_uint64(stream, &packet->challenge_salt);
    return true;
}



typedef struct ConnectionDisconnectPacket {

    int packet_type; // = 3;
    int client_server_type; // = 5;

    uint64_t client_salt;
    uint64_t challenge_salt;
} ConnectionDisconnectPacket;

bool SerializeConnectionDisconnectPacket(Stream* stream, ConnectionDisconnectPacket* packet)
{
    r_serialize_int(stream, &packet->packet_type, 0, 3);
    r_serialize_int(stream, &packet->client_server_type, 0, CLIENT_SERVER_NUM_PACKETS);

    serialize_uint64(stream, &packet->client_salt);
    serialize_uint64(stream, &packet->challenge_salt);
    return true;
}








typedef struct {
    uint64_t client_salt; // random number generated by client and sent to server in connection request
    uint64_t challenge_salt; // random number generated by server and sent back to client in challenge packet
    double create_time; // time this challenge entry was created. used for challenge timeout
    double last_packet_send_time; // the last time we sent a challenge packet to this client
    Address address; // address the connection request came from
} ServerChallengeEntry;

typedef struct ServerChallengeHash {
    int num_entries;
    uint8_t exists[ChallengeHashSize];
    ServerChallengeEntry entries[ChallengeHashSize];
} ServerChallengeHash;

uint64_t CalculateChallengeHashKey(Address address, uint64_t clientSalt, uint64_t serverSeed)
{
    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    const int addressLength = (int)strlen(addressString);
    return murmur_hash_64(&serverSeed, 8, murmur_hash_64(&clientSalt, 8, murmur_hash_64(addressString, addressLength, 0)));
}

typedef struct ServerClientData {
    Address address;
    uint64_t clientSalt;
    uint64_t challengeSalt;
    double connectTime;
    double lastPacketSendTime;
    double lastPacketReceiveTime;
} ServerClientData;

ServerClientData SetServerClientData()
{
    ServerClientData server_client_data;

    server_client_data.clientSalt = 0;
    server_client_data.challengeSalt = 0;
    server_client_data.connectTime = 0.0;
    server_client_data.lastPacketSendTime = 0.0;
    server_client_data.lastPacketReceiveTime = 0.0;

    return server_client_data;
}

typedef struct Server {
    Socket* m_socket; // socket for sending and receiving packets.

    uint64_t m_serverSalt; // server salt. randomizes hash keys to eliminate challenge/response hash worst case attack.

    int m_numConnectedClients; // number of connected clients

    bool m_clientConnected[MaxClients]; // true if client n is connected

    uint64_t m_clientSalt[MaxClients]; // array of client salt values per-client

    uint64_t m_challengeSalt[MaxClients]; // array of challenge salt values per-client

    Address m_clientAddress[MaxClients]; // array of client address values per-client

    ServerClientData m_clientData[MaxClients]; // heavier weight data per-client, eg. not for fast lookup

    ServerChallengeHash m_challengeHash;
} Server;


bool CreateServer(Server* server, Socket* socket);
bool CleanServer(Server* server);
void ServerSendPacketsAll(Server* server, double time);
void ServerReceivePackets(Server* server, double time);
void ServerCheckForTimeOut(Server* server, double time);
void ServerResetClientState(Server* server, int clientIndex);
int ServerFindFreeClientIndex(Server* server);
int ServerFindExistingClientIndex(Server* server, Address address, uint64_t clientSalt, uint64_t challengeSalt);
void ServerConnectClient(Server* server, int clientIndex, Address address, uint64_t clientSalt, uint64_t challengeSalt, double time);
void ServerDisconnectClient(Server* server, int clientIndex, double time);
bool ServerClientIsConnected(Server* server, Address address, uint64_t clientSalt);
ServerChallengeEntry* ServerFindChallenge(Server* server, Address address, uint64_t clientSalt, double time);
ServerChallengeEntry* ServerFindOrInsertChallenge(Server* server, Address address, uint64_t clientSalt, double time);

void ServerSendPacketToConnectedClient(Server* server, int clientIndex, void* packet, int packetSize, double time);

void ServerProcessConnectionRequest(Server* server, const ConnectionRequestPacket* packet, Address address, double time);
void ServerProcessConnectionResponse(Server* server, const ConnectionResponsePacket* packet, Address address, double time);
void ServerProcessConnectionKeepAlive(Server* server, const ConnectionKeepAlivePacket* packet, Address address, double time);
void ServerProcessConnectionDisconnect(Server* server, const ConnectionDisconnectPacket* packet, Address address, double time);

bool CreateServer(Server* server, Socket* socket)
{
    server->m_socket = socket;
    server->m_serverSalt = GenerateSalt();
    server->m_numConnectedClients = 0;
    for (int i = 0; i < MaxClients; ++i)
        ServerResetClientState(server, i);

    return true;
}

bool CleanServer(Server* server)
{
    assert(server->m_socket);
    server->m_socket = NULL;

    return true;
}

void ServerSendPacketsAll(Server* server, double time)
{
    for (int i = 0; i < MaxClients; ++i) {
        if (!server->m_clientConnected[i])
            continue;
         
        if (server->m_clientData[i].lastPacketSendTime + ConnectionKeepAliveSendRate > time)
            return;

        ConnectionKeepAlivePacket connectionKeepAlivePacket;
        connectionKeepAlivePacket.packet_type = 3;
        connectionKeepAlivePacket.client_server_type = 4;

        connectionKeepAlivePacket.client_salt = server->m_clientSalt[i];
        connectionKeepAlivePacket.challenge_salt = server->m_challengeSalt[i];

        uint8_t packet_buffer[1024];

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionKeepAlivePacket(&writeStream, &connectionKeepAlivePacket);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        ServerSendPacketToConnectedClient(server, i, packet_buffer, bytesProcessed, time);
    }
}


bool ServerReceivePackets(Server* server, double time, Address address, Stream* stream, int client_packet_type)
{
    printf("Server recieved packet type ");
    printPackType((PacketTypes)client_packet_type);

    switch (client_packet_type) {

    case PACKET_CONNECTION_REQUEST: {

        printf("PACKET_CONNECTION_REQUEST\n");

        ConnectionRequestPacket packet;
        serialize_uint64(stream, &packet.client_salt);
        // serialize_bytes(stream, packet.data, 256);

        ServerProcessConnectionRequest(server, &packet, address, time);
        
    } break;

    case PACKET_CONNECTION_RESPONSE: {
        ConnectionResponsePacket ConnectionResponsePacket;
        serialize_uint64(stream, &ConnectionResponsePacket.client_salt);
        serialize_uint64(stream, &ConnectionResponsePacket.challenge_salt);

        ServerProcessConnectionResponse(server, &ConnectionResponsePacket, address, time);
    }  break;

    case PACKET_CONNECTION_KEEP_ALIVE: {
        ConnectionKeepAlivePacket connectionKeepAlivePacket;
        serialize_uint64(stream, &connectionKeepAlivePacket.client_salt);
        serialize_uint64(stream, &connectionKeepAlivePacket.challenge_salt);

        ServerProcessConnectionKeepAlive(server, &connectionKeepAlivePacket, address, time);
    }
    break;

    case PACKET_CONNECTION_DISCONNECT: {
        ConnectionDisconnectPacket connectionDisconnectPacket;
        serialize_uint64(stream, &connectionDisconnectPacket.client_salt);
        serialize_uint64(stream, &connectionDisconnectPacket.challenge_salt);

        ServerProcessConnectionDisconnect(server, &connectionDisconnectPacket, address, time);
    }
    break;

    default:
        break;
    }

    return true;
}

void ServerCheckForTimeOut(Server* server, double time)
{
    for (int i = 0; i < MaxClients; ++i) {
        if (!server->m_clientConnected[i])
            continue;

        if (server->m_clientData[i].lastPacketReceiveTime + KeepAliveTimeOut < time) {
            char buffer[256];
            const char* addressString = AddressToString(server->m_clientAddress[i], buffer, sizeof(buffer));
            printf("client %d timed out (client address = %s, client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", i, addressString, server->m_clientSalt[i], server->m_challengeSalt[i]);
            ServerDisconnectClient(server, i, time);
        }
    }
}

void ServerResetClientState(Server* server, int clientIndex)
{
    assert(clientIndex >= 0);
    assert(clientIndex < MaxClients);
    server->m_clientConnected[clientIndex] = false;
    server->m_clientSalt[clientIndex] = 0;
    server->m_challengeSalt[clientIndex] = 0;
    server->m_clientAddress[clientIndex] = address_set();
    server->m_clientData[clientIndex] = SetServerClientData();
}

int ServerFindFreeClientIndex(Server* server)
{
    for (int i = 0; i < MaxClients; ++i) {
        if (!server->m_clientConnected[i])
            return i;
    }
    return -1;
}

int ServerFindExistingClientIndex(Server* server, Address address, uint64_t clientSalt, uint64_t challengeSalt)
{
    for (int i = 0; i < MaxClients; ++i) {
        if (server->m_clientConnected[i] && server->m_clientAddress[i].m_address_ipv4 == address.m_address_ipv4 && server->m_clientSalt[i] == clientSalt && server->m_challengeSalt[i] == challengeSalt)
            return i;
    }
    return -1;
}

void ServerConnectClient(Server* server, int clientIndex, Address address, uint64_t clientSalt, uint64_t challengeSalt, double time)
{
    assert(server->m_numConnectedClients >= 0);
    assert(server->m_numConnectedClients < MaxClients - 1);
    assert(!server->m_clientConnected[clientIndex]);

    server->m_numConnectedClients++;

    server->m_clientConnected[clientIndex] = true;
    server->m_clientSalt[clientIndex] = clientSalt;
    server->m_challengeSalt[clientIndex] = challengeSalt;
    server->m_clientAddress[clientIndex] = address;

    server->m_clientData[clientIndex].address = address;
    server->m_clientData[clientIndex].clientSalt = clientSalt;
    server->m_clientData[clientIndex].challengeSalt = challengeSalt;
    server->m_clientData[clientIndex].connectTime = time;
    server->m_clientData[clientIndex].lastPacketSendTime = time;
    server->m_clientData[clientIndex].lastPacketReceiveTime = time;

    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    printf("client %d connected (client address = %s, client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", clientIndex, addressString, clientSalt, challengeSalt);

    ConnectionKeepAlivePacket connectionKeepAlivePacket;
    connectionKeepAlivePacket.packet_type = 3;
    connectionKeepAlivePacket.client_server_type = 4;

    connectionKeepAlivePacket.client_salt = server->m_clientSalt[clientIndex];
    connectionKeepAlivePacket.challenge_salt = server->m_challengeSalt[clientIndex];

    uint8_t packet_buffer[1024];

    Stream writeStream;
    r_stream_write_init(&writeStream, packet_buffer, 1024);

    SerializeConnectionKeepAlivePacket(&writeStream, &connectionKeepAlivePacket);

    FlushBits(&writeStream);

    size_t bytesProcessed = GetBytesProcessed(&writeStream);

    ServerSendPacketToConnectedClient(server, clientIndex, packet_buffer, bytesProcessed, time);
}

void ServerDisconnectClient(Server* server, int clientIndex, double time)
{
    assert(clientIndex >= 0);
    assert(clientIndex < MaxClients);
    assert(server->m_numConnectedClients > 0);
    assert(server->m_clientConnected[clientIndex]);

    char buffer[256];
    const char* addressString = AddressToString(server->m_clientAddress[clientIndex], buffer, sizeof(buffer));
    printf("client %d disconnected: (client address = %s, client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", clientIndex, addressString, server->m_clientSalt[clientIndex], server->m_challengeSalt[clientIndex]);

    ConnectionDisconnectPacket connectionDisconnectPacket;
    connectionDisconnectPacket.packet_type = 3;
    connectionDisconnectPacket.client_server_type = 5;

    connectionDisconnectPacket.client_salt = server->m_clientSalt[clientIndex];
    connectionDisconnectPacket.challenge_salt = server->m_challengeSalt[clientIndex];

    uint8_t packet_buffer[1024];

    Stream writeStream;
    r_stream_write_init(&writeStream, packet_buffer, 1024);

    SerializeConnectionDisconnectPacket(&writeStream, &connectionDisconnectPacket);

    FlushBits(&writeStream);

    size_t bytesProcessed = GetBytesProcessed(&writeStream);

    ServerSendPacketToConnectedClient(server, clientIndex, packet_buffer, bytesProcessed, time);

    ServerResetClientState(server, clientIndex);

    server->m_numConnectedClients--;
}

bool ServerClientIsConnected(Server* server, Address address, uint64_t clientSalt)
{
    for (int i = 0; i < MaxClients; ++i) {
        if (!server->m_clientConnected[i])
            continue;
        if (server->m_clientAddress[i].m_address_ipv4 == address.m_address_ipv4 && server->m_clientSalt[i] == clientSalt)
            return true;
    }
    return false;
}

ServerChallengeEntry* ServerFindChallenge(Server* server, Address address, uint64_t clientSalt, double time)
{
    const uint64_t key = CalculateChallengeHashKey(address, clientSalt, server->m_serverSalt);

    int index = key % ChallengeHashSize;

    printf("client salt = %" PRIx64 "\n", clientSalt);
    printf("challenge hash key = %" PRIx64 "\n", key);
    printf("challenge hash index = %d\n", index);

    if (server->m_challengeHash.exists[index] && server->m_challengeHash.entries[index].client_salt == clientSalt && server->m_challengeHash.entries[index].address.m_address_ipv4 == address.m_address_ipv4 && server->m_challengeHash.entries[index].create_time + ChallengeTimeOut >= time) {
        printf("found challenge entry at index %d\n", index);

        return &server->m_challengeHash.entries[index];
    }

    return NULL;
}

ServerChallengeEntry* ServerFindOrInsertChallenge(Server* server, Address address, uint64_t clientSalt, double time)
{
    const uint64_t key = CalculateChallengeHashKey(address, clientSalt, server->m_serverSalt);

    int index = key % ChallengeHashSize;

    printf("client salt = %" PRIx64 "\n", clientSalt);
    printf("challenge hash key = %" PRIx64 "\n", key);
    printf("challenge hash index = %d\n", index);

    if (!server->m_challengeHash.exists[index] || (server->m_challengeHash.exists[index] && server->m_challengeHash.entries[index].create_time + ChallengeTimeOut < time)) {
        printf("found empty entry in challenge hash at index %d\n", index);

        ServerChallengeEntry* entry = &server->m_challengeHash.entries[index];

        entry->client_salt = clientSalt;
        entry->challenge_salt = GenerateSalt();
        entry->last_packet_send_time = time - ChallengeSendRate * 2;
        entry->create_time = time;
        entry->address = address;

        server->m_challengeHash.exists[index] = 1;

        return entry;
    }

    if (server->m_challengeHash.exists[index] && server->m_challengeHash.entries[index].client_salt == clientSalt && server->m_challengeHash.entries[index].address.m_address_ipv4 == address.m_address_ipv4) {
        printf("found existing challenge hash entry at index %d\n", index);

        return &server->m_challengeHash.entries[index];
    }

    return NULL;
}


 void ServerSendPacketToConnectedClient(Server* server, int clientIndex, void* packet, int packetSize, double time)
{
    assert(packet);
    assert(clientIndex >= 0);
    assert(clientIndex < MaxClients);
    assert(server->m_clientConnected[clientIndex]);
    server->m_clientData[clientIndex].lastPacketSendTime = time;

    SendPacket(*server->m_socket, server->m_clientAddress[clientIndex], packet, packetSize);
}

void ServerProcessConnectionRequest(Server* server, const ConnectionRequestPacket* packet, Address address, double time)
{
    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    printf("processing connection request packet from: %s\n", addressString);

    if (server->m_numConnectedClients == MaxClients) {

        printf("connection denied: server is full\n");
        ConnectionDeniedPacket connectionDeniedPacket;
        connectionDeniedPacket.packet_type = 3;
        connectionDeniedPacket.client_server_type = 1;


        connectionDeniedPacket.client_salt = packet->client_salt;
        connectionDeniedPacket.reason = CONNECTION_DENIED_SERVER_FULL;


        uint8_t packet_buffer[1024];

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionDeniedPacket(&writeStream, &connectionDeniedPacket);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        SendPacket(*server->m_socket, address, packet_buffer, bytesProcessed);
        return;
    }

    if (ServerClientIsConnected(server, address, packet->client_salt)) {

        printf("connection denied: already connected\n");
        ConnectionDeniedPacket connectionDeniedPacket;
        connectionDeniedPacket.packet_type = 3;
        connectionDeniedPacket.client_server_type = 1;

        connectionDeniedPacket.client_salt = packet->client_salt;
        connectionDeniedPacket.reason = CONNECTION_DENIED_ALREADY_CONNECTED;

        uint8_t packet_buffer[1024];

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionDeniedPacket(&writeStream, &connectionDeniedPacket);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        SendPacket(*server->m_socket, address, packet_buffer, bytesProcessed);

        return;
    }

    ServerChallengeEntry* entry = ServerFindOrInsertChallenge(server, address, packet->client_salt, time);
    if (!entry)
        return;

    assert(entry);
    assert(entry->address.m_address_ipv4 == address.m_address_ipv4);
    assert(entry->client_salt == packet->client_salt);

    if (entry->last_packet_send_time + ChallengeSendRate < time) {
        printf("sending connection challenge to %s (challenge salt = %" PRIx64 ")\n", addressString, entry->challenge_salt);

        uint8_t buff[1024];

        ConnectionChallengePacket connectionChallengePacket;
        connectionChallengePacket.packet_type = 3;
        connectionChallengePacket.client_server_type = 2;

        connectionChallengePacket.client_salt = packet->client_salt;
        connectionChallengePacket.challenge_salt = entry->challenge_salt;


        Stream writeStream;
        r_stream_write_init(&writeStream, buff, 1024);

        SerializeConnectionChallengePacket(&writeStream, &connectionChallengePacket);

        FlushBits(&writeStream);


        Stream readStrem;
        r_stream_read_init(&readStrem, buff, 1024);

        ConnectionChallengePacket pack;

        SerializeConnectionChallengePacket(&readStrem, &pack);

        printf("pack.client_packet_type: %d\n", pack.client_server_type);



        size_t bytesProcessed = GetBytesProcessed(&writeStream);


        SendPacket(*server->m_socket, address, buff, bytesProcessed);
        entry->last_packet_send_time = time;
    }
}

 void ServerProcessConnectionResponse(Server* server, const ConnectionResponsePacket* packet, Address address, double time)
{
    const int existingClientIndex = ServerFindExistingClientIndex(server, address, packet->client_salt, packet->challenge_salt);
    if (existingClientIndex != -1) {
        assert(existingClientIndex >= 0);
        assert(existingClientIndex < MaxClients);

        if (server->m_clientData[existingClientIndex].lastPacketSendTime + ConnectionConfirmSendRate < time) {
            ConnectionKeepAlivePacket connectionKeepAlivePacket;
            connectionKeepAlivePacket.packet_type = 3;
            connectionKeepAlivePacket.client_server_type = 4;

            connectionKeepAlivePacket.client_salt = server->m_clientSalt[existingClientIndex];
            connectionKeepAlivePacket.challenge_salt = server->m_challengeSalt[existingClientIndex];

            uint8_t packet_buffer[1024];

            Stream writeStream;
            r_stream_write_init(&writeStream, packet_buffer, 1024);

            SerializeConnectionKeepAlivePacket(&writeStream, &connectionKeepAlivePacket);

            FlushBits(&writeStream);

            size_t bytesProcessed = GetBytesProcessed(&writeStream);

            ServerSendPacketToConnectedClient(server, existingClientIndex, packet_buffer, bytesProcessed, time);
        }

        return;
    }

    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    printf("processing connection response from client %s (client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", addressString, packet->client_salt, packet->challenge_salt);

    ServerChallengeEntry* entry = ServerFindChallenge(server, address, packet->client_salt, time);
    if (!entry)
        return;

    assert(entry);
    assert(entry->address.m_address_ipv4 == address.m_address_ipv4);
    assert(entry->client_salt == packet->client_salt);

    if (entry->challenge_salt != packet->challenge_salt) {
        printf("connection challenge mismatch: expected %" PRIx64 ", got %" PRIx64 "\n", entry->challenge_salt, packet->challenge_salt);
        return;
    }

    if (server->m_numConnectedClients == MaxClients) {
        if (entry->last_packet_send_time + ConnectionChallengeSendRate < time) {
            printf("connection denied: server is full\n");
            ConnectionDeniedPacket connectionDeniedPacket;
            connectionDeniedPacket.client_salt = packet->client_salt;
            connectionDeniedPacket.reason = CONNECTION_DENIED_SERVER_FULL;


            uint8_t packet_buffer[1024];

            Stream writeStream;
            r_stream_write_init(&writeStream, packet_buffer, 1024);

            SerializeConnectionDeniedPacket(&writeStream, &connectionDeniedPacket);

            FlushBits(&writeStream);

            size_t bytesProcessed = GetBytesProcessed(&writeStream);

            SendPacket(*server->m_socket, address, packet_buffer, bytesProcessed);
            entry->last_packet_send_time = time;
        }
        return;
    }

    const int clientIndex = ServerFindFreeClientIndex(server);

    assert(clientIndex != -1);
    if (clientIndex == -1)
        return;

    ServerConnectClient(server, clientIndex, address, packet->client_salt, packet->challenge_salt, time);
}

 void ServerProcessConnectionKeepAlive(Server* server, const ConnectionKeepAlivePacket* packet, Address address, double time)
{
    const int clientIndex = ServerFindExistingClientIndex(server, address, packet->client_salt, packet->challenge_salt);
    if (clientIndex == -1)
        return;

    assert(clientIndex >= 0);
    assert(clientIndex < MaxClients);

    server->m_clientData[clientIndex].lastPacketReceiveTime = time;
}

void ServerProcessConnectionDisconnect(Server* server, const ConnectionDisconnectPacket* packet, Address address, double time)
{
    const int clientIndex = ServerFindExistingClientIndex(server, address, packet->client_salt, packet->challenge_salt);
    if (clientIndex == -1)
        return;

    assert(clientIndex >= 0);
    assert(clientIndex < MaxClients);

    ServerDisconnectClient(server, clientIndex, time);
}




typedef enum ClientState {
    CLIENT_STATE_DISCONNECTED,
    CLIENT_STATE_SENDING_CONNECTION_REQUEST,
    CLIENT_STATE_SENDING_CHALLENGE_RESPONSE,
    CLIENT_STATE_CONNECTED,
    CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT,
    CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT,
    CLIENT_STATE_KEEP_ALIVE_TIMED_OUT,
    CLIENT_STATE_CONNECTION_DENIED_FULL,
    CLIENT_STATE_CONNECTION_DENIED_ALREADY_CONNECTED
} ClientState;

typedef struct Client {
    Socket* m_socket; // socket for sending and receiving packets.

    ClientState m_clientState; // current client state

    Address m_serverAddress; // server address we are connecting or connected to.

    uint64_t m_clientSalt; // client salt. randomly generated on each call to connect.

    uint64_t m_challengeSalt; // challenge salt sent back from server in connection challenge.

    double m_lastPacketSendTime; // time we last sent a packet to the server->

    double m_lastPacketReceiveTime; // time we last received a packet from the server (used for timeouts).

    double m_clientSaltExpiryTime; // time the client salt expires and we roll another (in case of collision).
} Client;

bool CreateClient(Client* client, Socket* socket);
bool CleanClient(Client* client);
void ClientConnect(Client* client, Address address, double time);
bool ClientIsConnecting(Client* client);
bool ClientIsConnected(Client* client);
bool ClientConnectionFailed(Client* client);
void ClientDisconnect(Client* client, double time);
void ClientSendPackets(Client* client, double time);
void ClientReceivePackets(Client* client, double time);
void ClientCheckForTimeOut(Client* client, double time);
void ClientResetConnectionData(Client* client);
void ClientSendPacketToServer(Client* client, void* packet, int packetSize, double time);
void ClientProcessConnectionDenied(Client* client, const ConnectionDeniedPacket* packet, Address address, double time);
void ClientProcessConnectionChallenge(Client* client, const ConnectionChallengePacket* packet, Address address, double time);
void ClientProcessConnectionKeepAlive(Client* client, const ConnectionKeepAlivePacket* packet, Address address, double time);
void ClientProcessConnectionDisconnect(Client* client, const ConnectionDisconnectPacket* packet, Address address, double time);


bool CreateClient(Client* client, Socket* socket)
{
    client->m_socket = socket;
    ClientResetConnectionData(client);
    return true;
}

bool CleanClient(Client* client)
{
    assert(client->m_socket);
    client->m_socket = NULL;
    return true;
}

void ClientConnect(Client* client, Address address, double time)
{
    ClientDisconnect(client,time);
    client->m_clientSalt = GenerateSalt();
    client->m_challengeSalt = 0;
    client->m_serverAddress = address;
    client->m_clientState = CLIENT_STATE_SENDING_CONNECTION_REQUEST;
    client->m_lastPacketSendTime = time - 1.0f;
    client->m_lastPacketReceiveTime = time;
    client->m_clientSaltExpiryTime = time + ClientSaltTimeout;
}

bool ClientIsConnecting(Client* client)
{
    return client->m_clientState == CLIENT_STATE_SENDING_CONNECTION_REQUEST || client->m_clientState == CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;
}

bool ClientIsConnected(Client* client)
{
    return client->m_clientState == CLIENT_STATE_CONNECTED;
}

bool ClientConnectionFailed(Client* client)
{
    return client->m_clientState > CLIENT_STATE_CONNECTED;
}

void ClientDisconnect(Client* client, double time)
{
    if (client->m_clientState == CLIENT_STATE_CONNECTED) {
        printf("client-side disconnect: (client salt = %" PRIx64 ", challenge salt = %" PRIx64 ")\n", client->m_clientSalt, client->m_challengeSalt);
        ConnectionDisconnectPacket connectionDisconnectPacket;
        connectionDisconnectPacket.packet_type = 3;
        connectionDisconnectPacket.client_server_type = 3;

        connectionDisconnectPacket.client_salt = client->m_clientSalt;
        connectionDisconnectPacket.challenge_salt = client->m_challengeSalt;

        uint8_t packet_buffer[1024];

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionDisconnectPacket(&writeStream, &connectionDisconnectPacket);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        ClientSendPacketToServer(client, packet_buffer, bytesProcessed, time);
    }

    ClientResetConnectionData(client);
}

void ClientSendPackets(Client* client, double time)
{
    switch (client->m_clientState) {
    case CLIENT_STATE_SENDING_CONNECTION_REQUEST: {
        if (client->m_lastPacketSendTime + ConnectionRequestSendRate > time)
            return;

        char buffer[256];
        const char* addressString = AddressToString(client->m_serverAddress, buffer, sizeof(buffer));
        printf("client sending connection request to server: %s\n", addressString);


        uint8_t buff[1024];

        ConnectionRequestPacket packet;
        packet.packet_type = 3;
        packet.client_server_type = 0;

        packet.client_salt = client->m_clientSalt;

        Stream writeStream;
        r_stream_write_init(&writeStream, buff, 1024);

        SerializeConnectionRequestPacket(&writeStream, &packet);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        printf("pk.client_salt: %llu\n", client->m_clientSalt);

        ClientSendPacketToServer(client, buff, bytesProcessed, time);
    } break;

    case CLIENT_STATE_SENDING_CHALLENGE_RESPONSE: {
        if (client->m_lastPacketSendTime + ConnectionResponseSendRate > time)
            return;

        char buffer[256];
        const char* addressString = AddressToString(client->m_serverAddress, buffer, sizeof(buffer));
        printf("client sending challenge response to server: %s\n", addressString);

        

        ConnectionResponsePacket packet;
        packet.packet_type = 3;
        packet.client_server_type = 3;

        packet.client_salt = client->m_clientSalt;
        packet.challenge_salt = client->m_challengeSalt;

        uint8_t packet_buffer[1024];

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionResponsePacket(&writeStream, &packet);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);

        ClientSendPacketToServer(client, packet_buffer, bytesProcessed, time);
    } break;

    case CLIENT_STATE_CONNECTED: {
        if (client->m_lastPacketSendTime + ConnectionKeepAliveSendRate > time)
            return;

        uint8_t packet_buffer[1024];

        ConnectionKeepAlivePacket packet;
        packet.packet_type = 3;
        packet.client_server_type = 4;

        packet.client_salt = client->m_clientSalt;
        packet.challenge_salt = client->m_challengeSalt;

        Stream writeStream;
        r_stream_write_init(&writeStream, packet_buffer, 1024);

        SerializeConnectionKeepAlivePacket(&writeStream, &packet);

        FlushBits(&writeStream);

        size_t bytesProcessed = GetBytesProcessed(&writeStream);


        ClientSendPacketToServer(client, packet_buffer, bytesProcessed, time);
    } break;

    default:
        break;
    }
}

bool ClientReceivePackets(Client* client, double time, Address address, Stream* stream, int client_packet_type)
{
    printf("Client recieved packet type ");
    printPackType((PacketTypes)client_packet_type);

    switch (client_packet_type) {
    case PACKET_CONNECTION_DENIED: {
        ConnectionDeniedPacket connectionDeniedPacket;
        serialize_uint64(stream, &connectionDeniedPacket.client_salt);
        serialize_enum(stream, connectionDeniedPacket.reason, ConnectionDeniedReason, CONNECTION_DENIED_NUM_VALUES);

        ClientProcessConnectionDenied(client, &connectionDeniedPacket, address, time);
    }

    break;

    case PACKET_CONNECTION_CHALLENGE: {
        ConnectionChallengePacket connectionChallengePacket;
        serialize_uint64(stream, &connectionChallengePacket.client_salt);
        serialize_uint64(stream, &connectionChallengePacket.challenge_salt);

        ClientProcessConnectionChallenge(client, &connectionChallengePacket, address, time);
    }

    break;

    case PACKET_CONNECTION_KEEP_ALIVE: {
        ConnectionKeepAlivePacket connectionKeepAlivePacket;
        serialize_uint64(stream, &connectionKeepAlivePacket.client_salt);
        serialize_uint64(stream, &connectionKeepAlivePacket.challenge_salt);

        ClientProcessConnectionKeepAlive(client, &connectionKeepAlivePacket, address, time);
    }

    break;

    case PACKET_CONNECTION_DISCONNECT: {
        ConnectionDisconnectPacket connectionDisconnectPacket;
        serialize_uint64(stream, &connectionDisconnectPacket.client_salt);
        serialize_uint64(stream, &connectionDisconnectPacket.challenge_salt);

        ClientProcessConnectionDisconnect(client, &connectionDisconnectPacket, address, time);
    }

    break;

    default:
        break;
    }

    return true;
}

void ClientCheckForTimeOut(Client* client, double time)
{
    switch (client->m_clientState) {
    case CLIENT_STATE_SENDING_CONNECTION_REQUEST: {
        if (client->m_lastPacketReceiveTime + ConnectionRequestTimeOut < time) {
            printf("connection request to server timed out\n");
            client->m_clientState = CLIENT_STATE_CONNECTION_REQUEST_TIMED_OUT;
            return;
        }

        if (client->m_clientSaltExpiryTime < time) {
            client->m_clientSalt = GenerateSalt();
            client->m_clientSaltExpiryTime = time + ClientSaltTimeout;
            printf("client salt timed out. new client salt is %" PRIx64 "\n", client->m_clientSalt);
        }
    } break;

    case CLIENT_STATE_SENDING_CHALLENGE_RESPONSE: {
        if (client->m_lastPacketReceiveTime + ChallengeResponseTimeOut < time) {
            printf("challenge response to server timed out\n");
            client->m_clientState = CLIENT_STATE_CHALLENGE_RESPONSE_TIMED_OUT;
            return;
        }
    } break;

    case CLIENT_STATE_CONNECTED: {
        if (client->m_lastPacketReceiveTime + KeepAliveTimeOut < time) {
            printf("keep alive timed out\n");
            client->m_clientState = CLIENT_STATE_KEEP_ALIVE_TIMED_OUT;
            ClientDisconnect(client, time);
            return;
        }
    } break;

    default:
        break;
    }
}

 void ClientResetConnectionData(Client* client)
{
    client->m_serverAddress = address_set();
    client->m_clientState = CLIENT_STATE_DISCONNECTED;
    client->m_clientSalt = 0;
    client->m_challengeSalt = 0;
    client->m_lastPacketSendTime = -1000.0;
    client->m_lastPacketReceiveTime = -1000.0;
}

void ClientSendPacketToServer(Client* client, void* packet, int packetSize, double time)
{
    assert(client->m_clientState != CLIENT_STATE_DISCONNECTED);
    //assert(client->m_serverAddress.IsValid());

    SendPacket(*client->m_socket, client->m_serverAddress, packet, packetSize);

    client->m_lastPacketSendTime = time;
}

void ClientProcessConnectionDenied(Client* client, const ConnectionDeniedPacket* packet, Address address, double time)
{
    if (client->m_clientState != CLIENT_STATE_SENDING_CONNECTION_REQUEST)
        return;

    if (packet->client_salt != client->m_clientSalt)
        return;

    if (address.m_address_ipv4 != client->m_serverAddress.m_address_ipv4)
        return;

    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    if (packet->reason == CONNECTION_DENIED_SERVER_FULL) {
        printf("client received connection denied from server: %s (server is full)\n", addressString);
        client->m_clientState = CLIENT_STATE_CONNECTION_DENIED_FULL;
    } else if (packet->reason == CONNECTION_DENIED_ALREADY_CONNECTED) {
        printf("client received connection denied from server: %s (already connected)\n", addressString);
        client->m_clientState = CLIENT_STATE_CONNECTION_DENIED_ALREADY_CONNECTED;
    }
}

void ClientProcessConnectionChallenge(Client* client, const ConnectionChallengePacket* packet, Address address, double time)
{
    if (client->m_clientState != CLIENT_STATE_SENDING_CONNECTION_REQUEST)
        return;

    if (packet->client_salt != client->m_clientSalt)
        return;

    if (address.m_address_ipv4 != client->m_serverAddress.m_address_ipv4)
        return;

    char buffer[256];
    const char* addressString = AddressToString(address, buffer, sizeof(buffer));
    printf("client received connection challenge from server: %s (challenge salt = %" PRIx64 ")\n", addressString, packet->challenge_salt);

    client->m_challengeSalt = packet->challenge_salt;

    client->m_clientState = CLIENT_STATE_SENDING_CHALLENGE_RESPONSE;

    client->m_lastPacketReceiveTime = time;
}

void ClientProcessConnectionKeepAlive(Client* client, const ConnectionKeepAlivePacket* packet, Address address, double time)
{
    if (client->m_clientState < CLIENT_STATE_SENDING_CHALLENGE_RESPONSE)
        return;

    if (packet->client_salt != client->m_clientSalt)
        return;

    if (packet->challenge_salt != client->m_challengeSalt)
        return;

    if (address.m_address_ipv4 != client->m_serverAddress.m_address_ipv4)
        return;

    if (client->m_clientState == CLIENT_STATE_SENDING_CHALLENGE_RESPONSE) {
        char buffer[256];
        const char* addressString = AddressToString(address, buffer, sizeof(buffer));
        printf("client is now connected to server: %s\n", addressString);
        client->m_clientState = CLIENT_STATE_CONNECTED;
    }

    client->m_lastPacketReceiveTime = time;
}

void ClientProcessConnectionDisconnect(Client* client, const ConnectionDisconnectPacket* packet, Address address, double time)
{
    if (client->m_clientState != CLIENT_STATE_CONNECTED)
        return;

    if (packet->client_salt != client->m_clientSalt)
        return;

    if (packet->challenge_salt != client->m_challengeSalt)
        return;

    if (address.m_address_ipv4 != client->m_serverAddress.m_address_ipv4)
        return;

    ClientDisconnect(client, time);
}


#endif