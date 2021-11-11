#include "option_parser.h"

using namespace std;

/**
 * @brief Fills my_argv array with options, so it could be parsed by getopt()
 * 
 * @param line String with options
 * @param my_argv Array what will be filled out, needs to be already allocated to size from count_items
 */
void fill_argv(string line, char *my_argv[]);

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

/**
 * @brief Counts items divided by 1 or more whitespaces in string
 * 
 * @param str String with items
 * @return Count of items
 */
int count_items(string str);

bool argparser(args *options, int argc, char *argv[])
{
    int opt_val = 0;
    string adress = "";

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
            if (!regex_match(optarg, regex("[0-9]*")))
            {
                cerr << "Timeout value must be a number\n";
                return false;
            }

            options->timeout = atoi(optarg);
            break;
        case 's':
            if (!regex_match(optarg, regex("[0-9]*")))
            {
                cerr << "Size value must be a number\n";
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
        cout << "Choose exactly one of these options: -W -R.\n";
        return false;
    }

    if (options->timeout < 0)
    {
        cout << "Tiemout cannot be 0 or negative.\n";
        return false;
    }

    if (options->size < 0)
    {
        cout << "Packet size cannot be 0 or negative.\n";
        return false;
    }

    if (options->path.empty())
    {
        cout << "-d (path) is required.\n";
        return false;
    }

    if (options->mode.compare("ascii") && options->mode.compare("netascii") && options->mode.compare("binary") && options->mode.compare("octet"))
    {
        cout << "Invalid mode(-c) setting, choose ascii, netascii, binary or octet.\n"
             << options->mode << endl;
        return false;
    }

    return true;
}

//TODO: MAKE THIS BOOL AND ADD A QUIT COMMAND
void get_options(args *options)
{

    string input_line;
    bool errorflag = false;
    int item_count;
    char **myArgv;

    do
    {
        cout << '>';
        getline(cin, input_line);
        item_count = count_items(input_line);
        //char *myArgv[item_count + 1];
        myArgv = new char*[item_count + 1];

        fill_argv(input_line, myArgv);
    }while(!argparser(options, item_count + 1, myArgv) || !check_options(options));

    

}

int count_items(string str)
{
    bool whitespace_flag = true, quotation_flag = false;
    int counter = 0;

    for (size_t i = 0; i < str.length(); i++)
    {
        char current = str[i];
        if ((current == ' ' || current == '\t') && !whitespace_flag && !quotation_flag)
        {
            counter++;
            whitespace_flag = true;
        }
        else if (current == '\'' || current == '\"')
        {
            quotation_flag = !quotation_flag;
            whitespace_flag = false;
        }
        else if (current != ' ' && current != '\t')
        {
            whitespace_flag = false;
        }
    }

    if (whitespace_flag)
    {
        counter--;
    }

    return ++counter;
}

void fill_argv(string line, char *my_argv[])
{

    my_argv[0] = (char *)"path"; //First argument, that will be skipped by getopt()

    bool whitespace_flag = true, quotation_flag = false;
    int counter = 1;
    string word = "";

    for (size_t i = 0; i < line.length(); i++)
    {
        char current = line[i];
        if ((current == ' ' || current == '\t') && !whitespace_flag && !quotation_flag)
        {
            char *word_c = new char[word.length()];
            strcpy(word_c, word.c_str());
            my_argv[counter] = word_c;
            word = "";
            counter++;
            whitespace_flag = true;
        }
        else if (current == '\'' || current == '\"')
        {
            quotation_flag = !quotation_flag;
            whitespace_flag = false;
        }
        else if ((current != ' ' && current != '\t') || quotation_flag)
        {
            whitespace_flag = false;
            word += current;
        }
    }

    char *word_c = new char[word.length()];
    strcpy(word_c, word.c_str());
    my_argv[counter] = word_c;

    return;
}
