// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "EXEFile.h"
#include "FSVolume.h"

EXEFile::EXEFile()
{
    setDescription("EXEFile");
}

bool
EXEFile::isEXEBuffer(const u8 *buffer, size_t length)
{
    u8 signature[] = { 0x00, 0x00, 0x03, 0xF3 };
                                                                                            
    assert(buffer != nullptr);
    
    return matchingBufferHeader(buffer, signature, sizeof(signature));
}

bool
EXEFile::isEXEFile(const char *path)
{
    u8 signature[] = { 0x00, 0x00, 0x03, 0xF3 };
    
    assert(path != nullptr);
    
    return matchingFileHeader(path, signature, sizeof(signature));
}

EXEFile *
EXEFile::makeWithBuffer(const u8 *buffer, size_t length)
{
    EXEFile *exe = new EXEFile();
    
    if (!exe->readFromBuffer(buffer, length)) {
        delete exe;
        return NULL;
    }
    
    return exe;
}

EXEFile *
EXEFile::makeWithFile(const char *path)
{
    EXEFile *exe = new EXEFile();
    
    if (!exe->readFromFile(path)) {
        delete exe;
        return NULL;
    }
    
    return exe;
}

bool
EXEFile::readFromBuffer(const u8 *buffer, size_t length)
{
    debug("readFromBuffer(%p, %lld)\n", buffer, length);
    
    bool success = false;
    
    if (!isEXEBuffer(buffer, length))
        return false;
        
    if (!AmigaFile::readFromBuffer(buffer, length))
        return false;
        
    // Create a new file system
    FSVolume volume = FSVolume("Disk");
    
    // Make the volume bootable
    volume.installBootBlock();
    
    // Add the exe file
    FSBlock *file = volume.makeFile("file");
    if (file) success = file->append(buffer, length);

    // Add a script directory
    volume.makeDir("s");
    volume.changeDir("s");
    volume.currentDirBlock()->printPath();
    
    // Add a startup sequence
    file = volume.makeFile("startup-sequence");
    if (success && file) success = file->append("file");
    
    // Convert the volume into an ADF
    assert(adf == nullptr);
    if (success) adf = ADFFile::makeWithVolume(volume);

    debug("adf = %p\n", adf); 
    volume.check(true);
    volume.dump();
    
    return adf != nullptr;
}
