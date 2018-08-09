#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 4096
#define NEXP 1000

#define SERVER_PORT     1337
#define SERVER_IP       "127.0.0.1"
#define FILENAME        "recv_file"
#define FILETIME        "time.txt"

#define SO_SMKEX_NOCRYPT 0xA001
#define SO_SMKEX_DHONLY 0xA002

void my_receive(int sockfd, char * buffer, int length) {
    int bytes_received = 0;
    int rc;
    while (bytes_received < length) {
        rc = recv(sockfd, buffer + bytes_received, length - bytes_received, 0);
        CHECK(rc >= 0, "recv");

        bytes_received += rc;
    }
}

double do_connect(unsigned int server_port, char *server_ip, char *filename, char dh_only)
{

    char buffer[BUFFER_SIZE];
    int ret;
    clock_t tstart, tend;
    double tspent;
    char str_time[64];

    // Open a TCP socket
    int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK(client_sockfd >= 0, "socket");

    // Setup sockaddr_in struct
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    server_addr.sin_port = htons(server_port);

    // Set no crypt mode
    //printf("[client] Setting no encryption on socket %d\n", client_sockfd);
    //ret = setsockopt(client_sockfd, SOL_SOCKET, SO_SMKEX_NOCRYPT, NULL, 0);
    //CHECK(ret == 0, "setsockopt");
    
    // Set only DH if requested
    if (dh_only)
    {
      printf("[client] Setting DH ONLY on socket %d\n", client_sockfd);
      ret = setsockopt(client_sockfd, SOL_SOCKET, SO_SMKEX_DHONLY, NULL, 0);
      CHECK(ret == 0, "setsockopt");
    }

    // Connect
    printf("[client] Before connect()\n");
    tstart = clock();
    ret = connect(client_sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    tend = clock();
    CHECK(ret >= 0, "connect");
    if(ret < 0)
      return -1;
    printf("[client] Connected to %d\n", server_port);
    tspent = ((double)(tend - tstart))/CLOCKS_PER_SEC;
    printf("[client] Time to connect: %lf seconds\n", tspent);

    // Recv file size
    ///*
    uint32_t file_size, file_size_net;
    my_receive(client_sockfd, (char*)&file_size_net, sizeof(file_size_net));
    file_size = ntohl(file_size_net);
    printf("[client] Got file size: %u\n", file_size);

    // Open file
    int file_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    CHECK(file_fd >= 0, "open");

    // Receive file
    int total = 0;
    while (total < file_size) {
        int bytes_recv = recv(client_sockfd, buffer, BUFFER_SIZE, 0);
        printf("[client] Received %d bytes\n", (int)bytes_recv);
        write(file_fd, buffer, bytes_recv);
        total += bytes_recv;
    }
    printf("[client] File receive completed\n");

    close(file_fd);
    //*/

    return tspent;
}


int main(int argc, char* argv[]) {
    unsigned int server_port = SERVER_PORT;
    char *server_ip = SERVER_IP;
    char *filename = FILENAME;
    char *filetime = FILETIME;
    int ret, k;
    double conntime, ttime = 0;
    double times[NEXP];
    double totaltimes[NEXP];
    char dh_only = 0;
    clock_t tstart, tend;
    FILE *fd_time;

    int opt;
    while ((opt = getopt(argc, argv, "i:p:f:t:d")) != -1) {
        switch (opt) {
            case 'i':
                server_ip = optarg;
                break;
            case 'p':
                server_port = atoi(optarg);
                break;
            case 'f':
                filename = optarg;
                break;
            case 't':
                filetime = optarg;
                break;
            case 'd':
                dh_only = 1;
                break;
            default:
                fprintf(stderr, "Usage %s [-i SERVER_IP] [-p PORT] [-f RECV_FILENAME] [-t TIME_FILENAME] [-d]\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }


    // Open file for time
    fd_time = fopen(filetime, "w");
    CHECK(fd_time != NULL, "fopen");

    for (k=0; k < NEXP; k++)
    {
      tstart = clock();
      conntime = do_connect(server_port, server_ip, filename, dh_only);
      tend = clock();
      ttime += conntime;
      times[k] = conntime;
      totaltimes[k] = ((double)(tend - tstart))/CLOCKS_PER_SEC;
      printf("[client] Time connect(): %lf\n", times[k]);
      printf("[client] Total time connect+transfer: %lf\n", totaltimes[k]);
      fprintf(fd_time, "%lf\n", times[k]);
    }

    fclose(fd_time);

    return 0;
}
