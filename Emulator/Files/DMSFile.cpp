// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "DMSFile.h"

extern "C" {
unsigned short extractDMS(FILE *fi, FILE *fo);
}

DMSFile::DMSFile()
{
    setDescription("DMSFile");
}

bool
DMSFile::isDMSBuffer(const u8 *buffer, size_t length)
{
    u8 signature[] = { 'D', 'M', 'S', '!' };
                                                                                            
    assert(buffer != NULL);
    
    return matchingBufferHeader(buffer, signature, sizeof(signature));
}

bool
DMSFile::isDMSFile(const char *path)
{
    u8 signature[] = { 'D', 'M', 'S', '!' };
    
    assert(path != NULL);
    
    return matchingFileHeader(path, signature, sizeof(signature));
}

DMSFile *
DMSFile::makeWithBuffer(const u8 *buffer, size_t length)
{
    DMSFile *dms = new DMSFile();
    
    if (!dms->readFromBuffer(buffer, length)) {
        delete dms;
        return NULL;
    }
    
    return dms;
}

DMSFile *
DMSFile::makeWithFile(const char *path)
{
    DMSFile *dms = new DMSFile();
    
    if (!dms->readFromFile(path)) {
        delete dms;
        return NULL;
    }
    
    return dms;
}

bool
DMSFile::readFromBuffer(const u8 *buffer, size_t length)
{
    FILE *fpi, *fpo;
    
    if (!isDMSBuffer(buffer, length))
        return false;
        
    if (!AmigaFile::readFromBuffer(buffer, length))
        return false;
    
    // We use a third-party tool called xdms to convert the DMS file into an
    // ADF file. To communicate with xdms, we use temporary files.

    // Create input file
    fpi = fopen("/tmp/tmp.dms", "w");
    for (size_t i = 0; i < size; i++) putc(data[i], fpi);
    fclose(fpi);

    // Create output file
    fpi = fopen("/tmp/tmp.dms", "r");
    fpo = fopen("/tmp/tmp.adf", "w");
    extractDMS(fpi, fpo);
    fclose(fpi);
    fclose(fpo);
    
    // Create ADF
    fpo = fopen("/tmp/tmp.adf", "r");
    adf = ADFFile::makeWithFile(fpo);
    fclose(fpo);
    
    debug("adf = %p\n", adf);    
    return adf != NULL;
}
