#include "tftp_lib.h"

#include <chrono>

using namespace std;

string get_filename(string path)
{
    return path.substr(path.find_last_of("/") + 1);
}

int build_rrq_packet(char *buffer, string path, string mode)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_RRQ);
    memcpy(buffer + len, path.c_str(), path.length());
    len += path.length() + 1;
    memcpy(buffer + len, mode.c_str(), mode.length());
    len += mode.length() + 1;
    return len;
}

int build_wrq_packet(char *buffer, string path, string mode)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_WRQ);
    memcpy(buffer + len, get_filename(path).c_str(), get_filename(path).length());
    len += get_filename(path).length() + 1;
    memcpy(buffer + len, mode.c_str(), mode.length());
    len += mode.length() + 1;
    return len;
}

int build_data_packet(char *buffer, int block_no, char *data, int size)
{

    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_DATA);
    *(short *)(buffer + 2) = htons(block_no);
    len += 2;
    memcpy(buffer + len, data, size);
    len += size;
    return len;
}

int build_ack_packet(char *buffer, int block_no)
{
    memset(buffer, 0, 5);

    *(short *)buffer = htons(OP_ACK);
    *(short *)(buffer + 2) = htons(block_no);

    return 4;
}

int build_error_packet(char *buffer, int error_code, string error_message)
{

    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_ERROR);
    *(short *)(buffer + len) = htons(error_code);
    len += 2;
    memcpy(buffer + len, error_message.c_str(), error_message.length());
    len += error_message.length() + 1;

    return len;
}

bool handle_data_packet(char *buffer, int expected_block_no, int recvbytes, ofstream *file)
{
    char block_no[3];

    memcpy(block_no, buffer + 2, 2);
    block_no[2] = '\0';

    if (ntohs(*(short *)block_no) == expected_block_no)
    {
        (*file).write(buffer + 4, recvbytes - 4);

        return true;
    }
    else
    {
        return false;
    }
}

bool handle_ack_packet(char *buffer, int expected_block_no)
{
    char block_no[3];
    memcpy(block_no, buffer + 2, 2);
    block_no[2] = '\0';

    return ntohs(*(short *)block_no) == expected_block_no;
}

//Source: https://stackoverflow.com/a/50923834
//Author: Enrico Pintus https://stackoverflow.com/users/7394372/enrico-pintus
string timestamp()
{
    auto currentTime = chrono::system_clock::now();
    char buffer[50];
    char millis_buffer[10];

    auto transformed = currentTime.time_since_epoch().count() / 1000000;

    auto millis = transformed % 1000;

    time_t tt;
    tt = chrono::system_clock::to_time_t(currentTime);
    auto timeinfo = localtime(&tt);
    strftime(buffer, 50, "[%F %H:%M:%S.", timeinfo);
    sprintf(millis_buffer,"%03d",(int)millis);
    //sprintf(buffer, "%s.%3d]\t", buffer, (int)millis);
    strcat(buffer,millis_buffer);
    strcat(buffer,"]\t");

    return string(buffer);
}

uint16_t get_packet_type_code(char *packet)
{
    return ntohs(*(short *)packet);
}