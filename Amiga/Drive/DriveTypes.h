// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

// This file must conform to standard ANSI-C to be compatible with Swift.

#ifndef _DRIVE_T_INC
#define _DRIVE_T_INC

typedef enum : long
{
    DRIVE_35_DD,
    DRIVE_35_DD_PC,
    DRIVE_35_HD,
    DRIVE_35_HD_PC,
    DRIVE_525_SD
}
DriveType;

inline bool isDriveType(long model)
{
    return model >= DRIVE_35_DD && model <= DRIVE_525_SD;
}

inline const char *driveTypeName(DriveType type)
{
    assert(isDriveType(type));
    
    switch (type) {
        case DRIVE_35_DD:    return "Drive 3.5\" DD";
        case DRIVE_35_DD_PC: return "Drive 3.5\" DD (PC)";
        case DRIVE_35_HD:    return "Drive 3.5\" HD";
        case DRIVE_35_HD_PC: return "Drive 3.5\" HD (PC)";
        case DRIVE_525_SD:   return "Drive 5.25\" SD";
        default:             return "???";
    }
}

typedef enum
{
    DRIVE_DMA_OFF,     // Drive is idle
    
    DRIVE_DMA_WAIT,    // Drive is waiting for the sync word
    DRIVE_DMA_READ,    // Drive is reading
    
    DRIVE_DMA_WRITE,   // Drive is writing
    DRIVE_DMA_FLUSH,   // Drive is finishing up the write process
}
DriveState;

#endif