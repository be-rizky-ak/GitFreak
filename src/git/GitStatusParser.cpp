#include "GitStatusParser.h"

#include <cstdlib>
#include <string>

namespace
{

bool startsWith(
    std::string_view value,
    std::string_view prefix)
{
    return value.size() >= prefix.size() &&
           value.substr(0, prefix.size()) == prefix;
}

FileStatus statusFromXY(
    char indexStatus,
    char workTreeStatus)
{
    if (indexStatus == 'U' ||
        workTreeStatus == 'U')
    {
        return FileStatus::Conflicted;
    }

    if (indexStatus == 'A')
    {
        return FileStatus::Added;
    }

    if (indexStatus == 'D' ||
        workTreeStatus == 'D')
    {
        return FileStatus::Deleted;
    }

    return FileStatus::Modified;
}

void parseBranchHeader(
    std::string_view entry,
    RepositoryStatus& result)
{
    if (startsWith(entry, "# branch.head "))
    {
        result.branch =
            std::string(entry.substr(14));

        result.detached =
            result.branch == "(detached)";

        return;
    }

    if (startsWith(entry, "# branch.oid "))
    {
        result.head =
            std::string(entry.substr(13));

        return;
    }

    if (startsWith(entry, "# branch.upstream "))
    {
        result.upstream =
            std::string(entry.substr(18));

        return;
    }

    if (startsWith(entry, "# branch.ab "))
    {
        std::string_view value =
            entry.substr(12);

        std::size_t separator =
            value.find(' ');

        if (separator == std::string_view::npos)
        {
            return;
        }

        if (separator > 1 &&
            value[0] == '+')
        {
            result.ahead =
                std::atoi(
                    std::string(
                        value.substr(
                            1,
                            separator - 1))
                        .c_str());
        }

        if (separator + 2 < value.size() &&
            value[separator + 1] == '-')
        {
            result.behind =
                std::atoi(
                    std::string(
                        value.substr(
                            separator + 2))
                        .c_str());
        }
    }
}

bool findPathAfterFields(
    std::string_view entry,
    std::size_t position,
    int spacesToSkip,
    std::string_view& path)
{
    for (std::size_t i = position;
         i < entry.size() &&
         spacesToSkip > 0;
         ++i)
    {
        if (entry[i] == ' ')
        {
            --spacesToSkip;

            if (spacesToSkip == 0)
            {
                position = i + 1;
                break;
            }
        }
    }

    if (spacesToSkip != 0 ||
        position >= entry.size())
    {
        return false;
    }

    path = entry.substr(position);

    return true;
}

void parseOrdinaryEntry(
    std::string_view entry,
    RepositoryStatus& result)
{
    // 1 XY SUB M1 M2 M3 M4 OID1 OID2 PATH

    if (entry.size() < 4 ||
        entry[0] != '1' ||
        entry[1] != ' ')
    {
        return;
    }

    std::size_t position = 2;

    if (position + 2 > entry.size())
    {
        return;
    }

    const char indexStatus =
        entry[position];

    const char workTreeStatus =
        entry[position + 1];

    if (position + 2 >= entry.size() ||
        entry[position + 2] != ' ')
    {
        return;
    }

    ++position;

    std::string_view path;

    // After XY:
    //
    // SUB M1 M2 M3 M4 OID1 OID2
    //
    // Seven fields separated by seven spaces.
    if (!findPathAfterFields(
            entry,
            position,
            7,
            path))
    {
        return;
    }

    ChangedFile file;

    file.path =
        std::filesystem::path(path);

    file.staged =
        indexStatus != '.';

    file.unstaged =
        workTreeStatus != '.';

    file.status =
        statusFromXY(
            indexStatus,
            workTreeStatus);

    result.files.push_back(
        std::move(file));
}

void parseUntrackedEntry(
    std::string_view entry,
    RepositoryStatus& result)
{
    // ? PATH

    if (entry.size() < 3 ||
        entry[0] != '?' ||
        entry[1] != ' ')
    {
        return;
    }

    ChangedFile file;

    file.path =
        std::filesystem::path(
            entry.substr(2));

    file.status =
        FileStatus::Untracked;

    file.unstaged = true;

    result.files.push_back(
        std::move(file));
}

void parseUnmergedEntry(
    std::string_view entry,
    RepositoryStatus& result)
{
    // u XY SUB M1 M2 M3 M4 OID1 OID2 OID3 PATH

    if (entry.size() < 4 ||
        entry[0] != 'u' ||
        entry[1] != ' ')
    {
        return;
    }

    std::size_t position = 2;

    if (position + 2 > entry.size())
    {
        return;
    }

    if (entry[position + 2] != ' ')
    {
        return;
    }

    ++position;

    std::string_view path;

    // After XY:
    //
    // SUB M1 M2 M3 M4 OID1 OID2 OID3
    //
    // Eight fields.
    if (!findPathAfterFields(
            entry,
            position,
            8,
            path))
    {
        return;
    }

    ChangedFile file;

    file.path =
        std::filesystem::path(path);

    file.status =
        FileStatus::Conflicted;

    file.staged = true;
    file.unstaged = true;

    result.files.push_back(
        std::move(file));
}

bool parseRenameOrCopyEntry(
    std::string_view entry,
    std::string_view originalPath,
    RepositoryStatus& result)
{
    // 2 XY SUB M1 M2 M3 M4 OID1 OID2 Xscore PATH

    if (entry.size() < 4 ||
        entry[0] != '2' ||
        entry[1] != ' ')
    {
        return false;
    }

    std::size_t position = 2;

    if (position + 2 > entry.size())
    {
        return false;
    }

    const char indexStatus =
        entry[position];

    const char workTreeStatus =
        entry[position + 1];

    if (entry[position + 2] != ' ')
    {
        return false;
    }

    ++position;

    std::string_view path;

    // After XY:
    //
    // SUB
    // M1
    // M2
    // M3
    // M4
    // OID1
    // OID2
    // Xscore
    //
    // Eight fields separated by eight spaces.
    if (!findPathAfterFields(
            entry,
            position,
            8,
            path))
    {
        return false;
    }

    ChangedFile file;

    file.path =
        std::filesystem::path(path);

    file.originalPath =
        std::filesystem::path(originalPath);

    file.staged =
        indexStatus != '.';

    file.unstaged =
        workTreeStatus != '.';

    if (indexStatus == 'C')
    {
        file.status =
            FileStatus::Copied;
    }
    else
    {
        file.status =
            FileStatus::Renamed;
    }

    result.files.push_back(
        std::move(file));

    return true;
}

} // namespace

RepositoryStatus GitStatusParser::parse(
    std::string_view output)
{
    RepositoryStatus result;

    std::size_t position = 0;

    while (position < output.size())
    {
        std::size_t end =
            output.find('\0', position);

        if (end == std::string_view::npos)
        {
            end = output.size();
        }

        std::string_view entry =
            output.substr(
                position,
                end - position);

        position =
            end < output.size()
                ? end + 1
                : end;

        if (entry.empty())
        {
            continue;
        }

        // Branch/header information.
        if (entry[0] == '#')
        {
            parseBranchHeader(
                entry,
                result);

            continue;
        }

        // Untracked.
        if (entry[0] == '?')
        {
            parseUntrackedEntry(
                entry,
                result);

            continue;
        }

        // Ordinary tracked entry.
        if (entry[0] == '1')
        {
            parseOrdinaryEntry(
                entry,
                result);

            continue;
        }

        // Rename / copy.
        //
        // With -z:
        //
        // 2 ... destination\0original\0
        //
        // Consume the original path here.
        if (entry[0] == '2')
        {
            if (position >= output.size())
            {
                continue;
            }

            std::size_t originalEnd =
                output.find('\0', position);

            if (originalEnd == std::string_view::npos)
            {
                originalEnd = output.size();
            }

            std::string_view originalPath =
                output.substr(
                    position,
                    originalEnd - position);

            position =
                originalEnd < output.size()
                    ? originalEnd + 1
                    : originalEnd;

            parseRenameOrCopyEntry(
                entry,
                originalPath,
                result);

            continue;
        }

        // Unmerged / conflicted.
        if (entry[0] == 'u')
        {
            parseUnmergedEntry(
                entry,
                result);

            continue;
        }
    }

    return result;
}