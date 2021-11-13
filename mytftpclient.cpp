#include <iostream>
#include <unistd.h>
#include <fstream>

#include "options_parser.h"
#include "tftp_lib.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define MAX_RETRIES 1
#define PORTLEN 6

int main()
{
    using namespace std;

    args *options = new (args);
    //bool quit_flag = false;

    while (true)
    {

        if (!get_options(options))
        {
            break;
        }

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

        if ((rv = getaddrinfo(NULL, "0", &client_hints, &client_servinfo)) != 0)
        {
            cerr << "Client getaddrinfo: " << gai_strerror(rv) << endl;
            return 1;
        }

        //SERVER getaddrinfo()
        memset(&server_hints, 0, sizeof(client_hints));
        server_hints.ai_family = AF_UNSPEC;
        server_hints.ai_socktype = SOCK_DGRAM;

        if ((rv = getaddrinfo(options->ip.c_str(), options->port.c_str(), &server_hints, &server_servinfo)) != 0)
        {
            cerr << "Server getaddrinfo: " << gai_strerror(rv) << endl;
            return 1;
        }

        for (p_client = client_servinfo; p_client != NULL; p_client = p_client->ai_next)
        {
            for (p_server = server_servinfo; p_server != NULL; p_server = p_server->ai_next)
            {

                if (p_server->ai_family != p_client->ai_family)
                {
                    continue;
                }

                if ((sockfd = socket(p_client->ai_family, p_client->ai_socktype, p_client->ai_protocol)) == -1)
                {
                    continue;
                }

                //TODO: delete this
                // if (bind(sockfd, p_client->ai_addr, p_client->ai_addrlen))
                // {
                //     close(sockfd);
                //     continue;
                // }

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

        if (options->timeout > 0)
        {
            struct timeval timeout;
            timeout.tv_sec = options->timeout;
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
        char server_IP[INET6_ADDRSTRLEN], server_port[PORTLEN];
        char server_TID[PORTLEN] = "0";
        bool errorflag = false;
        short retries = 0;

        //************************************READING FROM SERVER*************************************//
        if (options->read)
        {
            ofstream file;
            file.open(get_filename(options->path), ios::out | ios::trunc);

            //**********************************SENDING WRITE REQUEST**************************//
            sendlen = build_rrq_packet(sendbuf, options->path, options->mode);
            if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
            {
                cerr << timestamp() << "Sending read request packet error: " << strerror(errno) << " Attempting to send packet again\n";
                block_no = 1;
                errorflag = true;
            }
            else
            {
                
                cout << timestamp() << "Sent read request to server " << options->ip << ':' << options->port << endl;
            }

            do
            {
                //************SENDING LAST PACKET AGAIN ON ERROR*******************//
                if (errorflag) //On error, last packet is sent again
                {
                    retries++;
                    if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
                    {
                        errorflag = true;
                        if (retries >= MAX_RETRIES)
                        {
                            cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                            break;
                        }
                        cerr << timestamp() << "Resending last packet error: " << strerror(errno) << endl;
                        continue;
                    }
                    else
                    {
                        cout << timestamp() << "Sent last packet with a size of" << sentlen << " bytes again" << endl;
                        errorflag = false;
                    }
                }
                else //Block number is increased only if we are sending new packet
                {
                    block_no++;
                }

                /********************RECIEVING AND HANDLING FIRST ACK PACKET*****************/

                if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    cerr << timestamp() << "Connection timed out, sending last packet again\n";
                    continue;
                }

                //Checking TID
                getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);

                if (!strcmp(server_TID, "0")) //If server_TID is 0, no TID was chosen before (This is the first ACK packet received), so we set it's value
                {
                    strcpy(server_TID, server_port);
                }
                else if ((strcmp(server_port, server_TID))) //TID doesn't match server port
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    cerr << timestamp() << "Received packet with unexpected TID, sending last packet again and error packet\n";
                    errlen = build_error_packet(errbuff, 5, "Unexpected TID");
                    sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                    continue;
                }

                //Looking at type of received packet
                if (get_packet_type_code(recvbuf) == OP_ERROR) //Handling of error packet
                {
                    cout << timestamp() << "Error packet: " << recvbuf + 4 << endl;
                    errorflag = true;
                    break;
                }
                else if (get_packet_type_code(recvbuf) == OP_DATA) //Handling of data packet
                {
                    if (handle_data_packet(recvbuf, block_no, recvlen, &file))
                    {

                        cout << timestamp() << "Received data packet with a size of " << recvlen - 4 << " bytes\n";

                        sendlen = build_ack_packet(sendbuf, block_no);
                        if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, (struct sockaddr *)&server_addr, server_addrlen)) < 0)
                        {
                            errorflag = true;
                            if (retries >= MAX_RETRIES)
                            {
                                cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                                break;
                            }
                            cerr << timestamp() << "Sending acknowledgment packet error: " << strerror(errno) << " Sending packet again\n";
                            continue;
                        }
                        else
                        {
                            cout << timestamp() << "Sent acknowledgement packet with block number " << block_no << endl;
                        }
                    }
                    else
                    {
                        errorflag = true;
                        if (retries >= MAX_RETRIES)
                        {
                            cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                            break;
                        }
                        cerr << timestamp() << "Recieved packet with unexpected block number, sending last packet again.\n";
                        continue;
                    }
                }
                else //While reading from server, no other packet type is expected
                {

                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    cerr << timestamp() << "Received packet with unexpected packet type, sending last packet again\n";
                    continue;
                }

                retries = 0; //When this point is reached, any unsuccessful operation had to be completed, so we reset the retries counter
            } while (recvlen == options->size + 4 || errorflag);

            if (!errorflag)
            {
                cout << timestamp() << "Transfer completed\n";
            }
            file.close();
        }
        else //**************************************************WRITING TO SERVER********************************************************************//
        {
            ifstream file;
            file.open(options->path, ios::in | ios::binary);
            char data[MAX_DATA_LEN];

            sendlen = build_wrq_packet(sendbuf, get_filename(options->path), options->mode);

            //******************************ESTABLISHING CONNECTION*************************//

            do //do-while fo repeating request if something goes bad
            {
                //******************SENDING REQUEST PACKET******************************//
                if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, p_server->ai_addr, p_server->ai_addrlen)) < 0)
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    cerr << timestamp() << "Sending write request packet error: " << strerror(errno) << " Sending packet again\n";
                    retries++;
                    continue;
                }
                else
                {
                    
                    cout << timestamp() << "Sent write request to server " << options->ip << ':' << options->port << endl;
                }

                //********************RECIEVING AND HANDLING FIRST ACK PACKET*****************//
                if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Connection timed out, sending last packet again\n";
                    continue;
                }

                //Getting TID
                getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
                strcpy(server_TID, server_port);

                //Looking at type of packet
                if (ntohs(*(short *)recvbuf) == OP_ERROR) //Error packet
                {
                    cout << timestamp() << "Error packet: " << recvbuf + 4 << endl;
                    errorflag = true;
                    break;
                }
                else if (ntohs(*(short *)recvbuf) == OP_ACK) //ACK Packet
                {
                    if (!handle_ack_packet(recvbuf, block_no))
                    {
                        errorflag = true;
                        if (retries >= MAX_RETRIES)
                        {
                            cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                            break;
                        }
                        retries++;
                        cerr << timestamp() << "Unexpected block number, sending last packet again\n";
                        continue;
                    }
                    else
                    {
                        cout << timestamp() << "Recieved acknowledgement packet with block number 0\n";
                    }
                }
                else //Other packet types are not expected -> error
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Recieved packet with unexpected packet type, sending request packet again\n";
                    continue;
                }

                retries = 0;
                errorflag = false;
            } while (errorflag);

            if (errorflag) //If previous part ended with error, dont continue, free resources and continue
            {
                file.close();
                freeaddrinfo(server_servinfo);
                close(sockfd);
                continue;
            }

            //*********************************SENDING DATA LOOP****************************//

            do
            {
                //***********************************SENDING DATA PACKET*******************************//
                if (!errorflag) //On error, last packet will be sent, so we wont be incrementing block number, reading data from file ,or creating another data packet
                {
                    block_no++;

                    memset(data, 0, MAX_DATA_LEN);
                    file.read(data, options->size);

                    sendlen = build_data_packet(sendbuf, block_no, data, strlen(data));
                }

                if ((sentlen = sendto(sockfd, sendbuf, sendlen, 0, (struct sockaddr *)&server_addr, server_addrlen)) < 0)
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Sending of data packet failed, sending packet again.\n";
                    continue;
                }
                else
                {
                    if (errorflag)
                    {
                        cout << timestamp() << "Sent last packet with a size of" << sentlen << " bytes again" << endl;
                    }
                    else
                    {
                        cout << "Sent data packet with a size of " << sentlen - 4 << " bytes\n";
                    }
                    errorflag = false;
                    retries = 0;
                }

                //********************RECIEVING AND HANDLING FIRST ACK PACKET*****************//
                if ((recvlen = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0, (struct sockaddr *)&server_addr, &server_addrlen)) < 0)
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Connection timed out, sending last packet again\n";
                    continue;
                }

                //Checking for correct TID
                getnameinfo((struct sockaddr *)&server_addr, server_addrlen, server_IP, sizeof(server_IP), server_port, sizeof(server_port), NI_NUMERICHOST | NI_NUMERICSERV);
                if (strcmp(server_port, server_TID)) //TID doesn't match
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Received packet with unexpected TID, sending last packet again\n";
                    errlen = build_error_packet(errbuff, 5, "Unexpected TID");
                    sendto(sockfd, errbuff, errlen, 0, p_server->ai_addr, p_server->ai_addrlen);
                    continue;
                }

                //Looking at type of the packet
                if (get_packet_type_code(recvbuf) == OP_ERROR) //Error packet
                {
                    cout << timestamp() << "Error packet: " << recvbuf + 4 << endl;
                    errorflag = true;
                    freeaddrinfo(server_servinfo);
                    break;
                }
                else if (get_packet_type_code(recvbuf) == OP_ACK) //ACK packet
                {
                    //Checking packet's block number
                    if (!handle_ack_packet(recvbuf, block_no)) //Block number is incorrect
                    {
                        errorflag = true;
                        if (retries >= MAX_RETRIES)
                        {
                            cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished\n";
                            break;
                        }
                        retries++;
                        cerr << timestamp() << "Received acknowledgement packet with unexpected block number, sending last packet again.\n";
                        continue;
                    }
                    else
                    {
                        cout << timestamp() << "Recieved acknowledgement packet with block number " << block_no << endl;
                    }
                }
                else //Any other packet type is unexpected
                {
                    errorflag = true;
                    if (retries >= MAX_RETRIES)
                    {
                        cerr << timestamp() << "Transfer was unsuccessful after " << MAX_RETRIES << " retries and will not be finished.\n";
                        break;
                    }
                    retries++;
                    cerr << timestamp() << "Received packet with unexpected packet type, sending last packet again.\n";
                    continue;
                }

            } while (sentlen == options->size + 4 || errorflag); //Loop continues until data size is less than selected and no packet is waiting to be sent again

            if (!errorflag)
            {
                cout << timestamp() << "Transfer completed\n";
            }
            file.close();
        }

        freeaddrinfo(server_servinfo);
        close(sockfd);
    }
    delete (options);
    return 0;
}
