// ======================================================================
// \title  Os/FileUtilities.cpp
// \author devin
// \brief  Implementation of filesystem path utility functions
//
// \copyright
// Copyright 2009-2026, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#include <Os/FileUtilities.hpp>
#include <Fw/Types/Assert.hpp>
#include <Fw/Types/StringUtils.hpp>
#include <cstring>

namespace Os {

namespace FileUtilities {

// --------------------------------------------------------------------------
// Internal helpers (file-local)
// --------------------------------------------------------------------------

//! \brief Find the length of a device prefix in a path (e.g., "cf:" on VxWorks)
//!
//! A device prefix is defined as one or more alphanumeric characters followed by a colon.
//! Returns the number of characters in the prefix including the colon, or 0 if none found.
static FwSizeType findDevicePrefix(const char* path, FwSizeType pathLen) {
    for (FwSizeType i = 0; i < pathLen; i++) {
        const char c = path[i];
        if (c == ':') {
            // Found colon — everything up to and including it is the device prefix
            return i + 1;
        }
        if (c == '/') {
            // Hit a separator before finding a colon — no device prefix
            return 0;
        }
        // Only allow alphanumeric characters in device names
        const bool isAlpha = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
        const bool isDigit = (c >= '0' && c <= '9');
        if (!isAlpha && !isDigit) {
            return 0;
        }
    }
    return 0;
}

//! \brief Write a canonical path from resolved components into the output buffer
//!
//! \param components Array of pointers to component start positions in the original path
//! \param lengths Array of component lengths
//! \param count Number of components
//! \param prefix Pointer to the path prefix (device prefix and/or leading slash)
//! \param prefixLen Length of the prefix
//! \param outputPath Output buffer
//! \param outputSize Size of output buffer
//! \return Status of the operation
static Status writeResult(const char* const components[],
                          const FwSizeType lengths[],
                          FwSizeType count,
                          const char* prefix,
                          FwSizeType prefixLen,
                          char* outputPath,
                          FwSizeType outputSize) {
    // Calculate required size: prefix + components joined by "/" + null terminator
    FwSizeType requiredSize = prefixLen;
    for (FwSizeType i = 0; i < count; i++) {
        if (i > 0) {
            requiredSize++;  // separator
        }
        requiredSize += lengths[i];
    }
    requiredSize++;  // null terminator

    if (requiredSize > outputSize) {
        return BUFFER_TOO_SMALL;
    }

    // Write prefix
    FwSizeType pos = 0;
    for (FwSizeType i = 0; i < prefixLen; i++) {
        outputPath[pos] = prefix[i];
        pos++;
    }

    // Write components separated by "/"
    for (FwSizeType i = 0; i < count; i++) {
        if (i > 0) {
            outputPath[pos] = '/';
            pos++;
        }
        (void)memcpy(&outputPath[pos], components[i], static_cast<size_t>(lengths[i]));
        pos += lengths[i];
    }

    // Handle empty result — produce "/" for absolute paths, "." for relative paths
    if (pos == 0) {
        if (outputSize < 2) {
            return BUFFER_TOO_SMALL;
        }
        outputPath[0] = '.';
        pos = 1;
    } else if (pos == prefixLen && prefixLen > 0 && count == 0) {
        // Prefix only with no components — this is fine (e.g., "/" or "cf:/")
    }

    outputPath[pos] = '\0';
    return OP_OK;
}

Status canonicalize(const char* path, char* outputPath, FwSizeType outputSize) {
    FW_ASSERT(path != nullptr);
    FW_ASSERT(outputPath != nullptr);
    FW_ASSERT(outputSize > 0);

    const FwSizeType pathLen = static_cast<FwSizeType>(
        Fw::StringUtils::string_length(path, static_cast<FwSizeType>(4096)));

    if (pathLen == 0) {
        return INVALID_PATH;
    }

    // Identify device prefix (e.g., "cf:" on VxWorks)
    const FwSizeType deviceLen = findDevicePrefix(path, pathLen);

    // Determine if path is absolute (starts with "/" after any device prefix)
    const bool isAbsolute = (deviceLen < pathLen) && (path[deviceLen] == '/');

    // Build the prefix string: device prefix + "/" if absolute
    // We store the prefix in a small stack buffer
    char prefixBuf[264];  // enough for any reasonable device prefix + "/"
    FwSizeType prefixLen = 0;

    // Copy device prefix
    for (FwSizeType i = 0; i < deviceLen; i++) {
        if (prefixLen < sizeof(prefixBuf) - 1) {
            prefixBuf[prefixLen] = path[i];
            prefixLen++;
        }
    }

    // Add leading "/" for absolute paths
    if (isAbsolute) {
        if (prefixLen < sizeof(prefixBuf) - 1) {
            prefixBuf[prefixLen] = '/';
            prefixLen++;
        }
    }
    prefixBuf[prefixLen] = '\0';

    // Parse path components after the prefix
    // We skip the device prefix and any leading "/" already accounted for
    FwSizeType startIdx = deviceLen;
    if (isAbsolute) {
        startIdx++;  // skip the leading "/"
    }

    // Stack of resolved component pointers and lengths
    const char* components[MAX_PATH_COMPONENTS];
    FwSizeType lengths[MAX_PATH_COMPONENTS];
    FwSizeType stackSize = 0;

    // Track leading ".." components for relative paths
    FwSizeType leadingDotDots = 0;

    // Scan through the path, splitting on "/"
    FwSizeType compStart = startIdx;
    for (FwSizeType i = startIdx; i <= pathLen; i++) {
        // Process a component when we hit a separator or end of string
        if (i == pathLen || path[i] == '/') {
            const FwSizeType compLen = i - compStart;

            if (compLen == 0 || (compLen == 1 && path[compStart] == '.')) {
                // Empty component (consecutive separators) or "." — skip
            } else if (compLen == 2 && path[compStart] == '.' && path[compStart + 1] == '.') {
                // ".." component
                if (isAbsolute) {
                    if (stackSize == 0) {
                        // Cannot go above root in absolute path
                        return ABOVE_BASE;
                    }
                    stackSize--;
                } else {
                    // For relative paths, pop if we can, otherwise preserve the ".."
                    if (stackSize > leadingDotDots) {
                        stackSize--;
                    } else {
                        // Preserve leading ".." in relative paths
                        if (stackSize >= MAX_PATH_COMPONENTS) {
                            return OTHER_ERROR;
                        }
                        components[stackSize] = &path[compStart];
                        lengths[stackSize] = compLen;
                        stackSize++;
                        leadingDotDots++;
                    }
                }
            } else {
                // Normal component — push onto stack
                if (stackSize >= MAX_PATH_COMPONENTS) {
                    return OTHER_ERROR;
                }
                components[stackSize] = &path[compStart];
                lengths[stackSize] = compLen;
                stackSize++;
            }

            compStart = i + 1;
        }
    }

    return writeResult(components, lengths, stackSize, prefixBuf, prefixLen, outputPath, outputSize);
}

Status isWithinDirectory(const char* directory, const char* path) {
    FW_ASSERT(directory != nullptr);
    FW_ASSERT(path != nullptr);

    // Canonicalize both paths
    char canonDir[1024];
    char canonPath[1024];

    Status status = canonicalize(directory, canonDir, sizeof(canonDir));
    if (status != OP_OK) {
        return status;
    }

    status = canonicalize(path, canonPath, sizeof(canonPath));
    if (status != OP_OK) {
        return status;
    }

    const FwSizeType dirLen = static_cast<FwSizeType>(
        Fw::StringUtils::string_length(canonDir, sizeof(canonDir)));
    const FwSizeType pathLen = static_cast<FwSizeType>(
        Fw::StringUtils::string_length(canonPath, sizeof(canonPath)));

    // Path must be at least as long as directory
    if (pathLen < dirLen) {
        return INVALID_PATH;
    }

    // Check that path starts with the directory prefix
    if (memcmp(canonDir, canonPath, static_cast<size_t>(dirLen)) != 0) {
        return INVALID_PATH;
    }

    // If path is longer than directory, the next character must be a separator
    // This prevents "/sandbox_evil" from matching "/sandbox"
    if (pathLen > dirLen) {
        if (canonPath[dirLen] != '/') {
            return INVALID_PATH;
        }
    }

    return OP_OK;
}

}  // namespace FileUtilities

}  // namespace Os
