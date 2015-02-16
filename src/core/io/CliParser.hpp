#ifndef CLIPARSER_HPP_
#define CLIPARSER_HPP_

#include <tinyformat/tinyformat.hpp>
#include <unordered_map>
#include <iostream>
#include <vector>
#include <string>

namespace Tungsten {

class CliParser
{
    struct CliOption
    {
        char shortOpt;
        std::string longOpt;
        std::string description;
        bool hasParam;
        int token;

        std::string param;
        bool isPresent;
    };

    std::string _programName, _usage;
    std::vector<CliOption> _options;
    std::unordered_map<int, int> _tokenToOption;
    std::unordered_map<char, int> _shortOpts;
    std::unordered_map<std::string, int> _longOpts;

    std::vector<std::string> _operands;

    void wrapString(int width, int padding, const std::string &src) const;

    std::vector<std::string> retrieveUtf8Args(int argc, const char *argv[]);

public:
    CliParser(const std::string &programName, const std::string &usage = "[options] [operands]");

    template<typename T1, typename... Ts>
    void fail(const char *fmt, const T1 &v1, const Ts &... ts)
    {
        std::cerr << _programName << ": ";
        tfm::format(std::cerr, fmt, v1, ts...);
        exit(-1);
    }

    void fail(const char *fmt)
    {
        std::cerr << _programName << ": " << fmt;
        exit(-1);
    }

    void printHelpText(int maxWidth = 80) const;

    void addOption(char shortOpt, const std::string &longOpt,
            const std::string &description, bool hasParam, int token);

    void parse(int argc, const char *argv[]);

    bool isPresent(int token) const;
    const std::string &param(int token) const;

    const std::vector<std::string> &operands() const
    {
        return _operands;
    }
};

}

#endif /* CLIPARSER_HPP_ */
