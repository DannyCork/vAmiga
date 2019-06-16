// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

#include "Amiga.h"

SerialPort::SerialPort()
{
    setDescription("SerialPort");

    registerSnapshotItems(vector<SnapshotItem> {

        { &device,        sizeof(device),        PERSISTANT },
        { &port,          sizeof(port),          0 },
    });
}

void
SerialPort::_initialize()
{
    events = &amiga->agnus.events;
    uart = &amiga->paula.uart;
}

void
SerialPort::_powerOn()
{
    port = 0x1FFFFFE; 
}

void
SerialPort::_inspect()
{
    pthread_mutex_lock(&lock);

    info.port = port; 
    info.txd = getTXD();
    info.rxd = getRXD();
    info.rts = getRTS();
    info.cts = getCTS();
    info.dsr = getDSR();
    info.cd = getCD();
    info.dtr = getDTR();

    pthread_mutex_unlock(&lock);
}

void
SerialPort::_dump()
{
    plainmsg("    device: %d\n", device);
    plainmsg("      port: %X\n", port);
}

SerialPortInfo
SerialPort::getInfo()
{
    SerialPortInfo result;

    pthread_mutex_lock(&lock);
    result = info;
    pthread_mutex_unlock(&lock);

    return result;
}

void
SerialPort::connectDevice(SerialPortDevice device)
{
    if (isSerialPortDevice(device)) {
        debug("connectDevice(%d)\n");
        this->device = device;
    } else {
        assert(false);
    }

}

bool
SerialPort::getPin(int nr)
{
    assert(nr >= 1 && nr <= 25);

    bool result = !!GET_BIT(port, nr);

    debug(2, "getPin(%d) = %d port = %X\n", nr, result, port);
    return result;
}

void
SerialPort::setPin(int nr, bool value)
{
    debug(2, "setPin(%d,%d)\n", nr, value);
    assert(nr >= 1 && nr <= 25);

    // Only continue if the pin value changes
    if (GET_BIT(port, nr) == value) return;

    // Set the new pin value
    WRITE_BIT(port, nr, value);

    // Perform special actions
    switch (nr) {

        case 2: // TXD

            // Emulate the loopback cable (if connected)
            if (device == SPD_LOOPBACK) {
                setRXD(value);
            }
            break;

        case 3: // RXD

            // Let the UART know about the change
            uart->rxdHasChanged(value);
            break;

        case 4: // RTS

            // Emulate the loopback cable (if connected)
            if (device == SPD_LOOPBACK) {
                setCTS(value);
                setDSR(value);
            }
            break;

        case 20: // DTR

            // Emulate the loopback cable (if connected)
            if (device == SPD_LOOPBACK) {
                setCD(value);
            }
            break;

    }
}