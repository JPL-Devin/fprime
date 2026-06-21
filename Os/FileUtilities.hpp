// ======================================================================
// \title  Os/FileUtilities.hpp
// \author devin
// \brief  Utility functions for filesystem path operations
//
// \copyright
// Copyright 2009-2026, by the California Institute of Technology.
// ALL RIGHTS RESERVED.  United States Government Sponsorship
// acknowledged.
//
// ======================================================================

#ifndef OS_FILE_UTILITIES_HPP
#define OS_FILE_UTILITIES_HPP

#include <Fw/FPrimeBasicTypes.hpp>

namespace Os {

namespace FileUtilities {

//! \brief Status codes for FileUtilities operations
enum Status {
    OP_OK,             //!< Operation succeeded
    INVALID_PATH,      //!< Path is null or empty
    BUFFER_TOO_SMALL,  //!< Output buffer is too small to hold the result
    ABOVE_BASE,        //!< Path traverses above the base (e.g., absolute path resolves above root)
    OTHER_ERROR,       //!< Other error
};

//! \brief Maximum number of path components supported during canonicalization
//!
//! This limits the depth of directory nesting that can be resolved.
//! Paths with more components than this will return OTHER_ERROR.
static constexpr FwSizeType MAX_PATH_COMPONENTS = 128;

//! \brief Canonicalize a path by resolving "." and ".." components and normalizing separators
//!
//! This is a purely lexical operation -- it does not access the filesystem or resolve symlinks.
//! It normalizes multiple consecutive separators, removes trailing separators, resolves "."
//! (current directory) and ".." (parent directory) references.
//!
//! For absolute paths (starting with "/"), attempting to traverse above the root returns ABOVE_BASE.
//! For relative paths, leading ".." components are preserved.
//!
//! Device prefixes (e.g., "cf:" on VxWorks) are preserved and not modified.
//!
//! It is invalid to pass nullptr for path or outputPath.
//! It is invalid to pass 0 for outputSize.
//!
//! \param path The input path to canonicalize
//! \param outputPath Buffer to store the canonicalized result (null-terminated)
//! \param outputSize Size of the output buffer in bytes (must include space for null terminator)
//! \return Status of the operation
Status canonicalize(const char* path, char* outputPath, FwSizeType outputSize);

//! \brief Check if a path is contained within a given directory after canonicalization
//!
//! Both the directory and the path are canonicalized before comparison. The function checks
//! that the canonicalized path starts with the canonicalized directory prefix, ensuring that
//! the path does not escape the directory via ".." or other traversal techniques.
//!
//! It is invalid to pass nullptr for directory or path.
//!
//! \param directory The sandbox/base directory path
//! \param path The path to check
//! \return OP_OK if path is within directory, INVALID_PATH if path escapes the directory,
//!         or another Status code on error
Status isWithinDirectory(const char* directory, const char* path);

}  // namespace FileUtilities

}  // namespace Os

#endif  // OS_FILE_UTILITIES_HPP
