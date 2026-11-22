#pragma once

#include <filesystem>
#include <fstream>

// Copy only file contents so read-only source attributes do not affect staging.
inline bool StageProblemUpload(const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    std::error_code error;
    const bool destinationExists = std::filesystem::exists(destination, error);
    if (error) return false;
    if (destinationExists) {
        const bool sameFile = std::filesystem::equivalent(source, destination, error);
        if (error || sameFile) return false;
    }

    std::ifstream input(source, std::ios::binary);
    if (!input) return false;
    std::ofstream output(destination, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    char buffer[65536];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() != 0) {
        output.write(buffer, input.gcount());
        if (!output) return false;
    }
    const bool readSucceeded = input.eof() && !input.bad();
    output.close();
    return readSucceeded && !output.fail();
}
