

#include <getopt.h>
#include <iostream>
#include <regex>
#include "argparser.h"





int main(int argc, char *argv[])
{
    args options;

    if (!argparser(&options, argc, argv))
    {
        std::cout << "UH OH ON ARGPARSE\n";
        return false;
    }

    if (!check_options(&options))
    {
        std::cout << "UH OH ON CHECKOPTIONS\n";
        return false;
    }

    std::cout << "PATH: " << options.path << std::endl;
    std::cout << "SIZE: " << options.size << std::endl;
    std::cout << "TIMEOUT: " << options.timeout << std::endl;
    std::cout << "MODE: " << options.mode << std::endl;
    std::cout << "IP: " << options.ip << std::endl;
    std::cout << "PORT: " << options.port << std::endl;

    return 0;
}

