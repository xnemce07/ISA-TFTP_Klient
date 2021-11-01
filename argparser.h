#pragma once

#include <iostream>
#include <getopt.h>
#include <regex>

struct args
{
    bool read = false;
    bool write = false;
    std::string path = "";
    int timeout = 0;
    int size = 512;
    bool multicast = false;
    std::string mode = "binary";
    std::string ip = "127.0.0.1";
    std::string port = "69";
};

/**
 * @brief Load launch arguments into a structure
 * 
 * @param options This structure will be filled
 * @param argc Arguments count
 * @param argv Arguments
 * @return true On succes
 * @return false On failure
 */
bool argparser(args *options, int argc, char *argv[]);

/**
 * @brief Checks if options from arguments are valid
 * 
 * @param options Structure with arguments
 * @return true On options being valid
 * @return false On options being invalid
 */
bool check_options(args *options);