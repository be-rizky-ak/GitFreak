#include "GitHistoryParser.h"

#include <string>
#include <vector>

namespace
{

std::vector<std::string> splitParents(
    std::string_view value)
{
    std::vector<std::string> result;

    std::size_t position = 0;

    while (position < value.size())
    {
        while (position < value.size() &&
               value[position] == ' ')
        {
            ++position;
        }

        if (position >= value.size())
        {
            break;
        }

        std::size_t end =
            value.find(' ', position);

        if (end == std::string_view::npos)
        {
            end = value.size();
        }

        result.emplace_back(
            value.substr(
                position,
                end - position));

        position = end;
    }

    return result;
}

}

std::vector<Commit> GitHistoryParser::parse(
    std::string_view output)
{
    std::vector<Commit> result;

    std::vector<std::string_view> fields;

    std::size_t position = 0;

    while (position < output.size())
    {
        std::size_t end =
            output.find('\0', position);

        if (end == std::string_view::npos)
        {
            break;
        }

        fields.push_back(
            output.substr(
                position,
                end - position));

        position = end + 1;

        // One commit consists of:
        //
        // HASH
        // PARENTS
        // AUTHOR
        // EMAIL
        // DATE
        // SUBJECT
        //
        // six NUL-separated fields.
        if (fields.size() != 6)
        {
            continue;
        }

        Commit commit;

        commit.hash =
            std::string(fields[0]);

        commit.parents =
            splitParents(fields[1]);

        commit.author =
            std::string(fields[2]);

        commit.email =
            std::string(fields[3]);

        commit.date =
            std::string(fields[4]);

        commit.subject =
            std::string(fields[5]);

        result.push_back(
            std::move(commit));

        fields.clear();
    }

    return result;
}