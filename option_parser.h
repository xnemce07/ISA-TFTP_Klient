#pragma once

#include <iostream>
#include <getopt.h>
#include <regex>

using namespace std;
struct args
{
    bool read = false;
    bool write = false;
    string path = "";
    int timeout = 0;
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
void get_options(args *options);