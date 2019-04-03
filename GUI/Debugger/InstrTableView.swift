// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

class InstrTableView : NSTableView {
    
    @IBOutlet weak var inspector: Inspector!
    
    var mem = amigaProxy!.mem!
    var cpu = amigaProxy?.cpu
    
    // Display caches
    var addrInRow  : [Int:UInt32] = [:]
    // var instrInRow : [Int:String] = [:]
    // var dataInRow  : [Int:String] = [:]
    var rowForAddr : [UInt32:Int] = [:]
    var hex = true
    
    override func awakeFromNib() {
        
        delegate = self
        dataSource = self
        target = self
        
        doubleAction = #selector(doubleClickAction(_:))
        action = #selector(clickAction(_:))
    }
    
    @IBAction func clickAction(_ sender: NSTableView!) {
        
        let row = sender.clickedRow
        let col = sender.clickedColumn

        track("row = \(row) col = \(col)")
        
        if col == 0, let addr = addrInRow[row] {
            
            if !cpu!.hasBreakpoint(at: addr) {
                cpu?.setBreakpointAt(addr)
                return
            }
            if cpu!.hasDisabledBreakpoint(at: addr) {
                cpu?.enableBreakpoint(at: addr)
                return
            }
            if cpu!.hasBreakpoint(at: addr) {
                cpu?.disableBreakpoint(at: addr)
                return
            }
        }
    }
    
    @IBAction func doubleClickAction(_ sender: NSTableView!) {
        
        let row = sender.clickedRow
        let col = sender.clickedColumn

        if col > 0, let addr = addrInRow[row] {
            
            if cpu!.hasBreakpoint(at: addr) {
                cpu!.deleteBreakpoint(at: addr)
            } else {
                cpu!.setBreakpointAt(addr)
            }
        }
    }
    
    // Jumps to the instruction the program counter is currently pointing to
    func jumpToPC() {
        
        if let pc = cpu?.getInfo().pc { jumpTo(addr: pc) }
    }

    // Jumps to the specified address
    func jumpTo(addr: UInt32?) {
    
        guard let addr = addr else { return }
        
        track("jumpTo: \(addr) \(rowForAddr)")
        
        if let row = rowForAddr[addr] {
            
            track("*** addr = \(addr)")
            // If the requested address is already displayed, we simply
            // select the corresponding row.
            scrollRowToVisible(row)
            selectRowIndexes([row], byExtendingSelection: false)
            reloadData()
            
        } else {
            
            track("addr = \(addr)")

            // If the requested address is not displayed, we update the
            // whole view and display it in the first row.
            scrollRowToVisible(0)
            selectRowIndexes([0], byExtendingSelection: false)
            update(addr: addr)
        }
    }
    
    // Updates the currently displayed instructions
    func update() {
        update(addr: addrInRow[0])
    }
    
    // Updates the displayed instructions, starting at the specified address
    func update(addr: UInt32?) {
        
        guard var addr = addr else { return }
        
        addrInRow = [:]
        rowForAddr = [:]
        
        for i in 0 ..< Int(CPUINFO_INSTR_COUNT) {
            
            if (addr <= 0xFFFFFF) {
                
                let bytes = cpu!.getInstr(i).bytes
                track("bytes = \(bytes)")
                addrInRow[i] = addr
                rowForAddr[addr] = i
                addr += UInt32(bytes)
            }
        }
        
        reloadData()
    }
    
    func refresh(everything: Bool) {
    
        if (everything) {
        
            mem = amigaProxy!.mem
            cpu = amigaProxy!.cpu
            
            for (c,f) in ["addr" : fmt24] {
                let columnId = NSUserInterfaceItemIdentifier(rawValue: c)
                if let column = tableColumn(withIdentifier: columnId) {
                    if let cell = column.dataCell as? NSCell {
                        cell.formatter = f
                    }
                }
            }
        }
        
        jumpToPC()
    }
}

extension InstrTableView : NSTableViewDataSource {
    
    func numberOfRows(in tableView: NSTableView) -> Int {
        return Int(CPUINFO_INSTR_COUNT);
    }
    
    func tableView(_ tableView: NSTableView, objectValueFor tableColumn: NSTableColumn?, row: Int) -> Any? {
        
        guard let addr = addrInRow[row] else { return nil }
        
        switch tableColumn?.identifier.rawValue {
            
        case "break" where cpu!.hasDisabledBreakpoint(at: addr):
            return "\u{26AA}" // "⚪"
            // return "\u{2B55}" // "⭕"
        case "break" where cpu!.hasBreakpoint(at: addr):
            // return "\u{1F534}" // "🔴"
            return "\u{26D4}" // "⛔"
        case "addr":
            if var info = cpu?.getInstr(row) {
                return String(cString: &info.addr.0)
            }
        case "data":
            if var info = cpu?.getInstr(row) {
                return String(cString: &info.data.0)
            }
        case "instr":
            if var info = cpu?.getInstr(row) {
                return String(cString: &info.instr.0)
            }

        default:
            return "???"
        }
    
        return "??"
    }
}

extension InstrTableView : NSTableViewDelegate {
    
    func tableView(_ tableView: NSTableView, willDisplayCell cell: Any, for tableColumn: NSTableColumn?, row: Int) {
        
        let cell = cell as! NSTextFieldCell
        
        if let addr = addrInRow[row] {

            if cpu!.hasDisabledBreakpoint(at: addr) {
                cell.textColor = NSColor.disabledControlTextColor
            } else if cpu!.hasConditionalBreakpoint(at: addr) {
                cell.textColor = NSColor.systemOrange
            } else if cpu!.hasBreakpoint(at: addr) {
                cell.textColor = NSColor.systemRed
            } else {
                cell.textColor = NSColor.labelColor
            }
        }
    }
}
