// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "Amiga.h"

Disk::Disk(DiskType type)
{
    setDescription("Disk");
    
    this->type = type;
    writeProtected = false;
    modified = false;
    clearDisk();
}

Disk *
Disk::makeWithFile(DiskFile *file)
{
    Disk *disk = new Disk(file->getDiskType());
    
    if (!disk->encodeDisk(file)) {
        delete disk;
        return NULL;
    }
    
    disk->fnv = file->fnv();
    
    return disk;
}

Disk *
Disk::makeWithReader(SerReader &reader, DiskType diskType)
{
    Disk *disk = new Disk(diskType);
    disk->applyToPersistentItems(reader);
    
    return disk;
}

u8
Disk::readByte(Cylinder cylinder, Side side, u16 offset)
{
    assert(cylinder < 84);
    assert(side < 2);
    assert(offset < trackSize);

    return data.cyclinder[cylinder][side][offset];
}

void
Disk::writeByte(u8 value, Cylinder cylinder, Side side, u16 offset)
{
    assert(cylinder < 84);
    assert(side < 2);
    assert(offset < trackSize);

    data.cyclinder[cylinder][side][offset] = value;
}

void
Disk::clearDisk()
{
    assert(sizeof(data) == sizeof(data.raw));

    srand(0);
    for (int i = 0; i < sizeof(data); i++)
        data.raw[i] = rand() & 0xFF;
    
    /* We are allowed to place random data here. In order to make some copy
     * protected game titles work, we smuggle in some magic values.
     * Crunch factory: Looks for 0x44A2 on cylinder 80
     */
    for (int t = 0; t < 2*84; t++) {
        data.track[t][0] = 0x44;
        data.track[t][1] = 0xA2;
    }
    
    fnv = 0;
}

void
Disk::clearTrack(Track t)
{
    assert(t < 168);

    srand(0);
    for (int i = 0; i < sizeof(data.track[t]); i++)
        data.track[t][i] = rand() & 0xFF;
}

void
Disk::clearTrack(Track t, u8 value)
{
    assert(t < 168);
    memset(data.track[t], value, trackSize);
}

bool
Disk::encodeDisk(DiskFile *df)
{
    assert(df != NULL);
    assert(df->getDiskType() == getType());

    // Start with an unformatted disk
    clearDisk();

    // Call the proper encoder for this disk
    return df->isAmigaDisk() ? encodeAmigaDisk(df) : encodeDosDisk(df);
}

bool
Disk::encodeAmigaDisk(DiskFile *df)
{
    long tracks = df->numTracks();
    
    trace("Encoding Amiga disk (%d tracks)\n", tracks);
    
    bool result = true;
    for (Track t = 0; t < tracks; t++) result &= encodeAmigaTrack(df, t);

    // Run the decoder in debug mode
    if (MFM_DEBUG) {
        debug("Amiga disk fully encoded (success = %d)\n", result);
        ADFFile *tmp = ADFFile::makeWithDisk(this);
        if (tmp) {
            msg("Decoded image written to /tmp/debug.adf\n");
            tmp->writeToFile("/tmp/tmp.adf");
        }
    }

    return result;
}

bool
Disk::encodeAmigaTrack(DiskFile *df, Track t)
{
    long sectors = df->numSectorsPerTrack();
    
    trace(MFM_DEBUG, "Encoding Amiga track %d (%d sectors)\n", t, sectors);

    // Format track
    clearTrack(t, 0xAA);

    // Encode all sectors
    bool result = true;
    for (Sector s = 0; s < sectors; s++) result &= encodeAmigaSector(df, t, s);
    
    // Rectify the first clock bit (where buffer wraps over)
    if (data.track[t][trackSize - 1] & 1) data.track[t][0] &= 0x7F;

    // Compute a debugging checksum
    if (MFM_DEBUG) {
        u64 check = fnv_1a_32(data.track[t], trackSize);
        debug("Track %d checksum = %x\n", t, check);
    }

    return result;
}

bool
Disk::encodeAmigaSector(DiskFile *df, Track t, Sector s)
{
    assert(t < 168);
    assert(s < 11);
    
    debug(MFM_DEBUG, "Encoding sector %d\n", s);
    
    /* Block header layout:
     *                     Start  Size   Value
     * Bytes before SYNC   00      4     0xAA 0xAA 0xAA 0xAA
     * SYNC mark           04      4     0x44 0x89 0x44 0x89
     * Track & sector info 08      8     Odd/Even encoded
     * Unused area         16     32     0xAA
     * Block checksum      48      8     Odd/Even encoded
     * Data checksum       56      8     Odd/Even encoded
     */
    
    u8 *p = data.track[t] + (s * sectorSize) + trackGapSize;
    
    // Bytes before SYNC
    p[0] = (p[-1] & 1) ? 0x2A : 0xAA;
    p[1] = 0xAA;
    p[2] = 0xAA;
    p[3] = 0xAA;
    
    // SYNC mark
    u16 sync = 0x4489;
    p[4] = HI_BYTE(sync);
    p[5] = LO_BYTE(sync);
    p[6] = HI_BYTE(sync);
    p[7] = LO_BYTE(sync);
    
    // Track and sector information
    u8 info[4] = { 0xFF, (u8)t, (u8)s, (u8)(11 - s) };
    encodeOddEven(&p[8], info, sizeof(info));
    
    // Unused area
    for (unsigned i = 16; i < 48; i++)
    p[i] = 0xAA;
    
    // Data
    u8 bytes[512];
    df->readSector(bytes, t, s);
    encodeOddEven(&p[64], bytes, sizeof(bytes));
    
    // Block checksum
    u8 bcheck[4] = { 0, 0, 0, 0 };
    for(unsigned i = 8; i < 48; i += 4) {
        bcheck[0] ^= p[i];
        bcheck[1] ^= p[i+1];
        bcheck[2] ^= p[i+2];
        bcheck[3] ^= p[i+3];
    }
    encodeOddEven(&p[48], bcheck, sizeof(bcheck));
    
    // Data checksum
    u8 dcheck[4] = { 0, 0, 0, 0 };
    for(unsigned i = 64; i < 1088; i += 4) {
        dcheck[0] ^= p[i];
        dcheck[1] ^= p[i+1];
        dcheck[2] ^= p[i+2];
        dcheck[3] ^= p[i+3];
    }
    encodeOddEven(&p[56], dcheck, sizeof(bcheck));
    
    // Add clock bits
    for(unsigned i = 8; i < 1088; i++) {
        p[i] = addClockBits(p[i], p[i-1]);
    }
    
    return true;
}

bool
Disk::encodeDosDisk(DiskFile *df)
{
    long tracks = df->numTracks();
    
    debug(MFM_DEBUG, "Encoding DOS disk (%d tracks)\n", tracks);
    
    bool result = true;
    for (Track t = 0; t < tracks; t++) result &= encodeDosTrack(df, t);
    
    // Run the decoder in debug mode
    if (MFM_DEBUG) {
        debug("DOS disk fully encoded (success = %d)\n", result);
        IMGFile *tmp = IMGFile::makeWithDisk(this);
        if (tmp) {
            msg("Decoded image written to /tmp/debug.img\n");
            tmp->writeToFile("/tmp/tmp.img");
        }
    }

    return result;
}

bool
Disk::encodeDosTrack(DiskFile *df, Track t)
{
    long sectors = df->numSectorsPerTrack();

    debug(MFM_DEBUG, "Encoding DOS track %d (%d sectors)\n", t, sectors);

    u8 *p = data.track[t];

    // Clear track
    for (int i = 0; i < trackSize; i += 2) { p[i] = 0x92; p[i+1] = 0x54; }

    // Encode track header
    p += 82;                                        // GAP
    for (int i = 0; i < 24; i++) { p[i] = 0xAA; }   // SYNC
    p += 24;
    p[0] = 0x52; p[1] = 0x24;                       // IAM
    p[2] = 0x52; p[3] = 0x24;
    p[4] = 0x52; p[5] = 0x24;
    p[6] = 0x55; p[7] = 0x52;
    p += 8;
    p += 80;                                        // GAP
        
    // Encode all sectors
    bool result = true;
    for (Sector s = 0; s < sectors; s++) result &= encodeDosSector(df, t, s);
    
    // Compute a checksum for debugging
    if (MFM_DEBUG) {
        u64 check = fnv_1a_32(data.track[t], trackSize);
        debug("Track %d checksum = %x\n", t, check);
    }

    return result;
}

bool
Disk::encodeDosSector(DiskFile *df, Track t, Sector s)
{
    u8 buf[60 + 512 + 2 + 109]; // Header + Data + CRC + Gap
        
    debug(MFM_DEBUG, "  Encoding DOS sector %d\n", s);
    
    // Write SYNC
    for (int i = 0; i < 12; i++) { buf[i] = 0x00; }
    
    // Write IDAM
    buf[12] = 0xA1;
    buf[13] = 0xA1;
    buf[14] = 0xA1;
    buf[15] = 0xFE;
    
    // Write CHRN
    buf[16] = (u8)(t / 2);
    buf[17] = (u8)(t % 2);
    buf[18] = (u8)(s + 1);
    buf[19] = 2;
    
    // Compute and write CRC
    u16 crc = crc16(&buf[12], 8);
    buf[20] = HI_BYTE(crc);
    buf[21] = LO_BYTE(crc);

    // Write GAP
    for (int i = 22; i < 44; i++) { buf[i] = 0x4E; }

    // Write SYNC
    for (int i = 44; i < 56; i++) { buf[i] = 0x00; }

    // Write DATA AM
    buf[56] = 0xA1;
    buf[57] = 0xA1;
    buf[58] = 0xA1;
    buf[59] = 0xFB;

    // Write DATA
    df->readSector(&buf[60], t, s);
    
    // Compute and write CRC
    crc = crc16(&buf[56], 516);
    buf[572] = HI_BYTE(crc);
    buf[573] = LO_BYTE(crc);

    // Write GAP
    for (int i = 574; i < sizeof(buf); i++) { buf[i] = 0x4E; }

    // Determine the start of this sector inside the current track
    u8 *p = data.track[t] + 194 + s * 1300;

    // Create the MFM data stream
    encodeMFM(p, buf, sizeof(buf));
    addClockBits(p, 2 * sizeof(buf));
    
    // Remove certain clock bits in IDAM block
    p[2*12+1] &= 0xDF;
    p[2*13+1] &= 0xDF;
    p[2*14+1] &= 0xDF;
    
    // Remove certain clock bits in DATA AM block
    p[2*56+1] &= 0xDF;
    p[2*57+1] &= 0xDF;
    p[2*58+1] &= 0xDF;

    return true;
}

bool
Disk::decodeAmigaDisk(u8 *dst, long numTracks, long numSectors)
{
    trace(MFM_DEBUG,
          "Decoding Amiga disk (%d tracks, %d sectors)\n", numTracks, numSectors);
    
    for (Track t = 0; t < numTracks; t++, dst += numSectors * 512) {
        if (!decodeAmigaTrack(dst, t, numSectors)) return false;
    }
    
    return true;
}

bool
Disk::decodeAmigaTrack(u8 *dst, Track t, long numSectors)
{
    assert(t < 168);
        
    trace(MFM_DEBUG, "Decoding track %d\n", t);
    
    // Create a local (double) copy of the track to simply the analysis
    u8 local[2 * trackSize];
    memcpy(local, data.track[t], trackSize);
    memcpy(local + trackSize, data.track[t], trackSize);
    
    // Seek all sync marks
    int sectorStart[numSectors], index = 0, nr = 0;
    while (index < trackSize + sectorSize && nr < numSectors) {

        // Scan MFM stream for $4489 $4489
        if (local[index++] != 0x44) continue;
        if (local[index++] != 0x89) continue;
        if (local[index++] != 0x44) continue;
        if (local[index++] != 0x89) continue;

        // Make sure it's not a DOS track
        if (local[index + 1] == 0x89) continue;

        sectorStart[nr++] = index;
    }
    
    if (nr != numSectors) {
        warn("Found %d sectors, expected %d. Aborting.\n", nr, numSectors);
        return false;
    }
    
    // Encode all sectors
    for (Sector s = 0; s < numSectors; s++) {
        decodeAmigaSector(dst, local + sectorStart[s]);
        dst += 512;
    }
    
    return true;
}

void
Disk::decodeAmigaSector(u8 *dst, u8 *src)
{
    assert(dst != NULL);
    assert(src != NULL);

    // Skip sector header
    src += 56;
    
    // Decode sector data
    decodeOddEven(dst, src, 512);
}

bool
Disk::decodeDOSDisk(u8 *dst, long numTracks, long numSectors)
{
    trace(MFM_DEBUG,
          "Decoding DOS disk (%d tracks, %d sectors)\n", numTracks, numSectors);
    
    for (Track t = 0; t < numTracks; t++, dst += numSectors * 512) {
        if (!decodeDOSTrack(dst, t, numSectors)) return false;
    }
    
    return true;
}

bool
Disk::decodeDOSTrack(u8 *dst, Track t, long numSectors)
{
    assert(t < 168);
        
    trace(MFM_DEBUG, "Decoding DOS track %d\n", t);
    
    // Create a local (double) copy of the track to simply the analysis
    u8 local[2 * trackSize];
    memcpy(local, data.track[t], trackSize);
    memcpy(local + trackSize, data.track[t], trackSize);
    
    // Determine the start of all sectors contained in this track
    int sectorStart[numSectors];
    for (int i = 0; i < numSectors; i++) {
        sectorStart[i] = 0;
    }
    int cnt = 0;
    for (int i = 0; i < 1.5 * trackSize;) {
        
        // Seek IDAM block
        if (local[i++] != 0x44) continue;
        if (local[i++] != 0x89) continue;
        if (local[i++] != 0x44) continue;
        if (local[i++] != 0x89) continue;
        if (local[i++] != 0x44) continue;
        if (local[i++] != 0x89) continue;
        if (local[i++] != 0x55) continue;
        if (local[i++] != 0x54) continue;

        // Decode CHRN block
        struct { u8 c; u8 h; u8 r; u8 n; } chrn;
        decodeMFM((u8 *)&chrn, &local[i], 4);
        trace(MFM_DEBUG, "c: %d h: %d r: %d n: %d\n", chrn.c, chrn.h, chrn.r, chrn.n);
        
        if (chrn.r >= 1 && chrn.r <= numSectors) {
            
            // Break the loop once we see the same sector twice
            if (sectorStart[chrn.r - 1] != 0) {
                break;
            }
            sectorStart[chrn.r - 1] = i + 88;
            cnt++;

        } else {
            warn("Invalid sector number %d. Aborting", chrn.r);
            return false;
        }
    }

    if (cnt != numSectors) {
        warn("Found %d sectors, expected %d. Aborting", cnt, numSectors);
        return false;
    }
        
    // Do some consistency checking
    for (int i = 0; i < numSectors; i++) assert(sectorStart[i] != 0);
    
    // Encode all sectors
    for (Sector s = 0; s < numSectors; s++) {
        decodeDOSSector(dst, local + sectorStart[s]);
        dst += 512;
    }
    
    return true;
}

void
Disk::decodeDOSSector(u8 *dst, u8 *src)
{
    assert(dst != NULL);
    assert(src != NULL);

    decodeMFM(dst, src, 512);
}

void
Disk::encodeMFM(u8 *dst, u8 *src, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        
        u16 mfm =
        ((src[i] & 0b10000000) << 7) |
        ((src[i] & 0b01000000) << 6) |
        ((src[i] & 0b00100000) << 5) |
        ((src[i] & 0b00010000) << 4) |
        ((src[i] & 0b00001000) << 3) |
        ((src[i] & 0b00000100) << 2) |
        ((src[i] & 0b00000010) << 1) |
        ((src[i] & 0b00000001) << 0);
        
        dst[2*i+0] = HI_BYTE(mfm);
        dst[2*i+1] = LO_BYTE(mfm);
    }
}

void
Disk::decodeMFM(u8 *dst, u8 *src, size_t count)
{
    for(size_t i = 0; i < count; i++) {
        
        u16 mfm = HI_LO(src[2*i], src[2*i+1]);
        dst[i] =
        ((mfm & 0b0100000000000000) >> 7) |
        ((mfm & 0b0001000000000000) >> 6) |
        ((mfm & 0b0000010000000000) >> 5) |
        ((mfm & 0b0000000100000000) >> 4) |
        ((mfm & 0b0000000001000000) >> 3) |
        ((mfm & 0b0000000000010000) >> 2) |
        ((mfm & 0b0000000000000100) >> 1) |
        ((mfm & 0b0000000000000001) >> 0);
    }
}

void
Disk::encodeOddEven(u8 *dst, u8 *src, size_t count)
{
    // Encode odd bits
    for(size_t i = 0; i < count; i++)
        dst[i] = (src[i] >> 1) & 0x55;
    
    // Encode even bits
    for(size_t i = 0; i < count; i++)
        dst[i + count] = src[i] & 0x55;
}

void
Disk::decodeOddEven(u8 *dst, u8 *src, size_t count)
{
    // Decode odd bits
    for(size_t i = 0; i < count; i++)
        dst[i] = (src[i] & 0x55) << 1;
    
    // Decode even bits
    for(size_t i = 0; i < count; i++)
        dst[i] |= src[i + count] & 0x55;
}

void
Disk::addClockBits(u8 *dst, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        dst[i] = addClockBits(dst[i], dst[i-1]);
    }
}

u8
Disk::addClockBits(u8 value, u8 previous)
{
    // Clear all previously set clock bits
    value &= 0x55;

    // Compute clock bits (clock bit values are inverted)
    u8 lShifted = (value << 1);
    u8 rShifted = (value >> 1) | (previous << 7);
    u8 cBitsInv = lShifted | rShifted;

    // Reverse the computed clock bits
    u64 cBits = cBitsInv ^ 0xAA;
    
    // Return original value with the clock bits added
    return value | cBits;
}
