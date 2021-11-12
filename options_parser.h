#pragma once

#include <iostream>
#include <getopt.h>
#include <regex>

#define DEFAULT_TIMEOUT 10

using namespace std;
struct args
{
    bool read = false;
    bool write = false;
    string path = "";
    int timeout = DEFAULT_TIMEOUT;
    int size = 512;
    bool multicast = false;
    string mode = "octet";
    string ip = "127.0.0.1";
    string port = "69";
};






/**
 * @brief Parses options from stdin, repeats until all everything is set correctly
 * 
 * @param options Args structure that will be filled with options
 */
bool get_options(args *options);