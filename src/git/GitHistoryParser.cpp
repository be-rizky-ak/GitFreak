#include "GitHistoryParser.h"

#include <string>
#include <string_view>
#include <vector>

namespace
{

void trimLineEndings(
    std::string& value)
{
    std::size_t start = 0;

    // Remove leading CR/LF.
    while (start < value.size() &&
           (value[start] == '\r' ||
            value[start] == '\n'))
    {
        ++start;
    }

    std::size_t end =
        value.size();

    // Remove trailing CR/LF.
    while (end > start &&
           (value[end - 1] == '\r' ||
            value[end - 1] == '\n'))
    {
        --end;
    }

    if (start != 0 ||
        end != value.size())
    {
        value =
            value.substr(
                start,
                end - start);
    }
}

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

        // Git's pretty format leaves a newline
        // between records. That newline becomes
        // part of the next HASH field.
        //
        // Remove only CR/LF. Do not trim general
        // whitespace because spaces can be valid
        // content in fields such as the subject.
        std::string hash =
            std::string(fields[0]);

        std::string parents =
            std::string(fields[1]);

        std::string author =
            std::string(fields[2]);

        std::string email =
            std::string(fields[3]);

        std::string date =
            std::string(fields[4]);

        std::string subject =
            std::string(fields[5]);

        trimLineEndings(hash);
        trimLineEndings(parents);
        trimLineEndings(author);
        trimLineEndings(email);
        trimLineEndings(date);
        trimLineEndings(subject);

        commit.hash =
            std::move(hash);

        commit.parents =
            splitParents(parents);

        commit.author =
            std::move(author);

        commit.email =
            std::move(email);

        commit.date =
            std::move(date);

        commit.subject =
            std::move(subject);

        result.push_back(
            std::move(commit));

        fields.clear();
    }

    return result;
}