#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <fstream>
#include <cstring>

#define OP_RRQ 1 /* TFTP op-codes */
#define OP_WRQ 2
#define OP_DATA 3
#define OP_ACK 4
#define OP_ERROR 5

#define MAXBUFLEN 1024
#define TIMEOUT 10

using namespace std;

/**
 * @brief Get the filename from path
 * 
 * @param path Path leading to the file
 * @return Filename
 */
string get_filename(string path);

/**
 * @brief Build a read request packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param path Path to the file
 * @param mode Mode of the file transfer
 * @return Length of the packet
 */
int build_rrq_packet(char *buffer, string path, string mode);

/**
 * @brief Build a write request packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param path Path to the file
 * @param mode Mode of the file transfer
 * @return Length of the packet
 */
int build_wrq_packet(char *buffer, string path, string mode);

/**
 * @brief Build a data packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param block_no Block number of the packet
 * @param data Data to send
 * @return Length of the packet
 */
int build_data_packet(char *buffer, int block_no, char *data);

/**
 * @brief Build an acknowledgement packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param block_no Block number of the acknowledged packet
 * @return Length of the packet
 */
int build_ack_packet(char *buffer, int block_no);

/**
 * @brief Build an error packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param error_code Error code
 * @param error_message Error message
 * @return Length of the packet
 */
int build_error_packet(char *buffer, int error_code, string error_message);

/**
 * @brief Handle an acknowledgement packet
 * 
 * @param buffer Array with the packet
 * @param expected_block_no Expected block number
 * @return true If block number of the packet matches expected block number
 * @return false If block numbers don't match
 */
bool handle_ack_packet(char *buffer, int expected_block_no);

/**
 * @brief Handle a data packet
 * 
 * @param buffer Array with the packet
 * @param file Filestream to write received data in
 * @param recvbytes Length of the packet
 * @param expected_block_no Expected block number
 * @return true If block number of the packet matches expected block number
 * @return false If block numbers don't match
 */
bool handle_data_packet(char *buffer, int expected_block_no, int recvbytes, ofstream *file);

/**
 * @brief Creates string with current time in format [YYYY-MM-DD hh:mm:ss.ms] + tab space
 * 
 * @return String with the timestamp
 */
string timestamp();