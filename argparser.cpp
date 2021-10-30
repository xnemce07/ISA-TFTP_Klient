#include "argparser.h"




bool argparser(args *options, int argc, char *argv[])
{
    int opt_val = 0;
    std::string adress = "";

    while ((opt_val = getopt(argc, argv, "RWd:t:s:a:c:m")) != -1)
    {
        switch (opt_val)
        {
        case 'R':
            options->read = true;
            break;
        case 'W':
            options->write = true;
            break;
        case 'd':
            options->path = optarg;
            break;
        case 't':
            if (!std::regex_match(optarg, std::regex("[0-9]*")))
            {
                std::cerr << "Timeout value must be a number\n";
                return false;
            }

            options->timeout = atoi(optarg);
            break;
        case 's':
            if (!std::regex_match(optarg, std::regex("[0-9]*")))
            {
                std::cerr << "Size value must be a number\n";
                return false;
            }

            options->size = atoi(optarg);
            break;
        case 'a':
            adress = optarg;
            break;
        case 'c':
            options->mode = optarg;
            break;
        case 'm':
            options->multicast = true;
            break;
        default:
            return false;
            break;
        }
    }

    if (adress.length() != 0)
    {
        options->ip = adress.substr(0, adress.find(','));
        options->port = adress.substr(adress.find(',') + 1, adress.length());
    }

    return true;
}

bool check_options(args *options)
{
    if (options->read == options->write)
    {
        std::cerr << "Choose exactly one of these options: -W -R.\n";
        return false;
    }

    if (options->timeout < 0)
    {
        std::cerr << "Tiemout cannot be 0 or negative.\n";
        return false;
    }

    if (options->size < 0)
    {
        std::cerr << "Packet size cannot be 0 or negative.\n";
        return false;
    }

    if (options->mode.compare("ascii") && options->mode.compare("netascii") && options->mode.compare("binary") && options->mode.compare("octet"))
    {
        std::cerr << "Invalid mode(-c) setting, choose ascii, netascii, binary or octet.\n" << options->mode << std::endl;
        return false;
    }


    return true;
}