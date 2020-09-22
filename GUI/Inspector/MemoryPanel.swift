// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

/*
extension NSColor {
    
    convenience init(r: Int, g: Int, b: Int, a: Int) {
        
        self.init(red: CGFloat(r) / 0xFF,
                  green: CGFloat(g) / 0xFF,
                  blue: CGFloat(b) / 0xFF,
                  alpha: CGFloat(a) / 0xFF)
    }
}
*/

struct MemColors {

    static let unmapped = NSColor.gray

    static let chip = NSColor.init(r: 0x80, g: 0xFF, b: 0x00, a: 0xFF)
    static let slow = NSColor.init(r: 0x66, g: 0xCC, b: 0x00, a: 0xFF)
    static let fast = NSColor.init(r: 0x4C, g: 0x99, b: 0x00, a: 0xFF)

    static let rom = NSColor.init(r: 0xFF, g: 0x00, b: 0x00, a: 0xFF)
    static let wom = NSColor.init(r: 0xCC, g: 0x00, b: 0x00, a: 0xFF)
    static let ext = NSColor.init(r: 0x99, g: 0x00, b: 0x00, a: 0xFF)

    static let cia = NSColor.init(r: 0x66, g: 0xB2, b: 0xFF, a: 0xFF)
    static let rtc = NSColor.init(r: 0xB2, g: 0x66, b: 0xFF, a: 0xFF)
    static let custom = NSColor.init(r: 0xFF, g: 0xFF, b: 0x66, a: 0xFF)
    static let auto = NSColor.init(r: 0xFF, g: 0x66, b: 0xB2, a: 0xFF)
}

extension Inspector {

    var memLayoutImage: NSImage? {

        guard let memory = parent?.amiga.mem else { return nil }

        // Create image representation in memory
        let size = CGSize.init(width: 256, height: 16)
        let cap = Int(size.width) * Int(size.height)
        let mask = calloc(cap, MemoryLayout<UInt32>.size)!
        let ptr = mask.bindMemory(to: UInt32.self, capacity: cap)

        // Create image data
        for x in 0...255 {

            let src = memory.memSrc(x << 16)
            var color: NSColor
            var mirror = false
            
            switch src {
            case .MEM_NONE:          color = MemColors.unmapped
            case .MEM_CHIP:          color = MemColors.chip
            case .MEM_CHIP_MIRROR:   color = MemColors.chip;     mirror = true
            case .MEM_FAST:          color = MemColors.fast
            case .MEM_SLOW:          color = MemColors.slow
            case .MEM_SLOW_MIRROR:   color = MemColors.slow;     mirror = true
            case .MEM_ROM:           color = MemColors.rom
            case .MEM_ROM_MIRROR:    color = MemColors.rom;      mirror = true
            case .MEM_WOM:           color = MemColors.wom
            case .MEM_EXT:           color = MemColors.ext
            case .MEM_CIA:           color = MemColors.cia
            case .MEM_CIA_MIRROR:    color = MemColors.cia;      mirror = true
            case .MEM_RTC:           color = MemColors.rtc
            case .MEM_CUSTOM:        color = MemColors.custom
            case .MEM_CUSTOM_MIRROR: color = MemColors.custom;   mirror = true
            case .MEM_AUTOCONF:      color = MemColors.auto
            default:                 fatalError()
            }
            let ciBgColor = CIColor(color: MemColors.unmapped)!
            let ciColor = CIColor(color: color)!
            
            for y in 0...15 {
                
                let c = 2
                var r, g, b, a: Int
                
                if mirror && (y % 4) == (x % 4) {
                    r = Int(ciBgColor.red * CGFloat(255 - y*c))
                    g = Int(ciBgColor.green * CGFloat(255 - y*c))
                    b = Int(ciBgColor.blue * CGFloat(255 - y*c))
                    a = Int(ciBgColor.alpha)
                } else {
                    r = Int(ciColor.red * CGFloat(255 - y*c))
                    g = Int(ciColor.green * CGFloat(255 - y*c))
                    b = Int(ciColor.blue * CGFloat(255 - y*c))
                    a = Int(ciColor.alpha)
                }
                ptr[x + 256*y] = UInt32(r | g << 8 | b << 16 | a << 24)
            }
        }

        // Create image
        let image = NSImage.make(data: mask, rect: size)
        let resizedImage = image?.resizeSharp(width: 512, height: 16)
        return resizedImage
    }

    private func refreshMemoryLayout() {

        let config = amiga.config
        let size = NSSize(width: 16, height: 16)

        memLayoutButton.image   = memLayoutImage
        memChipRamButton.image  = NSImage.init(color: MemColors.chip, size: size)
        memFastRamButton.image  = NSImage.init(color: MemColors.fast, size: size)
        memSlowRamButton.image  = NSImage.init(color: MemColors.slow, size: size)
        memRomButton.image      = NSImage.init(color: MemColors.rom, size: size)
        memWomButton.image      = NSImage.init(color: MemColors.wom, size: size)
        memExtButton.image      = NSImage.init(color: MemColors.ext, size: size)
        memCIAButton.image      = NSImage.init(color: MemColors.cia, size: size)
        memRTCButton.image      = NSImage.init(color: MemColors.rtc, size: size)
        memOCSButton.image      = NSImage.init(color: MemColors.custom, size: size)
        memAutoConfButton.image = NSImage.init(color: MemColors.auto, size: size)

        let chipKB = config.mem.chipSize / 1024
        let fastKB = config.mem.fastSize / 1024
        let slowKB = config.mem.slowSize / 1024
        let romKB = config.mem.romSize / 1024
        let womKB = config.mem.womSize / 1024
        let extKB = config.mem.extSize / 1024
        memChipRamText.stringValue = String.init(format: "%d KB", chipKB)
        memFastRamText.stringValue = String.init(format: "%d KB", fastKB)
        memSlowRamText.stringValue = String.init(format: "%d KB", slowKB)
        memRomText.stringValue = String.init(format: "%d KB", romKB)
        memWomText.stringValue = String.init(format: "%d KB", womKB)
        memExtText.stringValue = String.init(format: "%d KB", extKB)
    }

    func refreshMemory(count: Int = 0, full: Bool = false) {

        if full { refreshMemoryLayout() }

        memTableView.refresh(count: count, full: full)
        memBankTableView.refresh(count: count, full: full)
    }

    func jumpTo(addr: Int) {
        
        if addr >= 0 && addr <= 0xFFFFFF {
            
            searchAddress = addr
            jumpTo(bank: addr >> 16)
            let row = (addr & 0xFFFF) / 16
            memTableView.scrollRowToVisible(row)
            memTableView.selectRowIndexes([row], byExtendingSelection: false)
        }
    }
    
    func jumpTo(source: MemorySource) {

        for bank in 0...255 {

            if parent?.amiga.mem.memSrc(bank << 16) == source {
                jumpTo(bank: bank)
                return
            }
        }
    }

    func jumpTo(bank nr: Int) {
        
        if nr >= 0 && nr <= 0xFF {
            
            displayedBank = nr
            displayedBankType = parent?.amiga.mem.memSrc(displayedBank << 16) ?? .MEM_NONE
            memLayoutSlider.integerValue = displayedBank
            memTableView.scrollRowToVisible(0)
            memBankTableView.scrollRowToVisible(nr)
            memBankTableView.selectRowIndexes([nr], byExtendingSelection: false)
            fullRefresh()
        }
    }

    @IBAction func memSliderAction(_ sender: NSSlider!) {

        jumpTo(bank: sender.integerValue)
    }

    @IBAction func memChipAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_CHIP)
    }

    @IBAction func memFastRamAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_FAST)
    }
    
    @IBAction func memSlowRamAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_SLOW)
    }

    @IBAction func memRomAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_ROM)
    }

    @IBAction func memWomAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_WOM)
    }

    @IBAction func memExtAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_EXT)
    }

    @IBAction func memCIAAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_CIA)
    }
 
    @IBAction func memRTCAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_RTC)
    }

    @IBAction func memOCSAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_CUSTOM)
    }

    @IBAction func memAutoConfAction(_ sender: NSButton!) {

        jumpTo(source: .MEM_AUTOCONF)
    }

    @IBAction func memSearchAction(_ sender: NSTextField!) {

        let input = sender.stringValue
        if let addr = Int(input, radix: 16), input != "" {
            sender.stringValue = String(format: "%06X", addr)
            jumpTo(addr: addr)
        } else {
            sender.stringValue = ""
            searchAddress = -1
        }
        fullRefresh()
    }
}
