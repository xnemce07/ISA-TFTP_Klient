#include "tftp_lib.h"

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
    strcpy(buffer + len, path.c_str());
    len += path.length() + 1;
    strcpy(buffer + len, mode.c_str());
    len += mode.length() + 1;
    return len;
}

int build_wrq_packet(char *buffer, string path, string mode)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_WRQ);
    strcpy(buffer + len, get_filename(path).c_str());
    len += get_filename(path).length() + 1;
    strcpy(buffer + len, mode.c_str());
    len += mode.length() + 1;
    return len;
}

int build_data_packet(char *buffer, char *block_no, char *data)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_DATA);
    strcpy(buffer + len, block_no);
    len += strlen(block_no) + 1;
    strcpy(buffer + len, data);
    len += strlen(data);
    return len;
}

int build_ack_packet(char *buffer, char *block_no)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_ACK);
    strcpy(buffer + len, block_no);
    len += strlen(block_no) + 1;
    return len;
}

int build_error_packet(char *buffer, int error_code, string error_message)
{
    memset(buffer, 0, MAXBUFLEN);
    int len = 2;
    *(short *)buffer = htons(OP_ACK);
    *(short *)(buffer + len) = htons(error_code);
    len += 3;
    strcpy(buffer + len, error_message.c_str());
    len += error_message.length() + 1;

    return len;
}

bool handle_data_packet(char *buffer, int expected_block_no, int recvbytes, ofstream *file)
{
    char block_no[3];
    
    memcpy(block_no, buffer + 2, 2);
    block_no[2] = '\0';

    if (ntohs(*(short*)block_no) == expected_block_no)
    {
        (*file).write(buffer + 4, recvbytes - 4);
        
        return true;
    }
    else
    {
        cerr << "Block numbers don't match.\n";
        return false;
    }
}

bool handle_ack_packet(char *buffer, int expected_block_no)
{
    char block_no[3];
    memcpy(block_no, buffer + 2, 2);
    block_no[2] = '\0';

    return ntohs(*(short*)block_no) == expected_block_no;
}
