#pragma once

#include <iostream>
#include <arpa/inet.h>
#include <fstream>
#include <cstring>
#include <chrono>
#include "tftp_macros.h"





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
 * @param blocksize Blocksize option for file transfer
 * @param timeout Timeoute oprion for file transfer
 * @return Length of the packet
 */
int build_rrq_packet(char *buffer, string path, string mode,int blocksize, int timeout);

/**
 * @brief Build a write request packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param path Path to the file
 * @param mode Mode of the file transfer
 * @param blocksize Blocksize option for file transfer
 * @param timeout Timeoute oprion for file transfer
 * @return Length of the packet
 */
int build_wrq_packet(char *buffer, string path, string mode, int transfersize, int blocksize, int timeout);

/**
 * @brief Build a data packet
 * 
 * @param buffer Pointer to an array, where the packet will be built
 * @param block_no Block number of the packet
 * @param data Data to send
 * @return Length of the packet
 */
int build_data_packet(char *buffer, int block_no, char *data,int size);

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



/**
 * @brief Get packet type code from packet
 * 
 * @param packet Received packet
 * @return packet type code
 */
uint16_t get_packet_type_code(char *packet);

void handle_oack_packet(char *buffer,int buffsize, int requested_blocksize, int *blocksize, int requested_timeout, int *timeout);