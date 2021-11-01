#include <iostream>

#include "argparser.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>

#define OP_RRQ 1
#define MAXBUFLEN 100
#define TIMEOUT 10
#define BSIZE 600

using namespace std;

void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}



int main(int argc, char *argv[])
{

    
    args options;

    if (!argparser(&options, argc, argv))
    {
        return false;
    }

    if (!check_options(&options))
    {
        return false;
    }

    //TODO: Randomize tid
    const char *my_tid = "2323";

    //**************Finding a match and creating a socket + bind******************////
    struct addrinfo client_hints, *client_servinfo, *p_client;
    struct addrinfo server_hints, *server_servinfo, *p_server;

    int rv;
    int sockfd;

    //CLIENT getaddrinfo()
    memset(&client_hints, 0, sizeof(client_hints));
    client_hints.ai_family = AF_UNSPEC;
    client_hints.ai_socktype = SOCK_DGRAM;
    client_hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, my_tid, &client_hints, &client_servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    //SERVER getaddrinfo()
    memset(&server_hints, 0, sizeof(client_hints));
    server_hints.ai_family = AF_UNSPEC;
    server_hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(options.ip.c_str(), options.port.c_str(), &server_hints, &server_servinfo)) != 0)
    {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    for (p_client = client_servinfo; p_client != NULL; p_client = p_client->ai_next)
    {
        for (p_server = server_servinfo; p_server != NULL; p_server = p_server->ai_next)
        {
            //TODO: check if we also need to compare ai_protocol and ai_socktype
            if (p_server->ai_family != p_client->ai_family)
            {
                perror("Didnt find a match in ai_family.\n");
                continue;
            }

            if ((sockfd = socket(p_client->ai_family, p_client->ai_socktype, p_client->ai_protocol)) == -1)
            {
                perror("socket() failed.\n");
                continue;
            }

            if (bind(sockfd, p_client->ai_addr, p_client->ai_addrlen))
            {
                close(sockfd);
                perror("bind() failed.\n");
                continue;
            }

            break;
        }

        if (p_server != NULL)
        {
            break;
        }
    }


    
    freeaddrinfo(client_servinfo);
    //Here we shoul be able to sendto() and recvfrom() p_server for first request, then with port from their info from recvfrom()

    //message building
    char   message[BSIZE], *p;
    *(short *)message = htons(OP_RRQ);
    p = message + 2;
    strcpy(p, options.path.c_str());			/* The file name */
    p += options.path.length()    + 1;	/* Keep the nul  */
    strcpy(p, options.mode.c_str());			/* The Mode      */
    p += options.mode.length() + 1;

    


    //message building end
    int numbytes = sendto(sockfd, message, p-message, 0, p_server->ai_addr, p_server->ai_addrlen);

    cout << "SENT " << numbytes << "BYTES.\n";
    cout << "MESSAGE: " << message << endl;

    struct sockaddr_storage their_addr;
    char buf[MAXBUFLEN];
    socklen_t addr_len;
    char s[INET6_ADDRSTRLEN];

    std::cout << "WAITING FOR RESPONSE.\n";
    addr_len = sizeof(their_addr);

    if ((numbytes = recvfrom(sockfd, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
    {
        close(sockfd);
        perror("recvfrom");
        exit(1);
    }

    printf("listener: got packet from %s\n", inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s));
    printf("listener: packet is %d bytes long\n", numbytes);

    buf[numbytes] = '\0';
    printf("listener: packet contains \"%s\"\n", buf);

    freeaddrinfo(server_servinfo);
    close(sockfd);
    return 0;
}
