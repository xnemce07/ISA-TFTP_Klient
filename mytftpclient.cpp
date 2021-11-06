#include <iostream>
#include <unistd.h>
#include <fstream>

#include "argparser.h"
#include "tftp_lib.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <ctime>

#define MAX_RETRIES 3

using namespace std;

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
    const char *my_tid = "3232";
    const char *server_tid = "";

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
                perror("Didnt find a match in ai_family");
                continue;
            }

            if ((sockfd = socket(p_client->ai_family, p_client->ai_socktype, p_client->ai_protocol)) == -1)
            {
                continue;
            }

            if (bind(sockfd, p_client->ai_addr, p_client->ai_addrlen))
            {
                close(sockfd);
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

    if (options.timeout > 0)
    {
        struct timeval timeout;
        timeout.tv_sec = options.timeout;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            perror("Setsockopt");
        }
    }


    ofstream file;
    short retries = 0;

    if (options.read)
    {
        file.open(get_filename(options.path), ios::out | ios::trunc);

        struct sockaddr_storage server_addr;
        socklen_t server_addrlen = sizeof(server_addr);
        int block_no = 0;
        int recvlen, sendlen, sentlen, errlen;
        char recvbuf[MAXBUFLEN], sendbuf[MAXBUFLEN], errbuff[MAXBUFLEN];
        char server_IP[INET6_ADDRSTRLEN], server_port[6];
        char server_TID[6] = "";
        bool errorflag = false;

        sendlen = build_rrq_packet(sendbuf, options.path, options.mode);
        sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen);

        cout << timestamp() << "Sent read request with the size of " << sentlen << " bytes to " << options.ip << ':' << options.port << endl;

        do
        {

            if (!errorflag)
            {
                block_no++;
            }
            else
            {
                sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                errorflag = false;
                retries++;
            }

            cout << timestamp() << "Waiting for response...\n";
            recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen);
            if (recvlen < 0)
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "Connection timed out, sending last packet again.\n";
                errorflag = true;
                recvlen = options.size + 4;
                continue;
            }

            getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
            if (!strcmp(server_port, server_TID) && !strcmp(server_port, "0"))
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "TID's don't match, sending last packet again.\n";
                errlen = build_error_packet(errbuff, 5, "Unknown TID.\n");
                sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                errorflag = true;
                continue;
            }

            strcpy(server_TID, server_port);

            if (ntohs(*(short *)recvbuf) == OP_ERROR)
            {
                cerr << "Error: " << recvbuf + 4 << endl;
                break;
            }
            else
            {
                if (handle_data_packet(recvbuf, block_no, recvlen, &file))
                {

                    cout << timestamp() << "Received " << recvlen - 4 << " bytes from " << server_IP << ':' << server_port << endl;

                    sendlen = build_ack_packet(sendbuf, block_no);
                    sentlen = sendto(sockfd, sendbuf, sendlen, 0, (struct sockaddr *)&server_addr, server_addrlen);

                    cout << timestamp() << "Sent acknowledgement to " << server_IP << ':' << server_port << endl;
                }
                else
                {
                    if (retries >= MAX_RETRIES)
                    {
                        cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                        break;
                    }
                    cout << timestamp() << "Unexpected block number, sending last packet again.\n";
                    errorflag = true;
                    continue;
                }
            }

            retries = 0;
        } while (recvlen == options.size + 4);

        file.close();
    }

    freeaddrinfo(server_servinfo);
    close(sockfd);
    return 0;
}
