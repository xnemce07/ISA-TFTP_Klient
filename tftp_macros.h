#pragma once

#define MAX_RETRIES 1

#define OP_RRQ 1 /* TFTP op-codes */
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5
#define OP_OACK 6

#define MAXBUFLEN 1024
#define MAX_DATA_LEN 1020

#define PORTLEN 6

#define DEFAULT_TIMEOUT 5
#define DEFAULT_BLOCKSIZE 512
#define DEFAULT_MODE "octet"