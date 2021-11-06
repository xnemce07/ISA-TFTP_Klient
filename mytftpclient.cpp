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

#define TIMEBUFF_SIZE 30

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
    const char *my_tid = "2323";
    const char *server_tid = "";

    //**************Finding a match and creating a socket + bind******************////
    struct addrinfo client_hints, *client_servinfo, *p_client;
    struct addrinfo server_hints, *server_servinfo, *p_server;

    int rv;
    int sockfd;

    char timebuff[TIMEBUFF_SIZE];
    time_t rawtime;
    struct tm *timeinfo;

    time(&rawtime);
    timeinfo = localtime(&rawtime);

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

    if (options.timeout > 0)
    {
        struct timeval timeout;
        timeout.tv_sec = options.timeout;
        timeout.tv_usec = 0;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
        {
            perror("Setsockopt: ");
        }
    }

    //Here we shoul be able to sendto() and recvfrom() p_server for first request, then with port from their info from recvfrom()

    // if (options.read)
    // {
    //     
    //     
    //     ofstream myfile;
    //     myfile.open(get_filename(options.path), ios::out | ios::trunc);

    //     char recv_buf[MAXBUFLEN];
    //     char send_buf[MAXBUFLEN];
    //     int recvbytes;
    //     int sentbytes, send_len;

    //     struct sockaddr_storage their_addr;
    //     socklen_t addr_len = sizeof(their_addr);
    //     char their_ip[INET6_ADDRSTRLEN]; //char their_ip[NI_MAXHOST];
    //     char their_port[NI_MAXSERV];
    //     int block_number = 0;

    //     send_len = build_rrq_packet(send_buf, options.path, options.mode);

    //     sentbytes = sendto(sockfd, send_buf, send_len, 0, p_server->ai_addr, p_server->ai_addrlen);

    //     cout << "SENT " << sentbytes << "BYTES.\n";

    //     cout << "Waiting for response.\n";

    //     do
    //     {
    //         block_number++;
    //         if ((recvbytes = recvfrom(sockfd, recv_buf, MAXBUFLEN, 0, (struct sockaddr *)&their_addr, &addr_len)) == -1)
    //         {
    //             close(sockfd);
    //             perror("recvfrom");
    //             exit(1);
    //         }

    //         //source: https://stackoverflow.com/questions/7335738/retrieving-ip-and-port-from-a-sockaddr-storage/39816122
    //         getnameinfo((struct sockaddr *)&their_addr, addr_len, their_ip, sizeof(their_ip), their_port, sizeof(their_port), NI_NUMERICHOST | NI_NUMERICSERV);

    //         if (ntohs(*(short *)recv_buf) == OP_ERROR)
    //         {
    //             cerr << "Error: " << recv_buf + 4 << endl;
    //             break;
    //         }
    //         else
    //         {
    //             if (handle_data_packet(recv_buf, block_number, recvbytes, &myfile))
    //             {
    //                 cout << "Received " << recvbytes - 4 << " bytes.\n";
    //                 *(short *)recv_buf = htons(OP_ACK);

    //                 sentbytes  = build_ack_packet(send_buf,block_number);
    //                 sentbytes = sendto(sockfd, recv_buf, sentbytes, 0, (struct sockaddr *)&their_addr, addr_len);
    //                 cout << "Sent acknowledgement.\n";
    //             }
    //         }

    //     } while (recvbytes == options.size + 4);

    //     myfile.close();
    // }

    ofstream file;
    if (options.read)
    {
        file.open(get_filename(options.path), ios::out | ios::trunc);

        struct sockaddr_storage server_addr;
        socklen_t server_addrlen = sizeof(server_addr);
        int block_no = 0;
        int recvlen, sendlen, sentlen, errlen;
        char recvbuf[MAXBUFLEN], sendbuf[MAXBUFLEN], errbuff[MAXBUFLEN];
        char server_IP[INET6_ADDRSTRLEN], server_port[6];
        char server_TID[6] = "0";
        bool errorflag = false;

        sendlen = build_rrq_packet(sendbuf, options.path, options.mode);
        sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen);

        strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
        cout << timebuff << "Sent read request the size of " << sentlen << " bytes to " << options.ip << ':' << options.port << endl;

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
            }

            strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
            cout << timebuff << "Waiting for response...\n";
            recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen);
            if (recvlen < 0)
            {
                strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                cout << timebuff << "Connection timed out, sending last packet again.\n";
                errorflag = true;
                recvlen = options.size + 4;
                continue;
            }

            getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
            if (!strcmp(server_port, server_TID) && !strcmp(server_port, "0"))
            {

                strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                cout << timebuff << "TID's don't match, sending last packet again.\n";
                errlen = build_error_packet(errbuff, 5, "Unknown TID.\n");
                sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                errorflag = true;
                continue;
            }

            strcpy(server_TID, server_port);

            if (ntohs(*(short *)recvbuf) == OP_ERROR)
            {
                strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                cerr << timebuff << "Error: " << recvbuf + 4 << endl;
                break;
            }
            else
            {
                if (handle_data_packet(recvbuf, block_no, recvlen, &file))
                {
                    strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                    cout << timebuff << "Received " << recvlen - 4 << " bytes from " << server_IP << ':' << server_port << endl;

                    sendlen = build_ack_packet(sendbuf, block_no);
                    sentlen = sendto(sockfd, sendbuf, sendlen, 0, (struct sockaddr *)&server_addr, server_addrlen);
                    strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                    cout << timebuff << "Sent acknowledgement to " << server_IP << ':' << server_port << endl;
                }
                else
                {
                    strftime(timebuff, TIMEBUFF_SIZE, "[%F %X]\t", timeinfo);
                    cout << timebuff << "Unexpected block number, sending last packet again.\n";
                    errorflag = true;
                    continue;
                }
            }

        } while (recvlen == options.size + 4);

        file.close();
    }

    freeaddrinfo(server_servinfo);
    close(sockfd);
    return 0;
}
