#include "CliParser.hpp"

#include "Platform.hpp"
#include "Debug.hpp"

#include "math/MathUtil.hpp"

#if _WIN32
#include "io/UnicodeUtils.hpp"

#include <windows.h>
#endif

namespace Tungsten {

CliParser::CliParser(const std::string &programName, const std::string &usage)
: _programName(programName),
  _usage(usage)
{
}

void CliParser::wrapString(int width, int padding, const std::string &src) const
{
    width -= padding;
    std::string::size_type pos = 0;
    while (pos != std::string::npos) {
        std::string::size_type wrapPos = pos;
        while (true) {
            std::string::size_type next = src.find_first_of(' ', wrapPos + 1);
            if (next == std::string::npos || next - pos > unsigned(width))
                break;
            wrapPos = next;
        }
        if (src.size() <= pos + width || wrapPos == pos)
            wrapPos = min(src.size(), pos + width);
        if (pos > 0)
            std::cout.width(padding);
        std::cout << "" << src.substr(pos, wrapPos - pos) << '\n';

        pos = src.find_first_not_of(' ', wrapPos);
    }
}

std::vector<std::string> CliParser::retrieveUtf8Args(int argc, const char *argv[])
{
    std::vector<std::string> result;

#if _WIN32
    MARK_UNUSED(argc);
    MARK_UNUSED(argv);

    LPCWSTR args = GetCommandLineW();
    int numArgs;
    LPWSTR *splitArgs = CommandLineToArgvW(args, &numArgs);
    if (splitArgs) {
        for (int i = 0; i < numArgs; ++i)
            result.emplace_back(UnicodeUtils::wcharToUtf8(splitArgs[i]));
        LocalFree(splitArgs);
    }
#else
    for (int i = 0; i < argc; ++i)
        result.emplace_back(argv[i]);
#endif

    return std::move(result);
}

void CliParser::printHelpText(int maxWidth) const
{
    int longOptLength = 0;
    for (const CliOption &o : _options)
        longOptLength = max(longOptLength, int(o.longOpt.size()));
    longOptLength += 4;

    std::cout << "Usage: " << _programName << " " << _usage << "\nOptions:\n";
    for (const CliOption &o : _options) {
        if (o.shortOpt == '\0')
            std::cout << "     ";
        else
            std::cout << " -" << o.shortOpt << "  ";
        std::cout.width(longOptLength);
        std::cout << std::left;
        if (o.longOpt.empty())
            std::cout << "";
        else
            std::cout << ("--" + o.longOpt);
        std::cout << "  ";
        wrapString(maxWidth, 5 + longOptLength + 2, o.description);
    }
    std::cout.flush();
}

void CliParser::addOption(char shortOpt, const std::string &longOpt,
        const std::string &description, bool hasParam, int token)
{
    if (_tokenToOption.find(token) != _tokenToOption.end())
        FAIL("Duplicate token %i", token);
    if (shortOpt != '\0' && _shortOpts.find(shortOpt) != _shortOpts.end())
        FAIL("Duplicate short option %s", shortOpt);
    if (!longOpt.empty() && _longOpts.find(longOpt) != _longOpts.end())
        FAIL("Duplicate long option %s", longOpt);

    _tokenToOption.insert(std::make_pair(token, int(_options.size())));
    if (shortOpt != '\0')
        _shortOpts.insert(std::make_pair(shortOpt, int(_options.size())));
    if (!longOpt.empty())
        _longOpts.insert(std::make_pair(longOpt, int(_options.size())));

    _options.push_back(CliOption{
        shortOpt, longOpt, description, hasParam, token, "", false
    });
}

void CliParser::parse(int nativeArgc, const char *nativeArgv[])
{
    std::vector<std::string> argv = retrieveUtf8Args(nativeArgc, nativeArgv);
    int argc = int(argv.size());

    bool operandsOnly = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--") {
            operandsOnly = true;
            continue;
        }

        if (!operandsOnly && arg.size() > 1 && arg.front() == '-') {
            std::string param;
            std::string::size_type idx = arg.find_first_of('=');
            if (idx != std::string::npos) {
                param = arg.substr(idx + 1);
                arg.erase(idx);
            }

            if (arg[1] == '-') {
                std::string longOpt = arg.substr(2);
                auto iter = _longOpts.find(longOpt);
                if (iter == _longOpts.end())
                    fail("Unrecognized command line option %s", arg);

                CliOption &opt = _options[iter->second];

                if (opt.isPresent)
                    fail("Duplicate command line option %s", arg);
                if (opt.hasParam && param.empty() && i == argc - 1)
                    fail("Missing parameter for command line option %s", arg);

                if (opt.hasParam)
                    opt.param = param.empty() ? argv[++i] : param;
                opt.isPresent = true;
            } else {
                for (std::string::size_type j = 1; j < arg.size(); ++j) {
                    auto iter = _shortOpts.find(arg[j]);
                    if (iter == _shortOpts.end())
                        fail("Unrecognized command line option %s", arg);

                    CliOption &opt = _options[iter->second];
                    if (opt.isPresent)
                        fail("Duplicate command line option %s", arg);
                    if (opt.hasParam && ((param.empty() && i == argc - 1) || arg.size() > 2))
                        fail("Missing parameter for command line option %s", arg);

                    if (opt.hasParam)
                        opt.param = param.empty() ? argv[++i] : param;
                    opt.isPresent = true;
                }
            }
        } else {
            _operands.push_back(arg);
        }
    }
}

bool CliParser::isPresent(int token) const
{
    auto iter = _tokenToOption.find(token);
    if (iter == _tokenToOption.end())
        FAIL("Could not find option corresponding to token %i", token);

    return _options[iter->second].isPresent;
}

const std::string &CliParser::param(int token) const
{
    auto iter = _tokenToOption.find(token);
    if (iter == _tokenToOption.end())
        FAIL("Could not find option corresponding to token %i", token);

    return _options[iter->second].param;
}

}
