/////////////////////////////////////////////////////////////////////////////
// Name:        filereader.h
// Author:      Laurent Pugin
// Created:     31/01/2024
// Copyright (c) Authors and others. All rights reserved.
/////////////////////////////////////////////////////////////////////////////

#ifndef __VRV_FILEREADER_H__
#define __VRV_FILEREADER_H__

#include <list>
#include <string>
#include <vector>

//----------------------------------------------------------------------------

/** Forward declaration of the zip_file.hpp class */
namespace miniz_cpp {
class zip_file;
}

//----------------------------------------------------------------------------

namespace vrv {

//----------------------------------------------------------------------------
// ZipFileReader
//----------------------------------------------------------------------------

/**
 * This class is a reader for zip archives.
 */
class ZipFileReader {
public:
    /**
     * @name Constructors, destructors, and other standard methods
     */
    ///@{
    ZipFileReader();
    ~ZipFileReader();
    ///@}

    /**
     * Reset a previously loaded file.
     */
    void Reset();

    /**
     * Load a file into memory.
     */
    bool Load(const std::string &filename);

    /**
     * Load a vector into memory
     */
    bool LoadBytes(const std::vector<unsigned char> &bytes);

    /**
     * Check if the archive contains the file
     */
    bool HasFile(const std::string &filename);

    /**
     * Read the text file.
     * Return an empty string if the file does not exist.
     */
    std::string ReadTextFile(const std::string &filename);

    /**
     * Return a list of all files (including directories)
     */
    std::list<std::string> GetFileList() const;

private:
    //
public:
    //
private:
    /** A pointer to the miniz zip file */
    miniz_cpp::zip_file *m_file;

}; // class ZipFileReader

//----------------------------------------------------------------------------
// ZipFileWriter
//----------------------------------------------------------------------------

/**
 * This class is a writer for zip archives.
 */
class ZipFileWriter {
public:
    /**
     * @name Constructors, destructors, and other standard methods
     */
    ///@{
    ZipFileWriter();
    ~ZipFileWriter();
    ///@}

    /**
     * Add a file with the given content to the archive.
     */
    void AddFile(const std::string &archivePath, const std::string &content);

    /**
     * Save the archive to a file.
     * Return false (and log an error) if the archive could not be saved.
     */
    bool Save(const std::string &filename);

    /**
     * Return the archive as a vector of bytes.
     * Return an empty vector (and log an error) if the archive could not be serialized.
     */
    std::vector<unsigned char> GetBytes();

private:
    //
public:
    //
private:
    /** A pointer to the miniz zip file */
    miniz_cpp::zip_file *m_file;

}; // class ZipFileWriter

} // namespace vrv

#endif // __VRV_FILEREADER_H__
