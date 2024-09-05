#ifndef PROXY_SERVER_WITH_CACHE_H
#define PROXY_SERVER_WITH_CACHE_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <time.h>

#define MAX_BYTES 4096
#define MAX_CLIENTS 400
#define MAX_SIZE 200 * (1 << 20)
#define MAX_ELEMENT_SIZE 10 * (1 << 20)

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

int sendErrorMessage(SOCKET socket, int status_code);
SOCKET connectRemoteServer(char *host_addr, int port_num);
int handle_request(SOCKET clientSocket, ParsedRequest *request, char *tempReq);
unsigned __stdcall handleClient(void *p);
int main();

#endif // PROXY_SERVER_WITH_CACHE_H
