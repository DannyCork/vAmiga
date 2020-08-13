// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "Amiga.h"

void
Paula::serviceIrqEvent()
{
    assert(agnus.slot[IRQ_SLOT].id == IRQ_CHECK);

    Cycle clock = agnus.clock;
    Cycle next = NEVER;

    // Check all interrupt sources
    for (int src = 0; src < 16; src++) {

        // Check if the interrupt source is due
        if (clock >= setIntreq[src]) {
            setINTREQ(true, 1 << src);
            setIntreq[src] = NEVER;
        } else {
             next = MIN(next, setIntreq[src]);
        }
    }

    // Schedule next event
    agnus.scheduleAbs<IRQ_SLOT>(next, IRQ_CHECK);
}

void
Paula::serviceIplEvent()
{
    assert(agnus.slot[IPL_SLOT].id == IPL_CHANGE);
    
    cpu.setIPL((iplPipe >> 32) & 0xFF);
    iplPipe = (iplPipe << 8) | (iplPipe & 0xFF);
    
    // Reschedule event until the pipe has been shifted through entirely
    i64 repeat = agnus.slot[IPL_SLOT].data;
    if (repeat) {
        agnus.scheduleRel<IPL_SLOT>(DMA_CYCLES(1), IPL_CHANGE, repeat - 1);
    } else {
        agnus.cancel<IPL_SLOT>();
    }
}

void
Paula::servicePotEvent(EventID id)
{
    debug(POT_DEBUG, "servicePotEvent(%d)\n", id);

    switch (id) {

        case POT_DISCHARGE:
        {
            if (--agnus.slot[POT_SLOT].data) {

                // Discharge capacitors
                if (!OUTLY()) chargeY0 = 0.0;
                if (!OUTLX()) chargeX0 = 0.0;
                if (!OUTRY()) chargeY1 = 0.0;
                if (!OUTRX()) chargeX1 = 0.0;

                agnus.scheduleRel<POT_SLOT>(DMA_CYCLES(HPOS_CNT), POT_DISCHARGE);

            } else {

                // Reset counters
                // For input pins, we need to set the couter value to -1. It'll
                // wrap over to 0 in the hsync handler.
                potCntY0 = OUTLY() ? 0 : -1;
                potCntX0 = OUTLX() ? 0 : -1;
                potCntY1 = OUTRY() ? 0 : -1;
                potCntX1 = OUTRX() ? 0 : -1;

                // Schedule first charge event
                agnus.scheduleRel<POT_SLOT>(DMA_CYCLES(HPOS_CNT), POT_CHARGE);
            }
            break;
        }
        case POT_CHARGE:
        {
            bool cont = false;

            // Get delta charges for each line
            double dy0 = controlPort1.getChargeDY();
            double dx0 = controlPort1.getChargeDX();
            double dy1 = controlPort2.getChargeDY();
            double dx1 = controlPort2.getChargeDX();

            // Charge capacitors
            if (dy0 && chargeY0 < 1.0 && !OUTLY()) { chargeX0 += dy0; cont = true; }
            if (dx0 && chargeX0 < 1.0 && !OUTLX()) { chargeX0 += dx0; cont = true; }
            if (dy1 && chargeY1 < 1.0 && !OUTRY()) { chargeX0 += dy1; cont = true; }
            if (dx1 && chargeX1 < 1.0 && !OUTRX()) { chargeX0 += dx1; cont = true; }

            // Schedule next event
            if (cont) {
                agnus.scheduleRel<POT_SLOT>(DMA_CYCLES(HPOS_CNT), POT_CHARGE);
            } else {
                agnus.cancel<POT_SLOT>();
            }
            break;
        }
        default:
            assert(false);
    }
}