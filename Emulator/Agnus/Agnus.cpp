// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "Amiga.h"

Agnus::Agnus(Amiga& ref) : AmigaComponent(ref)
{
    setDescription("Agnus");
    
    subComponents = vector<HardwareComponent *> {
        
        &copper,
        &blitter,
        &dmaDebugger
    };
    
    config.revision = AGNUS_ECS_1MB;
    ptrMask = 0x0FFFFF;
    
    initLookupTables();
}

void Agnus::_reset(bool hard)
{
    RESET_SNAPSHOT_ITEMS(hard)
    
    // Start with a long frame
    frame = Frame();
    
    // Initialize statistical counters
    clearStats();
    
    // Initialize event tables
    for (int i = pos.h; i < HPOS_CNT; i++) bplEvent[i] = bplDMA[0][0][i];
    for (int i = pos.h; i < HPOS_CNT; i++) dasEvent[i] = dasDMA[0][i];
    updateBplJumpTable();
    updateDasJumpTable();
    
    // Initialize the event slots
    for (unsigned i = 0; i < SLOT_COUNT; i++) {
        slot[i].triggerCycle = NEVER;
        slot[i].id = (EventID)0;
        slot[i].data = 0;
    }
    
    // Schedule initial events
    scheduleRel<RAS_SLOT>(DMA_CYCLES(HPOS_CNT), RAS_HSYNC);
    scheduleRel<CIAA_SLOT>(CIA_CYCLES(AS_CIA_CYCLES(clock)), CIA_EXECUTE);
    scheduleRel<CIAB_SLOT>(CIA_CYCLES(AS_CIA_CYCLES(clock)), CIA_EXECUTE);
    scheduleRel<SEC_SLOT>(NEVER, SEC_TRIGGER);
    scheduleRel<VBL_SLOT>(DMA_CYCLES(HPOS_CNT * vStrobeLine()), VBL_STROBE0);
    scheduleRel<IRQ_SLOT>(NEVER, IRQ_CHECK);
    diskController.scheduleFirstDiskEvent();
    scheduleNextBplEvent();
    scheduleNextDasEvent();
    
    /*
    // Start with long frames by setting the LOF bit
    pokeVPOS(0x8000);
    */
    pokeVPOS(0);
}

long
Agnus::getConfigItem(ConfigOption option)
{
    switch (option) {
            
        case OPT_AGNUS_REVISION: return config.revision;
        case OPT_SLOW_RAM_MIRROR: return config.slowRamMirror;
            
        default: assert(false);
    }
}

bool
Agnus::setConfigItem(ConfigOption option, long value)
{
    switch (option) {
            
        case OPT_AGNUS_REVISION:
            
            #ifdef FORCE_AGNUS_REVISION
            value = FORCE_AGNUS_REVISION;
            warn("Overriding Agnus revision: %d\n", value);
            #endif
            
            if (!isAgnusRevision(value)) {
                warn("Invalid Agnus revision: %d\n", value);
                return false;
            }
            
            if (config.revision == value) {
                return false;
            }
            
            amiga.suspend();
            
            config.revision = (AgnusRevision)value;
            switch (config.revision) {
                case AGNUS_OCS:     ptrMask = 0x07FFFF; break;
                case AGNUS_ECS_1MB: ptrMask = 0x0FFFFF; break;
                case AGNUS_ECS_2MB: ptrMask = 0x1FFFFF; break;
                default: assert(false);
            }
            mem.updateMemSrcTables();
            
            amiga.resume();
            return true;
            
        case OPT_SLOW_RAM_MIRROR:
            
            if (config.slowRamMirror == value) {
                return false;
            }
            config.slowRamMirror = value;
            return true;
            
        default:
            return false;
    }
}

i16
Agnus::idBits()
{
    switch (config.revision) {
            
        case AGNUS_ECS_2MB: return 0x2000; // TODO: CHECK ON REAL MACHINE
        case AGNUS_ECS_1MB: return 0x2000;
        default:            return 0x0000;
    }
}

long
Agnus::chipRamLimit()
{
    switch (config.revision) {

        case AGNUS_ECS_2MB: return 2048;
        case AGNUS_ECS_1MB: return 1024;
        default:            return 512;
    }
}

bool
Agnus::slowRamIsMirroredIn()
{

    if (config.slowRamMirror && isECS()) {
        return mem.chipRamSize() == KB(512) && mem.slowRamSize() == KB(512);
    } else {
        return false;
    }
}

void
Agnus::_inspect()
{
    synchronized {
        
        info.vpos     = pos.v;
        info.hpos     = pos.h;
        
        info.dmacon   = dmacon;
        info.bplcon0  = bplcon0;
        info.bpu      = bpu();
        info.ddfstrt  = ddfstrt;
        info.ddfstop  = ddfstop;
        info.diwstrt  = diwstrt;
        info.diwstop  = diwstop;
        
        info.bpl1mod  = bpl1mod;
        info.bpl2mod  = bpl2mod;
        info.bltamod  = blitter.bltamod;
        info.bltbmod  = blitter.bltbmod;
        info.bltcmod  = blitter.bltcmod;
        info.bltdmod  = blitter.bltdmod;
        info.bltcon0  = blitter.bltcon0;
        info.bls      = bls;
        
        info.coppc    = copper.coppc & ptrMask;
        info.dskpt    = dskpt & ptrMask;
        info.bltpt[0] = blitter.bltapt & ptrMask;
        info.bltpt[1] = blitter.bltbpt & ptrMask;
        info.bltpt[2] = blitter.bltcpt & ptrMask;
        info.bltpt[3] = blitter.bltdpt & ptrMask;
        for (unsigned i = 0; i < 6; i++) info.bplpt[i] = bplpt[i] & ptrMask;
        for (unsigned i = 0; i < 4; i++) info.audpt[i] = audpt[i] & ptrMask;
        for (unsigned i = 0; i < 4; i++) info.audlc[i] = audlc[i] & ptrMask;
        for (unsigned i = 0; i < 8; i++) info.sprpt[i] = sprpt[i] & ptrMask;
    }
}

void
Agnus::_dump()
{
    msg(" actions : %X\n", hsyncActions);

    msg("   dskpt : %X\n", dskpt);
    for (unsigned i = 0; i < 4; i++) msg("audpt[%d] : %X\n", i, audpt[i]);
    for (unsigned i = 0; i < 6; i++) msg("bplpt[%d] : %X\n", i, bplpt[i]);
    for (unsigned i = 0; i < 8; i++) msg("bplpt[%d] : %X\n", i, sprpt[i]);
    
    msg("   hstrt : %d\n", diwHstrt);
    msg("   hstop : %d\n", diwHstop);
    msg("   vstrt : %d\n", diwVstrt);
    msg("   vstop : %d\n", diwVstop);

    msg("\nEvents:\n\n");
    dumpEvents();

    msg("\nBPL DMA table:\n\n");
    dumpBplEventTable();

    msg("\nDAS DMA table:\n\n");
    dumpDasEventTable();
}

void
Agnus::clearStats()
{
    for (int i = 0; i < BUS_OWNER_COUNT; i++) stats.usage[i] = 0;
    
    stats.copperActivity = 0;
    stats.blitterActivity = 0;
    stats.diskActivity = 0;
    stats.audioActivity = 0;
    stats.spriteActivity = 0;
    stats.bitplaneActivity = 0;
}

void
Agnus::updateStats()
{
    const double w = 0.5;
    
    double copper = stats.usage[BUS_COPPER];
    double blitter = stats.usage[BUS_BLITTER];
    double disk = stats.usage[BUS_DISK];
    double audio = stats.usage[BUS_AUDIO];
    
    double sprite =
    stats.usage[BUS_SPRITE0] +
    stats.usage[BUS_SPRITE1] +
    stats.usage[BUS_SPRITE2] +
    stats.usage[BUS_SPRITE3] +
    stats.usage[BUS_SPRITE4] +
    stats.usage[BUS_SPRITE5] +
    stats.usage[BUS_SPRITE6] +
    stats.usage[BUS_SPRITE7];
    
    double bitplane =
    stats.usage[BUS_BPL1] +
    stats.usage[BUS_BPL2] +
    stats.usage[BUS_BPL3] +
    stats.usage[BUS_BPL4] +
    stats.usage[BUS_BPL5] +
    stats.usage[BUS_BPL6];

    stats.copperActivity = w * stats.copperActivity + (1 - w) * copper;
    stats.blitterActivity = w * stats.blitterActivity + (1 - w) * blitter;
    stats.diskActivity = w * stats.diskActivity + (1 - w) * disk;
    stats.audioActivity = w * stats.audioActivity + (1 - w) * audio;
    stats.spriteActivity = w * stats.spriteActivity + (1 - w) * sprite;
    stats.bitplaneActivity = w * stats.bitplaneActivity + (1 - w) * bitplane;
    
    for (int i = 0; i < BUS_OWNER_COUNT; i++) stats.usage[i] = 0;
}

Cycle
Agnus::cyclesInFrame()
{
    return DMA_CYCLES(frame.numLines() * HPOS_CNT);
}

Cycle
Agnus::startOfFrame()
{
    return clock - DMA_CYCLES(pos.v * HPOS_CNT + pos.h);
}

Cycle
Agnus::startOfNextFrame()
{
    return startOfFrame() + cyclesInFrame();
}

bool
Agnus::belongsToPreviousFrame(Cycle cycle)
{
    return cycle < startOfFrame();
}

bool
Agnus::belongsToCurrentFrame(Cycle cycle)
{
    return !belongsToPreviousFrame(cycle) && !belongsToNextFrame(cycle);
}

bool
Agnus::belongsToNextFrame(Cycle cycle)
{
    return cycle >= startOfNextFrame();
}

bool
Agnus::inBplDmaLine(u16 dmacon, u16 bplcon0) {

    return
    ddfVFlop                 // Outside VBLANK, inside DIW
    && bpu(bplcon0)          // At least one bitplane enabled
    && bpldma(dmacon);       // Bitplane DMA enabled
}

Cycle
Agnus::beamToCycle(Beam beam)
{
    return startOfFrame() + DMA_CYCLES(beam.v * HPOS_CNT + beam.h);
}

Beam
Agnus::cycleToBeam(Cycle cycle)
{
    Beam result;

    Cycle diff = AS_DMA_CYCLES(cycle - startOfFrame());
    assert(diff >= 0);

    result.v = diff / HPOS_CNT;
    result.h = diff % HPOS_CNT;
    return result;
}

Beam
Agnus::addToBeam(Beam beam, Cycle cycles)
{
    Beam result;

    Cycle cycle = beam.v * HPOS_CNT + beam.h + cycles;
    result.v = cycle / HPOS_CNT;
    result.h = cycle % HPOS_CNT;

    return result;
}


void
Agnus::predictDDF()
{
    DDF oldLores = ddfLores;
    DDF oldHires = ddfHires;
    DDFState oldState = ddfState;
    
    ddfstrtReached = ddfstrt < HPOS_CNT ? ddfstrt : -1;
    ddfstopReached = ddfstop < HPOS_CNT ? ddfstop : -1;
    
    computeDDFWindow();
    
    if (ddfLores != oldLores || ddfHires != oldHires || ddfState != oldState) {
        
        hsyncActions |= HSYNC_UPDATE_BPL_TABLE; // Update bitplane events
        hsyncActions |= HSYNC_PREDICT_DDF;      // Call this function again
    }
    
    trace(DDF_DEBUG, "predictDDF LORES: %d %d\n", ddfLores.strtOdd, ddfLores.stopOdd);
    trace(DDF_DEBUG, "predictDDF HIRES: %d %d\n", ddfHires.strtOdd, ddfHires.stopOdd);
}

void
Agnus::computeDDFWindow()
{
    isOCS() ? computeDDFWindowOCS() : computeDDFWindowECS();
}

#define DDF_EMPTY     0
#define DDF_STRT_STOP 1
#define DDF_STRT_D8   2
#define DDF_18_STOP   3
#define DDF_18_D8     4

void
Agnus::computeDDFWindowOCS()
{
    /* To determine the correct data fetch window, we need to distinguish
     * three kinds of DDFSTRT / DDFSTOP values.
     *
     *   0:   small : Value is smaller than the left hardware stop.
     *   1:  medium : Value complies to the specs.
     *   2:   large : Value is larger than HPOS_MAX and thus never reached.
     */
    int strt = (ddfstrtReached < 0) ? 2 : (ddfstrtReached < 0x18) ? 0 : 1;
    int stop = (ddfstopReached < 0) ? 2 : (ddfstopReached < 0x18) ? 0 : 1;

    /* Emulate the special "scan line effect" of the OCS Agnus.
     * If DDFSTRT is set to a small value, DMA is enabled every other row.
     */
    if (ddfstrtReached < 0x18) {
        if (ocsEarlyAccessLine == pos.v) {
            ddfLores.compute(ddfstrtReached, ddfstopReached, bplcon1 & 0xF);
            ddfHires.compute(ddfstrtReached, ddfstopReached, bplcon1 & 0xF);
        } else {
            ddfLores.clear();
            ddfHires.clear();
            ocsEarlyAccessLine = pos.v + 1;
        }
        return;
    }

    /* Nr | DDFSTRT | DDFSTOP | State   || Data Fetch Window   | Next State
     *  --------------------------------------------------------------------
     *  0 | small   | small   | -       || Empty               | DDF_OFF
     *  1 | small   | medium  | -       || [0x18 ; DDFSTOP]    | DDF_OFF
     *  2 | small   | large   | -       || [0x18 ; 0xD8]       | DDF_OFF
     *  3 | medium  | small   | -       || not handled         | DDF_OFF
     *  4 | medium  | medium  | -       || [DDFSTRT ; DDFSTOP] | DDF_OFF
     *  5 | medium  | large   | -       || [DDFSTRT ; 0xD8]    | DDF_OFF
     *  6 | large   | small   | -       || not handled         | DDF_OFF
     *  7 | large   | medium  | -       || not handled         | DDF_OFF
     *  8 | large   | large   | -       || Empty               | DDF_OFF
     */
    const struct { int interval; } table[9] = {
        { DDF_EMPTY     }, // 0
        { DDF_18_STOP   }, // 1
        { DDF_18_D8     }, // 2
        { DDF_EMPTY     }, // 3
        { DDF_STRT_STOP }, // 4
        { DDF_STRT_D8   }, // 5
        { DDF_EMPTY     }, // 6
        { DDF_EMPTY     }, // 7
        { DDF_EMPTY     }  // 8
    };

    int index = 3*strt + stop;
    switch (table[index].interval) {

        case DDF_EMPTY:
            ddfLores.clear();
            ddfHires.clear();
            break;
        case DDF_STRT_STOP:
            ddfLores.compute(ddfstrtReached, ddfstopReached, bplcon1 & 0xF);
            ddfHires.compute(ddfstrtReached, ddfstopReached, bplcon1 & 0xF);
            break;
        case DDF_STRT_D8:
            ddfLores.compute(ddfstrtReached, 0xD8, bplcon1 & 0xF);
            ddfHires.compute(ddfstrtReached, 0xD8, bplcon1 & 0xF);
            break;
        case DDF_18_STOP:
            ddfLores.compute(0x18, ddfstopReached, bplcon1 & 0xF);
            ddfHires.compute(0x18, ddfstopReached, bplcon1 & 0xF);
            break;
        case DDF_18_D8:
            ddfLores.compute(0x18, 0xD8, bplcon1 & 0xF);
            ddfHires.compute(0x18, 0xD8, bplcon1 & 0xF);
            break;
    }

    trace(DDF_DEBUG, "DDF Window Odd (OCS):  (%d,%d) (%d,%d)\n",
          ddfLores.strtOdd, ddfHires.strtOdd, ddfLores.stopOdd, ddfHires.stopOdd);
    trace(DDF_DEBUG, "DDF Window Even (OCS): (%d,%d) (%d,%d)\n",
          ddfLores.strtEven, ddfHires.strtEven, ddfLores.stopEven, ddfHires.stopEven);

    return;
}

void
Agnus::computeDDFWindowECS()
{
    /* To determine the correct data fetch window, we need to distinguish
     * three kinds of DDFSTRT / DDFSTOP values.
     *
     *   0:   small : Value is smaller than the left hardware stop.
     *   1:  medium : Value complies to the specs.
     *   2:   large : Value is larger than HPOS_MAX and thus never reached.
     */
    int strt = (ddfstrtReached < 0) ? 2 : (ddfstrtReached < 0x18) ? 0 : 1;
    int stop = (ddfstopReached < 0) ? 2 : (ddfstopReached < 0x18) ? 0 : 1;

    /* Nr | DDFSTRT | DDFSTOP | State   || Data Fetch Window   | Next State
     *  --------------------------------------------------------------------
     *  0 | small   | small   | DDF_OFF || Empty               | DDF_OFF
     *  1 | small   | small   | DDF_ON  || Empty               | DDF_OFF
     *  2 | small   | medium  | DDF_OFF || [0x18 ; DDFSTOP]    | DDF_OFF
     *  3 | small   | medium  | DDF_ON  || [0x18 ; DDFSTOP]    | DDF_OFF
     *  4 | small   | large   | DDF_OFF || [0x18 ; 0xD8]       | DDF_ON
     *  5 | small   | large   | DDF_ON  || [0x18 ; 0xD8]       | DDF_ON
     *  6 | medium  | small   | DDF_OFF || not handled         | -
     *  7 | medium  | small   | DDF_ON  || not handled         | -
     *  8 | medium  | medium  | DDF_OFF || [DDFSTRT ; DDFSTOP] | DDF_OFF
     *  9 | medium  | medium  | DDF_ON  || [0x18 ; DDFSTOP]    | DDF_OFF
     * 10 | medium  | large   | DDF_OFF || [DDFSTRT ; 0xD8]    | DDF_ON
     * 11 | medium  | large   | DDF_ON  || [0x18 ; 0xD8]       | DDF_ON
     * 12 | large   | small   | DDF_OFF || not handled         | -
     * 13 | large   | small   | DDF_ON  || not handled         | -
     * 14 | large   | medium  | DDF_OFF || not handled         | -
     * 15 | large   | medium  | DDF_ON  || not handled         | -
     * 16 | large   | large   | DDF_OFF || Empty               | DDF_OFF
     * 17 | large   | large   | DDF_ON  || [0x18 ; 0xD8]       | DDF_ON
     */
    const struct { int interval; DDFState state; } table[18] = {
        { DDF_EMPTY ,    DDF_OFF }, // 0
        { DDF_EMPTY ,    DDF_OFF }, // 1
        { DDF_18_STOP ,  DDF_OFF }, // 2
        { DDF_18_STOP ,  DDF_OFF }, // 3
        { DDF_18_D8 ,    DDF_ON  }, // 4
        { DDF_18_D8 ,    DDF_ON  }, // 5
        { DDF_EMPTY ,    DDF_OFF }, // 6
        { DDF_EMPTY ,    DDF_OFF }, // 7
        { DDF_STRT_STOP, DDF_OFF }, // 8
        { DDF_18_STOP ,  DDF_OFF }, // 9
        { DDF_STRT_D8 ,  DDF_ON  }, // 10
        { DDF_18_D8 ,    DDF_ON  }, // 11
        { DDF_EMPTY ,    DDF_OFF }, // 12
        { DDF_EMPTY ,    DDF_OFF }, // 13
        { DDF_EMPTY ,    DDF_OFF }, // 14
        { DDF_EMPTY ,    DDF_OFF }, // 15
        { DDF_EMPTY ,    DDF_OFF }, // 16
        { DDF_18_D8 ,    DDF_ON  }, // 17
    };

    int index = 6*strt + 2*stop + (ddfState == DDF_ON);
    switch (table[index].interval) {

        case DDF_EMPTY:
            ddfLores.clear();
            ddfHires.clear();
            break;
        case DDF_STRT_STOP:
            ddfLores.compute(ddfstrtReached, ddfstopReached, bplcon1);
            ddfHires.compute(ddfstrtReached, ddfstopReached, bplcon1);
            break;
        case DDF_STRT_D8:
            ddfLores.compute(ddfstrtReached, 0xD8, bplcon1);
            ddfHires.compute(ddfstrtReached, 0xD8, bplcon1);
            break;
        case DDF_18_STOP:
            ddfLores.compute(0x18, ddfstopReached, bplcon1);
            ddfHires.compute(0x18, ddfstopReached, bplcon1);
            break;
        case DDF_18_D8:
            ddfLores.compute(0x18, 0xD8, bplcon1);
            ddfHires.compute(0x18, 0xD8, bplcon1);
            break;
    }
    ddfState = table[index].state;

    trace(DDF_DEBUG, "DDF Window Odd (ECS):  (%d,%d) (%d,%d)\n",
          ddfLores.strtOdd, ddfHires.strtOdd, ddfLores.stopOdd, ddfHires.stopOdd);
    trace(DDF_DEBUG, "DDF Window Even (ECS): (%d,%d) (%d,%d)\n",
          ddfLores.strtEven, ddfHires.strtEven, ddfLores.stopEven, ddfHires.stopEven);

    return;
}


int
Agnus::bpu(u16 v)
{
    // Extract the three BPU bits and check for hires mode
    int bpu = (v >> 12) & 0b111;
    bool hires = GET_BIT(v, 15);

    if (hires) {
        return bpu < 5 ? bpu : 0; // Disable all channels if value is invalid
    } else {
        return bpu < 7 ? bpu : 4; // Enable four channels if value is invalid
    }
}

void
Agnus::execute()
{
    // Process pending events
    if (nextTrigger <= clock) {
        executeEventsUntil(clock);
    } else {
        assert(pos.h < 0xE2);
    }

    // Advance the internal clock and the horizontal counter
    clock += DMA_CYCLES(1);

    assert(pos.h <= HPOS_MAX);
    pos.h = pos.h < HPOS_MAX ? pos.h + 1 : 0; // (pos.h + 1) % HPOS_CNT;

    // If this assertion hits, the HSYNC event hasn't been served
    // if (pos.h > HPOS_CNT) { dump(); dumpBplEventTable(); }
    assert(pos.h <= HPOS_CNT);
}

#ifdef AGNUS_EXEC_DEBUG

void
Agnus::executeUntil(Cycle targetClock)
{
    // Align to DMA cycle raster
    targetClock &= ~0b111;

    // Compute the number of DMA cycles to execute
    DMACycle dmaCycles = (targetClock - clock) / DMA_CYCLES(1);

    // Execute DMA cycles one after another
    for (DMACycle i = 0; i < dmaCycles; i++) execute();
}

#else

void
Agnus::executeUntil(Cycle targetClock)
{
    // Align to DMA cycle raster
    targetClock &= ~0b111;

    // Compute the number of DMA cycles to execute
    DMACycle dmaCycles = (targetClock - clock) / DMA_CYCLES(1);

    if (targetClock < nextTrigger && dmaCycles > 0) {

        // Advance directly to the target clock
        clock = targetClock;
        pos.h += dmaCycles;

        // If this assertion hits, the HSYNC event hasn't been served
        assert(pos.h <= HPOS_CNT);

    } else {

        // Execute DMA cycles one after another
        for (DMACycle i = 0; i < dmaCycles; i++) execute();
    }
}
#endif

void
Agnus::syncWithEClock()
{
    // Check if E clock syncing is disabled
    if (!ciaa.getEClockSyncing()) return;

    /* The E clock is 6 clocks low and 4 clocks high:
     *
     *     |   |   |   |   |   |   |---|---|---|---|
     *     |---|---|---|---|---|---|   |   |   |   |
     *      (4) (5) (6) (7) (8) (9) (0) (1) (2) (3)   (eClk)
     */

    // Determine where we are in the current E clock cycle
    Cycle eClk = (clock >> 2) % 10;
    
    // We want to sync to position (2).
    // If we are already too close, we seek (2) in the next E clock cycle.
    Cycle delay;
    switch (eClk) {
        case 0: delay = 4 * (2 + 10); break;
        case 1: delay = 4 * (1 + 10); break;
        case 2: delay = 4 * (0 + 10); break;
        case 3: delay = 4 * 9;        break;
        case 4: delay = 4 * 8;        break;
        case 5: delay = 4 * 7;        break;
        case 6: delay = 4 * 6;        break;
        case 7: delay = 4 * (5 + 10); break;
        case 8: delay = 4 * (4 + 10); break;
        case 9: delay = 4 * (3 + 10); break;
        default: assert(false);
    }
    
    // Doublecheck that we are going to sync to a DMA cycle
    assert(DMA_CYCLES(AS_DMA_CYCLES(clock + delay)) == clock + delay);
    
    // Execute Agnus until the target cycle has been reached
    executeUntil(clock + delay);

    // Add wait states to the CPU
    cpu.addWaitStates(delay);
}

bool
Agnus::inSyncWithEClock()
{
    // Check if E clock syncing is disabled
    if (!ciaa.getEClockSyncing()) return true;
        
    // Determine where we are in the current E clock cycle
    Cycle eClk = (clock >> 2) % 10;
    
    // Unsure if this condition is accurate
    return eClk >= 2 || eClk <= 6;
}

void
Agnus::executeUntilBusIsFree()
{    
    i16 posh = pos.h == 0 ? HPOS_MAX : pos.h - 1;

    // Check if the bus is blocked
    if (busOwner[posh] != BUS_NONE) {

        // This variable counts the number of DMA cycles the CPU will be suspended
        DMACycle delay = 0;

        // Execute Agnus until the bus is free
        do {
            // debug("Blocked by %d\n", busOwner[posh]);

            posh = pos.h;
            execute();
            if (++delay == 2) bls = true;
            
        } while (busOwner[posh] != BUS_NONE);

        // Clear the BLS line (Blitter slow down)
        bls = false;

        // Add wait states to the CPU
        cpu.addWaitStates(DMA_CYCLES(delay));
    }

    // Assign bus to the CPU
    busOwner[posh] = BUS_CPU;
}

void
Agnus::executeUntilBusIsFreeForCIA()
{
    // Sync with the E clock driving the CIA
    syncWithEClock();
    
    i16 posh = pos.h == 0 ? HPOS_MAX : pos.h - 1;
    
    // Check if the bus is blocked
    if (busOwner[posh] != BUS_NONE) {

        // This variable counts the number of DMA cycles the CPU will be suspended
        DMACycle delay = 0;

        // Execute Agnus until the bus is free
        do {

            posh = pos.h;
            execute();
            if (++delay == 2) bls = true;
            
        } while (busOwner[posh] != BUS_NONE || !inSyncWithEClock());

        // Clear the BLS line (Blitter slow down)
        bls = false;

        // Add wait states to the CPU
        cpu.addWaitStates(DMA_CYCLES(delay));
    }

    // Assign bus to the CPU
    busOwner[posh] = BUS_CPU;
}

void
Agnus::recordRegisterChange(Cycle delay, u32 addr, u16 value)
{
    // Record the new register value
    changeRecorder.insert(clock + delay, RegChange { addr, value} );
    
    // Schedule the register change
    scheduleNextREGEvent();
}

void
Agnus::updateRegisters()
{
}

template <int nr> void
Agnus::executeFirstSpriteCycle()
{
    trace(SPR_DEBUG, "executeFirstSpriteCycle<%d>\n", nr);

    if (pos.v == sprVStop[nr]) {

        sprDmaState[nr] = SPR_DMA_IDLE;

        if (busOwner[pos.h] == BUS_NONE) {

            // Read in the next control word (POS part)
            u16 value = doSpriteDMA<nr>();
            agnus.pokeSPRxPOS<nr>(value);
            denise.pokeSPRxPOS<nr>(value);
        }

    } else if (sprDmaState[nr] == SPR_DMA_ACTIVE) {

        if (busOwner[pos.h] == BUS_NONE) {

            // Read in the next data word (part A)
            u16 value = doSpriteDMA<nr>();
            denise.pokeSPRxDATA<nr>(value);
        }
    }
}

template <int nr> void
Agnus::executeSecondSpriteCycle()
{
    trace(SPR_DEBUG, "executeSecondSpriteCycle<%d>\n", nr);

    if (pos.v == sprVStop[nr]) {

        sprDmaState[nr] = SPR_DMA_IDLE;

        if (busOwner[pos.h] == BUS_NONE) {
            
            // Read in the next control word (CTL part)
            u16 value = doSpriteDMA<nr>();
            agnus.pokeSPRxCTL<nr>(value);
            denise.pokeSPRxCTL<nr>(value);
        }

    } else if (sprDmaState[nr] == SPR_DMA_ACTIVE) {

        if (busOwner[pos.h] == BUS_NONE) {

            // Read in the next data word (part B)
            u16 value = doSpriteDMA<nr>();
            denise.pokeSPRxDATB<nr>(value);
        }
    }
}

void
Agnus::updateSpriteDMA()
{
    // When the function is called, the sprite logic already sees an inremented
    // vertical position counter
    i16 v = pos.v + 1;

    // Reset the vertical trigger coordinates in line 25
    if (v == 25 && sprdma()) {
        for (int i = 0; i < 8; i++) sprVStop[i] = 25;
        return;
     }

    // Disable DMA in the last rasterline
    if (v == frame.lastLine()) {
        for (int i = 0; i < 8; i++) sprDmaState[i] = SPR_DMA_IDLE;
        return;
    }

    // Update the DMA status for all sprites
    for (int i = 0; i < 8; i++) {
        if (v == sprVStrt[i]) sprDmaState[i] = SPR_DMA_ACTIVE;
        if (v == sprVStop[i]) sprDmaState[i] = SPR_DMA_IDLE;
    }
}

void
Agnus::hsyncHandler()
{
    assert(pos.h == 0 || pos.h == HPOS_MAX + 1);

    // Call the hsync handlers of Denise
    denise.endOfLine(pos.v);

    // Update pot counters
    if (paula.chargeX0 < 1.0) paula.potCntX0++;
    if (paula.chargeY0 < 1.0) paula.potCntY0++;
    if (paula.chargeX1 < 1.0) paula.potCntX1++;
    if (paula.chargeY1 < 1.0) paula.potCntY1++;

    // Transfer DMA requests from Paula to Agnus
    paula.channel0.requestDMA();
    paula.channel1.requestDMA();
    paula.channel2.requestDMA();
    paula.channel3.requestDMA();

    // Reset the horizontal counter
    pos.h = 0;

    // Advance the vertical counter
    if (++pos.v >= frame.numLines()) vsyncHandler();

    // Initialize variables which keep values for certain trigger positions
    dmaconAtDDFStrt = dmacon;
    bplcon0AtDDFStrt = bplcon0;


    //
    // DIW
    //

    if (pos.v == diwVstrt && !diwVFlop) {
        diwVFlop = true;
        trace(DIW_DEBUG, "diwVFlop = %d\n", diwVFlop);
    }
    if (pos.v == diwVstop && diwVFlop) {
        diwVFlop = false;
        trace(DIW_DEBUG, "diwVFlop = %d\n", diwVFlop);
    }

    // Horizontal DIW flipflop
    diwHFlop = (diwHFlopOff != -1) ? false : (diwHFlopOn != -1) ? true : diwHFlop;
    diwHFlopOn = diwHstrt;
    diwHFlopOff = diwHstop;


    //
    // DDF
    //

    // Update the vertical DDF flipflop
    ddfVFlop = !inLastRasterline() && diwVFlop;


    //
    // Determine the bitplane DMA status for the line to come
    //

    bool newBplDmaLine = inBplDmaLine();

    // Update the bpl event table if the value has changed
    if (newBplDmaLine ^ bplDmaLine) {
        hsyncActions |= HSYNC_UPDATE_BPL_TABLE;
        bplDmaLine = newBplDmaLine;
    }


    //
    // Determine the disk, audio and sprite DMA status for the line to come
    //

    u16 newDmaDAS;

    if (dmacon & DMAEN) {

        // Copy DMA enable bits from dmacon
        newDmaDAS = dmacon & 0b111111;

        // Disable sprites outside the sprite DMA area
        if (pos.v < 25 || pos.v >= frame.lastLine()) newDmaDAS &= 0b011111;

    } else {

        newDmaDAS = 0;
    }

    if (dmaDAS != newDmaDAS) hsyncActions |= HSYNC_UPDATE_DAS_TABLE;
    dmaDAS = newDmaDAS;

    //
    // Process pending work items
    //

    if (hsyncActions) {

        if (hsyncActions & HSYNC_PREDICT_DDF) {
            hsyncActions &= ~HSYNC_PREDICT_DDF;
            predictDDF();
        }
        if (hsyncActions & HSYNC_UPDATE_BPL_TABLE) {
            hsyncActions &= ~HSYNC_UPDATE_BPL_TABLE;
            updateBplEvents();
        }
        if (hsyncActions & HSYNC_UPDATE_DAS_TABLE) {
            hsyncActions &= ~HSYNC_UPDATE_DAS_TABLE;
            updateDasEvents(dmaDAS);
        }
    }

    // Clear the bus usage table
    for (int i = 0; i < HPOS_CNT; i++) busOwner[i] = BUS_NONE;

    // Schedule the first BPL and DAS events
    scheduleNextBplEvent();
    scheduleNextDasEvent();


    //
    // Let other components prepare for the next line
    //

    denise.beginOfLine(pos.v);
}

void
Agnus::vsyncHandler()
{
    // Run the screen recorder
    denise.screenRecorder.vsyncHandler(clock - 50 * DMA_CYCLES(HPOS_CNT));

    // Synthesize sound samples
    paula.executeUntil(clock - 50 * DMA_CYCLES(HPOS_CNT));

    // Advance to the next frame
    frame.next(denise.lace());

    // Reset vertical position counter
    pos.v = 0;

    // Initialize the DIW flipflops
    diwVFlop = false;
    diwHFlop = true; 
            
    // Let other subcomponents do their own VSYNC stuff
    copper.vsyncHandler();
    denise.vsyncHandler();
    joystick1.execute();
    joystick2.execute();

    // Update statistics
    updateStats();
    mem.updateStats();
    
    // Count some sheep (zzzzzz) ...
    if (!amiga.inWarpMode()) {
        amiga.synchronizeTiming();
    }
}

//
// Instantiate template functions
//

template void Agnus::executeFirstSpriteCycle<0>();
template void Agnus::executeFirstSpriteCycle<1>();
template void Agnus::executeFirstSpriteCycle<2>();
template void Agnus::executeFirstSpriteCycle<3>();
template void Agnus::executeFirstSpriteCycle<4>();
template void Agnus::executeFirstSpriteCycle<5>();
template void Agnus::executeFirstSpriteCycle<6>();
template void Agnus::executeFirstSpriteCycle<7>();

template void Agnus::executeSecondSpriteCycle<0>();
template void Agnus::executeSecondSpriteCycle<1>();
template void Agnus::executeSecondSpriteCycle<2>();
template void Agnus::executeSecondSpriteCycle<3>();
template void Agnus::executeSecondSpriteCycle<4>();
template void Agnus::executeSecondSpriteCycle<5>();
template void Agnus::executeSecondSpriteCycle<6>();
template void Agnus::executeSecondSpriteCycle<7>();
