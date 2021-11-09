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

    if (p_server == NULL)
    {
        perror("Creating a socket failed");
        freeaddrinfo(client_servinfo);
        freeaddrinfo(server_servinfo);
        return 1;
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

    struct sockaddr_storage server_addr;
    socklen_t server_addrlen = sizeof(server_addr);
    int block_no = 0;
    int recvlen, sendlen, sentlen, errlen;
    char recvbuf[MAXBUFLEN], sendbuf[MAXBUFLEN], errbuff[MAXBUFLEN];
    char server_IP[INET6_ADDRSTRLEN], server_port[6];
    char server_TID[6] = "";
    bool errorflag = false;
    short retries = 0;

    if (options.read)
    {
        ofstream file;
        file.open(get_filename(options.path), ios::out | ios::trunc);

        //Sending write request
        sendlen = build_rrq_packet(sendbuf, options.path, options.mode);
        if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
        {
            cout << timestamp() << "Sending read request packet error: " << strerror(errno) << " Sending packet again.\n";
            block_no = 1;
            errorflag = true;
        }
        else
        {
            cout << timestamp() << "Sent read request with the size of " << sentlen << " bytes to " << options.ip << ':' << options.port << endl;
        }

        do
        {

            if (errorflag) //On error, last packet is sent again
            {
                retries++;
                if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
                {
                    if (retries >= MAX_RETRIES)
                    {
                        cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                        break;
                    }
                    cout << timestamp() << "Resending last packet error: " << strerror(errno) << endl;
                    continue;
                }
                else
                {
                    cout << timestamp() << "Sent last " << sentlen << " bytes again" << endl;
                    errorflag = false;
                }
            }
            else
            {
                block_no++;
            }

            cout << timestamp() << "Waiting for response...\n";
            //Recieving packet
            if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "Connection timed out, sending last packet again.\n";
                errorflag = true;
                continue;
            }

            //Checking TID
            getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
            if (!strcmp(server_port, server_TID) && !strcmp(server_port, "0"))
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "TID's don't match, sending error packet and last sent packet.\n";
                errlen = build_error_packet(errbuff, 5, "Unknown TID.\n");
                sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                errorflag = true;
                continue;
            }

            strcpy(server_TID, server_port);

            //Handling packet depending on its type
            if (ntohs(*(short *)recvbuf) == OP_ERROR) //Handling of error packet
            {
                cout << timestamp() << "Error: " << recvbuf + 4 << endl;
                break;
            }
            else if (ntohs(*(short *)recvbuf) == OP_DATA) //Handling of data packet
            {
                if (handle_data_packet(recvbuf, block_no, recvlen, &file))
                {

                    cout << timestamp() << "Received data packet with block number " << block_no << " and size " << recvlen - 4 << " bytes from " << server_IP << ':' << server_port << endl;

                    sendlen = build_ack_packet(sendbuf, block_no);
                    if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, (struct sockaddr *)&server_addr, server_addrlen)) < 0)
                    {
                        if (retries >= MAX_RETRIES)
                        {
                            cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                            break;
                        }
                        cout << timestamp() << "Sending acknowledgment packet error: " << strerror(errno) << " Sending packet again.\n";
                        errorflag = true;
                        continue;
                    }
                    else
                    {
                        cout << timestamp() << "Sent acknowledgement to " << server_IP << ':' << server_port << endl;
                    }
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
            else //While reading from server, no other packet type is expected
            {

                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "Unexpected packet type, sending last packet again.\n";
                errorflag = true;
                continue;
            }

            retries = 0; //When this point is reached, any unsuccessful operation had to be completed, so we reset the retries counter
        } while (recvlen == options.size + 4 || errorflag);

        file.close();
    }
    else
    {
        ifstream file;
        file.open(options.path, ios::in);
        char *data;

        sendlen = build_wrq_packet(sendbuf, get_filename(options.path), options.mode);

        do
        {
            if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "Sending write request packet error: " << strerror(errno) << " Sending packet again.\n";
                retries++;
                errorflag = true;
                continue;
            }
            else
            {
                cout << timestamp() << "Sent write request with the size of " << sentlen << " bytes to " << options.ip << ':' << options.port << endl;
            }

            if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                retries++;
                cout << timestamp() << "Connection timed out, sending last packet again.\n";
                errorflag = true;
                continue;
            }

            getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
            strcpy(server_TID, server_port);

            if (ntohs(*(short *)recvbuf) == OP_ERROR)
            {
                cout << timestamp() << "Error: " << recvbuf + 4 << endl;
                return 0; //TODO:END OP, NOT PROGRAM
            }
            else if (ntohs(*(short *)recvbuf) == OP_ACK)
            {
                if (!handle_ack_packet(recvbuf, block_no))
                {
                    if (retries >= MAX_RETRIES)
                    {
                        cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                        return 0; //TODO:END OP, NOT PROGRAM
                    }
                    retries++;
                    cout << timestamp() << "Unexpected block number, sending last packet again.\n";
                    errorflag = true;
                    continue;
                }
                else
                {
                    cout << timestamp() << "Recieved acknowledgement packet with block number 0.\n";
                }
            }
            else
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    return 0; //TODO:END OP, NOT PROGRAM
                }
                retries++;
                errorflag = true;
                cout << timestamp() << "Recieved unexpected packet type, sending request packet again.\n";
                continue;
            }

            retries = 0;
            errorflag = false;
        } while (errorflag);

        do
        {
            if (!errorflag)
            {
                block_no++;

                file.read(data, options.size);

                sendlen = build_data_packet(sendbuf, block_no, data);
            }
            sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen);

            if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "Connection timed out, sending last packet again.\n";
                errorflag = true;
                continue;
            }

            getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
            if (!strcmp(server_port, server_TID))
            {
                if (retries >= MAX_RETRIES)
                {
                    cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                    break;
                }
                cout << timestamp() << "TID's don't match, sending error packet and last sent packet.\n";
                errlen = build_error_packet(errbuff, 5, "Unknown TID.\n");
                sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                errorflag = true;
                continue;
            }

            if (ntohs(*(short *)recvbuf) == OP_ERROR)
            {
                cout << timestamp() << "Error: " << recvbuf + 4 << endl;
                freeaddrinfo(server_servinfo);
                break;
            }
            else if (ntohs(*(short *)recvbuf) == OP_ACK)
            {
                //handle ack packet and continue
                if (!handle_ack_packet(recvbuf, block_no))
                {
                    if (retries >= MAX_RETRIES)
                    {
                        cout << timestamp() << "Operation was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                        break;
                    }
                    retries++;
                    cout << timestamp() << "Unexpected block number, sending last packet again.\n";
                    errorflag = true;
                    continue;
                }
                else
                {
                    cout << timestamp() << "Recieved acknowledgement packet with block number " << block_no << ".\n";
                }
            }
            else
            {
                break;
            }

        } while (sentlen = options.size + 4 || errorflag);

        file.close();
    }

    freeaddrinfo(server_servinfo);
    close(sockfd);
    return 0;
}
