#include "GitRemoteParser.h"

#include <algorithm>
#include <string>

namespace
{

struct RemoteEntry
{
    std::string name;
    std::string url;
    bool fetch = false;
    bool push = false;
};

}

std::vector<Remote>
GitRemoteParser::parse(
    std::string_view output)
{
    std::vector<Remote> result;

    std::size_t position = 0;

    while (position < output.size())
    {
        std::size_t end =
            output.find('\n', position);

        if (end == std::string_view::npos)
        {
            end = output.size();
        }

        std::string_view line =
            output.substr(
                position,
                end - position);

        if (!line.empty() &&
            line.back() == '\r')
        {
            line.remove_suffix(1);
        }

        // Find first whitespace.
        std::size_t nameEnd =
            line.find_first_of(" \t");

        if (nameEnd == std::string_view::npos)
        {
            position =
                end < output.size()
                    ? end + 1
                    : end;

            continue;
        }

        std::string_view name =
            line.substr(
                0,
                nameEnd);

        // Skip whitespace.
        std::size_t urlStart =
            line.find_first_not_of(
                " \t",
                nameEnd);

        if (urlStart == std::string_view::npos)
        {
            position =
                end < output.size()
                    ? end + 1
                    : end;

            continue;
        }

        std::size_t urlEnd =
            line.find_first_of(
                " \t",
                urlStart);

        if (urlEnd == std::string_view::npos)
        {
            position =
                end < output.size()
                    ? end + 1
                    : end;

            continue;
        }

        std::string_view url =
            line.substr(
                urlStart,
                urlEnd - urlStart);

        std::size_t typeStart =
            line.find_first_not_of(
                " \t",
                urlEnd);

        if (typeStart == std::string_view::npos)
        {
            position =
                end < output.size()
                    ? end + 1
                    : end;

            continue;
        }

        std::string_view type =
            line.substr(typeStart);

        bool isFetch =
            type == "(fetch)";

        bool isPush =
            type == "(push)";

        if (!isFetch && !isPush)
        {
            position =
                end < output.size()
                    ? end + 1
                    : end;

            continue;
        }

        // Find an existing remote.
        auto it =
            std::find_if(
                result.begin(),
                result.end(),
                [&](const Remote& remote)
                {
                    return remote.name == name;
                });

        if (it == result.end())
        {
            Remote remote;

            remote.name =
                std::string(name);

            if (isFetch)
            {
                remote.fetchUrl =
                    std::string(url);
            }
            else
            {
                remote.pushUrl =
                    std::string(url);
            }

            result.push_back(
                std::move(remote));
        }
        else
        {
            if (isFetch)
            {
                it->fetchUrl =
                    std::string(url);
            }
            else
            {
                it->pushUrl =
                    std::string(url);
            }
        }

        position =
            end < output.size()
                ? end + 1
                : end;
    }

    return result;
}