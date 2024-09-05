
// // Header Files
#include "proxy_parse.h"
#include "proxy_server_with_cache.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
// Defines data types used in system calls.
#include <sys/types.h>
// Provides file control options.
#include <fcntl.h>
// Defines macros for reporting and retrieving error conditions through error codes.
#include <errno.h>

// Include Windows header for thread and synchronization functions
#include <windows.h>
// Sockets and Networking
#include <winsock2.h>
#include <ws2tcpip.h>

#define MAX_BYTES 4096                  // max allowed size of request/response
#define MAX_CLIENTS 400                 // max number of client requests served at a time
#define MAX_SIZE 200 * (1 << 20)        // size of the cache
#define MAX_ELEMENT_SIZE 10 * (1 << 20) // max size of an element in cache

typedef struct cache_element cache_element;

struct cache_element
{
    char *data;
    int len;
    char *url;
    time_t lru_time_track;
    cache_element *next;
};

cache_element *find(char *url);
int add_cache_element(char *data, int size, char *url);
void remove_cache_element();

int port_number = 8080;
SOCKET proxy_socketId;
HANDLE tid[MAX_CLIENTS];
HANDLE semaphore;
HANDLE cache_lock;

cache_element *head;
int cache_size;

int sendErrorMessage(SOCKET socket, int status_code)
{
    char str[1024];
    char currentTime[50];
    time_t now = time(0);

    struct tm data = *gmtime(&now);
    strftime(currentTime, sizeof(currentTime), "%a, %d %b %Y %H:%M:%S %Z", &data);

    switch (status_code)
    {
    case 400:
        snprintf(str, sizeof(str), "HTTP/1.1 400 Bad Request\r\nContent-Length: 95\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\n<BODY><H1>400 Bad Request</H1>\n</BODY></HTML>", currentTime);
        printf("400 Bad Request\n");
        send(socket, str, strlen(str), 0);
        break;

    case 403:
        snprintf(str, sizeof(str), "HTTP/1.1 403 Forbidden\r\nContent-Length: 112\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\n<BODY><H1>403 Forbidden</H1><br>Permission Denied\n</BODY></HTML>", currentTime);
        printf("403 Forbidden\n");
        send(socket, str, strlen(str), 0);
        break;

    case 404:
        snprintf(str, sizeof(str), "HTTP/1.1 404 Not Found\r\nContent-Length: 91\r\nContent-Type: text/html\r\nConnection: keep-alive\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\n<BODY><H1>404 Not Found</H1>\n</BODY></HTML>", currentTime);
        printf("404 Not Found\n");
        send(socket, str, strlen(str), 0);
        break;

    case 500:
        snprintf(str, sizeof(str), "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 115\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\n<BODY><H1>500 Internal Server Error</H1>\n</BODY></HTML>", currentTime);
        send(socket, str, strlen(str), 0);
        break;

    case 501:
        snprintf(str, sizeof(str), "HTTP/1.1 501 Not Implemented\r\nContent-Length: 103\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>501 Not Implemented</TITLE></HEAD>\n<BODY><H1>501 Not Implemented</H1>\n</BODY></HTML>", currentTime);
        printf("501 Not Implemented\n");
        send(socket, str, strlen(str), 0);
        break;

    case 505:
        snprintf(str, sizeof(str), "HTTP/1.1 505 HTTP Version Not Supported\r\nContent-Length: 125\r\nConnection: keep-alive\r\nContent-Type: text/html\r\nDate: %s\r\nServer: VaibhavN/14785\r\n\r\n<HTML><HEAD><TITLE>505 HTTP Version Not Supported</TITLE></HEAD>\n<BODY><H1>505 HTTP Version Not Supported</H1>\n</BODY></HTML>", currentTime);
        printf("505 HTTP Version Not Supported\n");
        send(socket, str, strlen(str), 0);
        break;

    default:
        return -1;
    }
    return 1;
}

SOCKET connectRemoteServer(char *host_addr, int port_num)
{
    SOCKET remoteSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (remoteSocket == INVALID_SOCKET)
    {
        printf("Error in Creating Socket.\n");
        return INVALID_SOCKET;
    }

    struct addrinfo *result = NULL, hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host_addr, NULL, &hints, &result) != 0)
    {
        fprintf(stderr, "No such host exists.\n");
        return INVALID_SOCKET;
    }

    struct sockaddr_in server_addr;
    ZeroMemory(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_num);
    server_addr.sin_addr = ((struct sockaddr_in *)result->ai_addr)->sin_addr;

    if (connect(remoteSocket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Error in connecting !\n");
        closesocket(remoteSocket);
        return INVALID_SOCKET;
    }

    freeaddrinfo(result);
    return remoteSocket;
}

int handle_request(SOCKET clientSocket, ParsedRequest *request, char *tempReq)
{
    char *buf = (char *)malloc(sizeof(char) * MAX_BYTES);
    strcpy(buf, "GET ");
    strcat(buf, request->path);
    strcat(buf, " ");
    strcat(buf, request->version);
    strcat(buf, "\r\n");

    size_t len = strlen(buf);

    if (ParsedHeader_set(request, "Connection", "close") < 0)
    {
        printf("set header key not work\n");
    }

    if (ParsedHeader_get(request, "Host") == NULL)
    {
        if (ParsedHeader_set(request, "Host", request->host) < 0)
        {
            printf("Set \"Host\" header key not working\n");
        }
    }

    if (ParsedRequest_unparse_headers(request, buf + len, (size_t)MAX_BYTES - len) < 0)
    {
        printf("unparse failed\n");
    }

    int server_port = 80;
    if (request->port != NULL)
        server_port = atoi(request->port);

    SOCKET remoteSocketID = connectRemoteServer(request->host, server_port);

    if (remoteSocketID == INVALID_SOCKET)
        return -1;

    int bytes_send = send(remoteSocketID, buf, strlen(buf), 0);

    ZeroMemory(buf, MAX_BYTES);

    bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    char *temp_buffer = (char *)malloc(sizeof(char) * MAX_BYTES);
    int temp_buffer_size = MAX_BYTES;
    int temp_buffer_index = 0;

    while (bytes_send > 0)
    {
        bytes_send = send(clientSocket, buf, bytes_send, 0);

        for (int i = 0; i < bytes_send; i++)
        {
            temp_buffer[temp_buffer_index] = buf[i];
            temp_buffer_index++;
        }
        temp_buffer_size += MAX_BYTES;
        temp_buffer = (char *)realloc(temp_buffer, temp_buffer_size);

        if (bytes_send < 0)
        {
            perror("Error in sending data to client socket.\n");
            break;
        }
        ZeroMemory(buf, MAX_BYTES);

        bytes_send = recv(remoteSocketID, buf, MAX_BYTES - 1, 0);
    }

    if (temp_buffer_index > 0)
    {
        int success = add_cache_element(temp_buffer, temp_buffer_index, request->path);
        if (success < 0)
        {
            fprintf(stderr, "Error in adding cache element\n");
        }
    }

    free(temp_buffer);
    closesocket(remoteSocketID);
    return 0;
}

unsigned __stdcall handleClient(void *p)
{
    SOCKET clientSocket = *(SOCKET *)p;
    char requestBuffer[MAX_BYTES];
    int bytesReceived = recv(clientSocket, requestBuffer, sizeof(requestBuffer), 0);
    if (bytesReceived <= 0)
    {
        closesocket(clientSocket);
        return 0;
    }

    ParsedRequest *request = parse_request(requestBuffer);
    if (request == NULL)
    {
        sendErrorMessage(clientSocket, 400);
        closesocket(clientSocket);
        return 0;
    }
    int cached = 0;
    if (find(request->path) != NULL)
    {
        // Handle cache hit
        // Note: Implement logic to handle cache hit
        cached = 1;
    }

    if (!cached)
    {
        if (handle_request(clientSocket, request, requestBuffer) < 0)
        {
            sendErrorMessage(clientSocket, 500);
        }
    }

    ParsedRequest_free(request);
    closesocket(clientSocket);
    return 0;
}

int main()
{
    WSADATA wsaData;
    SOCKET acceptSocket;
    struct sockaddr_in serverAddr;
    int clientAddrSize = sizeof(struct sockaddr_in);

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        fprintf(stderr, "Failed to initialize Winsock.\n");
        return 1;
    }

    proxy_socketId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (proxy_socketId == INVALID_SOCKET)
    {
        fprintf(stderr, "Socket creation failed.\n");
        WSACleanup();
        return 1;
    }

    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port_number);

    if (bind(proxy_socketId, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
    {
        fprintf(stderr, "Socket binding failed.\n");
        closesocket(proxy_socketId);
        WSACleanup();
        return 1;
    }

    if (listen(proxy_socketId, SOMAXCONN) == SOCKET_ERROR)
    {
        fprintf(stderr, "Listening failed.\n");
        closesocket(proxy_socketId);
        WSACleanup();
        return 1;
    }

    printf("Proxy server running on port %d...\n", port_number);

    while (1)
    {
        SOCKET clientSocket = accept(proxy_socketId, (struct sockaddr *)&serverAddr, &clientAddrSize);
        if (clientSocket == INVALID_SOCKET)
        {
            fprintf(stderr, "Client connection failed.\n");
            continue;
        }

        int i;
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (tid[i] == NULL)
            {
                tid[i] = (HANDLE)_beginthreadex(NULL, 0, handleClient, &clientSocket, 0, NULL);
                if (tid[i] == NULL)
                {
                    fprintf(stderr, "Error creating thread.\n");
                }
                break;
            }
        }

        if (i == MAX_CLIENTS)
        {
            WaitForSingleObject(semaphore, INFINITE);
            tid[i] = (HANDLE)_beginthreadex(NULL, 0, handleClient, &clientSocket, 0, NULL);
            if (tid[i] == NULL)
            {
                fprintf(stderr, "Error creating thread after semaphore.\n");
            }
        }
    }

    closesocket(proxy_socketId);
    WSACleanup();
    return 0;
}

// // Header Files
// #include "proxy_parse.h"
// #include <stdio.h>
// #include <string.h>
// #include <time.h>
// #include <stdlib.h>
// // Defines data types used in system calls.
// #include <sys/types.h>
// // Provides file control options.
// #include <fcntl.h>
// // Defines macros for reporting and retrieving error conditions through error codes.
// #include <errno.h>

// // Include Windows header for thread and synchronization functions
// #include <windows.h>
// // Sockets and Networking
// #include <winsock2.h>
// #include <ws2tcpip.h>

// #define MAX_CLIENTS 10
// // in c we need to again write struct in order to perform something on predefined struct
// typedef struct cache_element cache_element;
// // Components of Cache

// struct cache_element
// {
//     char *data;
//     int len;
//     char *url;
//     time_t lru_time_track;
//     struct cache_element *next;
// };

// // Declaring functions
// cache_element *find(char *url);
// int add_cache_element(char *data, int size, char *url);
// void remove_cache_element();

// // Proxy server will run on Port - 8080
// int port_number = 8080;

// // Forming Sockts for communication over the network
// SOCKET proxy_socketId;

// // Forming threads each thread will have 1 socket (array to store thread ids)
// HANDLE threadid[MAX_CLIENTS];
// HANDLE semaphore;
// HANDLE lock;

// // head of the cache list to find other cache elements
// cache_element *head;
// int cache_size;

// // argc and argv are used to handle command line arguments passed to the program
// // argc is the no of arguments
// // *argv is the pointer to argument vector where 1st argument is the name of the program
// int main(int argc, char *argv[])
// {
//     // client's socket id for the socket that server will create for that client and length of it's address
//     // length of address is needed in server fuctions like accept
//     // it looks like 192.168.1.1:8080
//     // ip:port
//     SOCKET client_socketId;
//     int client_len;

//     // struct sockaddr -> used for storing socket addresses in the sockets API
//     struct sockaddr server_addr, client_addr;

//     // Initialize Winsock
//     WSADATA wsaData;
//     // 2.2 version
//     int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
//     if (iResult != 0)
//     {
//         printf("WSAStartup failed: %d\n", iResult);
//         return 1;
//     }

//     // Create semaphore with initial value MAX_CLIENTS (security attributes,initiallyHowManyThreadsCanAccess,maximumCount,name)
//     semaphore = CreateSemaphore(NULL, MAX_CLIENTS, MAX_CLIENTS, NULL);
//     if (semaphore == NULL)
//     {
//         printf("Semaphore creation failed: %d\n", GetLastError());
//         return 1;
//     }

//     // Initialize mutex (security attribute,initial ownership, name)
//     lock = CreateMutex(NULL, FALSE, NULL);
//     if (lock == NULL)
//     {
//         printf("Mutex creation failed: %d\n", GetLastError());
//         return 1;
//     }

//     // if user wants to run it on differnt port
//     // checking for 2 argumets 1st name of program 2nd port no.
//     if (argc == 2)
//     {
//         // atoi converts string to no.
//         port_number = atoi(argv[1]);
//     }
//     else
//     {
//         printf("Too few arguments\n");
//         // whole program terminates by exit(1) system call;
//         exit(1);
//     }

//     // starting Proxy server
//     printf("Starting Proxy Server at port no.: %d\n", port_number);

//     // 1 main server socket formation
//     //(address family here IPv4,stream socket is connection-oriented here tcp, protocol when provided 0 - select yourself)
//     proxy_socketId = socket(AF_INET, SOCK_STREAM, 0);
//     // if socket connection is not valid then it will return - value
//     if (proxy_socketId < 0)
//     {
//         perror("failed to create a socket\n");
//         exit(1);
//     }
//     // reuse socket for listening
//     int reuse = 1;
//     // to set socket options
//     if (setsockopt(proxy_socketId, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse)) < 1)
//     {
//         perror("setsockopt failed\n");
//         exit(1);
//     }
//     //  Clean up Winsock when done

//     WSACleanup();
//     return 0;
// }
