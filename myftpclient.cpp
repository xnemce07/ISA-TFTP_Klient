#include <iostream>

#include "argparser.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>

#define OP_RRQ 1 /* TFTP op-codes */
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

#define MAXBUFLEN 1024
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

string get_filename(string path)
{
    return path.substr(path.find_last_of("/")+1);
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
        cerr << "Client getaddrinfo: " << gai_strerror(rv) << endl;
        return 1;
    }

    //SERVER getaddrinfo()
    memset(&server_hints, 0, sizeof(client_hints));
    server_hints.ai_family = AF_UNSPEC;
    server_hints.ai_socktype = SOCK_DGRAM;

    if ((rv = getaddrinfo(options.ip.c_str(), options.port.c_str(), &server_hints, &server_servinfo)) != 0)
    {
        cerr << "Server getaddrinfo: " << gai_strerror(rv) << endl;
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

    if (options.write)
    {
        //open/create file to write in
        ofstream myfile;
        myfile.open(get_filename(options.path),ios::out | ios::trunc);

        //message building
        //TODO: Change this part
        char message[BSIZE], *p;
        *(short *)message = htons(OP_RRQ);
        p = message + 2;
        strcpy(p, options.path.c_str()); /* The file name */
        p += options.path.length() + 1;  /* Keep the nul  */
        strcpy(p, options.mode.c_str()); /* The Mode      */
        p += options.mode.length() + 1;
        //message building end

        int sentbytes = sendto(sockfd, message, p - message, 0, p_server->ai_addr, p_server->ai_addrlen);

        cout << "SENT " << sentbytes << "BYTES.\n";

        struct sockaddr_storage their_addr;
        char buf[MAXBUFLEN];
        socklen_t addr_len = sizeof(their_addr);
        char their_ip[INET6_ADDRSTRLEN]; //char their_ip[NI_MAXHOST];
        char their_port[NI_MAXSERV];
        int recvbytes;

        cout << "Waiting for response.\n";

        do
        {

            if ((recvbytes = recvfrom(sockfd, buf, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
            {
                close(sockfd);
                perror("recvfrom");
                exit(1);
            }

            //source: https://stackoverflow.com/questions/7335738/retrieving-ip-and-port-from-a-sockaddr-storage/39816122
            if (!getnameinfo((struct sockaddr *)&their_addr, addr_len, their_ip, sizeof(their_ip), their_port, sizeof(their_port), NI_NUMERICHOST | NI_NUMERICSERV))
            {
                //cout << "Response from " << their_ip << ":" << their_port << ".\n";
            }

            if (ntohs(*(short *)buf) == OP_ERROR)
            {
                cerr << "rcat: " << buf + 4 << endl;
            }
            else
            {
                //cout << buf + 4;
                myfile << buf + 4;

                *(short *)buf = htons(OP_ACK);
                sendto(sockfd, buf, 4, 0, (struct sockaddr *)&their_addr, addr_len);
            }

        } while (recvbytes == options.size + 4);
    }

    freeaddrinfo(server_servinfo);
    close(sockfd);
    return 0;
}
