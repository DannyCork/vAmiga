// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

class Monitor: DialogController {

    // Display colors
    @IBOutlet weak var colCopper: NSColorWell!
    @IBOutlet weak var colBlitter: NSColorWell!
    @IBOutlet weak var colDisk: NSColorWell!
    @IBOutlet weak var colAudio: NSColorWell!
    @IBOutlet weak var colSprites: NSColorWell!
    @IBOutlet weak var colBitplanes: NSColorWell!
    @IBOutlet weak var colCPU: NSColorWell!
    @IBOutlet weak var colRefresh: NSColorWell!
    
    // Bus debugger
    @IBOutlet weak var busEnable: NSButton!
    
    @IBOutlet weak var busCopper: NSButton!
    @IBOutlet weak var busBlitter: NSButton!
    @IBOutlet weak var busDisk: NSButton!
    @IBOutlet weak var busAudio: NSButton!
    @IBOutlet weak var busSprites: NSButton!
    @IBOutlet weak var busBitplanes: NSButton!
    @IBOutlet weak var busCPU: NSButton!
    @IBOutlet weak var busRefresh: NSButton!
        
    @IBOutlet weak var busOpacity: NSSlider!
    @IBOutlet weak var busDisplayMode: NSPopUpButton!
    
    // Activity monitors
    @IBOutlet weak var monEnable: NSButton!
    
    @IBOutlet weak var monCopper: NSButton!
    @IBOutlet weak var monBlitter: NSButton!
    @IBOutlet weak var monDisk: NSButton!
    @IBOutlet weak var monAudio: NSButton!
    @IBOutlet weak var monSprites: NSButton!
    @IBOutlet weak var monBitplanes: NSButton!

    @IBOutlet weak var monChipRam: NSButton!
    @IBOutlet weak var monSlowRam: NSButton!
    @IBOutlet weak var monFastRam: NSButton!
    @IBOutlet weak var monKickRom: NSButton!

    @IBOutlet weak var monLeftWave: NSButton!
    @IBOutlet weak var monRightWave: NSButton!

    @IBOutlet weak var monOpacity: NSSlider!
    @IBOutlet weak var monLayout: NSPopUpButton!
    @IBOutlet weak var monSlider: NSSlider!

    // Pixel synthesis
    @IBOutlet weak var synEnable: NSButton!
    @IBOutlet weak var synSprite0: NSButton!
    @IBOutlet weak var synSprite1: NSButton!
    @IBOutlet weak var synSprite2: NSButton!
    @IBOutlet weak var synSprite3: NSButton!
    @IBOutlet weak var synSprite4: NSButton!
    @IBOutlet weak var synSprite5: NSButton!
    @IBOutlet weak var synSprite6: NSButton!
    @IBOutlet weak var synSprite7: NSButton!
    @IBOutlet weak var synPlayfield1: NSButton!
    @IBOutlet weak var synPlayfield2: NSButton!
    @IBOutlet weak var synOpacity: NSSlider!

    var layers: Int {
        get { return amiga.getConfig(OPT_HIDDEN_LAYERS) }
        set { amiga.configure(OPT_HIDDEN_LAYERS, value: newValue) }
    }
    var layerAlpha: Int {
        get { return 255 - amiga.getConfig(OPT_HIDDEN_LAYER_ALPHA) }
        set { amiga.configure(OPT_HIDDEN_LAYER_ALPHA, value: 255 - newValue) }
    }
    
    override func awakeFromNib() {

        track()
        super.awakeFromNib()
        refresh()
    }

    func refresh() {
        
        func enabled(_ monitor: Int) -> NSControl.StateValue {
            return parent.renderer.monitorEnabled[monitor] ? .on : .off
        }
        
        let info = amiga.agnus.getDebuggerInfo()
        let bus = info.enabled
        let mon = parent.renderer.drawActivityMonitors
        let syn = synEnable.state == .on
        let col = bus || mon
        
        let visualize = [
            info.visualize.0, info.visualize.1,
            info.visualize.2, info.visualize.3,
            info.visualize.4, info.visualize.5,
            info.visualize.6, info.visualize.7,
            info.visualize.8, info.visualize.9,
            info.visualize.10, info.visualize.11,
            info.visualize.12, info.visualize.13
        ]
        let rgb = [
            info.colorRGB.0, info.colorRGB.1,
            info.colorRGB.2, info.colorRGB.3,
            info.colorRGB.4, info.colorRGB.5,
            info.colorRGB.6, info.colorRGB.7,
            info.colorRGB.8, info.colorRGB.9,
            info.colorRGB.10, info.colorRGB.11,
            info.colorRGB.12, info.colorRGB.13
        ]
        
        // Bus debugger
        busEnable.state = bus ? .on : .off
        busBlitter.state = visualize[Int(BUS_BLITTER.rawValue)] ? .on : .off
        busCopper.state = visualize[Int(BUS_COPPER.rawValue)] ? .on : .off
        busDisk.state = visualize[Int(BUS_DISK.rawValue)] ? .on : .off
        busAudio.state = visualize[Int(BUS_AUDIO.rawValue)] ? .on : .off
        busSprites.state = visualize[Int(BUS_SPRITE.rawValue)] ? .on : .off
        busBitplanes.state = visualize[Int(BUS_BPL1.rawValue)] ? .on : .off
        busCPU.state = visualize[Int(BUS_CPU.rawValue)] ? .on : .off
        busRefresh.state = visualize[Int(BUS_REFRESH.rawValue)] ? .on : .off
        busOpacity.doubleValue = info.opacity * 100.0
        busDisplayMode.selectItem(withTag: info.displayMode.rawValue)
        busBlitter.isEnabled = bus
        busCopper.isEnabled = bus
        busDisk.isEnabled = bus
        busAudio.isEnabled = bus
        busSprites.isEnabled = bus
        busBitplanes.isEnabled = bus
        busCPU.isEnabled = bus
        busRefresh.isEnabled = bus
        busOpacity.isEnabled = bus
        busDisplayMode.isEnabled = bus
        
        // Activity monitors
        monEnable.state = mon ? .on : .off
        monCopper.state = enabled(Renderer.Monitor.copper)
        monBlitter.state = enabled(Renderer.Monitor.blitter)
        monDisk.state = enabled(Renderer.Monitor.disk)
        monAudio.state = enabled(Renderer.Monitor.audio)
        monSprites.state = enabled(Renderer.Monitor.sprite)
        monBitplanes.state = enabled(Renderer.Monitor.bitplane)
        monChipRam.state = enabled(Renderer.Monitor.chipRam)
        monSlowRam.state = enabled(Renderer.Monitor.slowRam)
        monFastRam.state = enabled(Renderer.Monitor.fastRam)
        monKickRom.state = enabled(Renderer.Monitor.kickRom)
        monLeftWave.state = enabled(Renderer.Monitor.waveformL)
        monRightWave.state = enabled(Renderer.Monitor.waveformR)
        monOpacity.floatValue = parent.renderer.monitorGlobalAlpha * 100.0
        monSlider.floatValue = parent.renderer.monitors[0].angle
        monLayout.selectItem(withTag: parent.renderer.monitorLayout)
        monCopper.isEnabled = mon
        monBlitter.isEnabled = mon
        monDisk.isEnabled = mon
        monAudio.isEnabled = mon
        monSprites.isEnabled = mon
        monBitplanes.isEnabled = mon
        monChipRam.isEnabled = mon
        monSlowRam.isEnabled = mon
        monFastRam.isEnabled = mon
        monKickRom.isEnabled = mon
        monLeftWave.isEnabled = mon
        monRightWave.isEnabled = mon
        monOpacity.isEnabled = mon
        monSlider.isEnabled = mon
        monLayout.isEnabled = mon
        
        // Colors
        colBlitter.setColor(rgb[Int(BUS_BLITTER.rawValue)])
        colCopper.setColor(rgb[Int(BUS_COPPER.rawValue)])
        colDisk.setColor(rgb[Int(BUS_DISK.rawValue)])
        colAudio.setColor(rgb[Int(BUS_AUDIO.rawValue)])
        colSprites.setColor(rgb[Int(BUS_SPRITE.rawValue)])
        colBitplanes.setColor(rgb[Int(BUS_BPL1.rawValue)])
        colCPU.setColor(rgb[Int(BUS_CPU.rawValue)])
        colRefresh.setColor(rgb[Int(BUS_REFRESH.rawValue)])
        colBlitter.isHidden = !col
        colCopper.isHidden = !col
        colDisk.isHidden = !col
        colAudio.isHidden = !col
        colSprites.isHidden = !col
        colBitplanes.isHidden = !col
        colCPU.isHidden = !col
        colRefresh.isHidden = !col
        
        // Layers
        synOpacity.integerValue = layerAlpha
        synSprite0.isEnabled = syn
        synSprite1.isEnabled = syn
        synSprite2.isEnabled = syn
        synSprite3.isEnabled = syn
        synSprite4.isEnabled = syn
        synSprite5.isEnabled = syn
        synSprite6.isEnabled = syn
        synSprite7.isEnabled = syn
        synPlayfield1.isEnabled = syn
        synPlayfield2.isEnabled = syn
        synOpacity.isEnabled = syn
    }

    func updateHiddenLayers() {
                        
        var mask = 0
        
        if synEnable.state == .on {
            if synSprite0.state == .on { mask |= 0x01 }
            if synSprite1.state == .on { mask |= 0x02 }
            if synSprite2.state == .on { mask |= 0x04 }
            if synSprite3.state == .on { mask |= 0x08 }
            if synSprite4.state == .on { mask |= 0x10 }
            if synSprite5.state == .on { mask |= 0x20 }
            if synSprite6.state == .on { mask |= 0x40 }
            if synSprite7.state == .on { mask |= 0x80 }
            if synPlayfield1.state == .on { mask |= 0x100 }
            if synPlayfield2.state == .on { mask |= 0x200 }
        }

        layers = mask
        layerAlpha = synOpacity.integerValue
    }
    
    //
    // Action methods
    //
    
    func busOwners(forTag tag: Int) -> [BusOwner] {
        
        switch tag {
        case 0: return [BUS_COPPER]
        case 1: return [BUS_BLITTER]
        case 2: return [BUS_DISK]
        case 3: return [BUS_AUDIO]
        case 4: return [BUS_SPRITE]
        case 5: return [BUS_BPL1, BUS_BPL2, BUS_BPL3, BUS_BPL4, BUS_BPL5, BUS_BPL6]
        case 6: return [BUS_CPU]
        case 7: return [BUS_REFRESH]
        default: fatalError()
        }
    }

    @IBAction func colorAction(_ sender: NSColorWell!) {
        
        let r = Double(sender.color.redComponent)
        let g = Double(sender.color.greenComponent)
        let b = Double(sender.color.blueComponent)
        
        for owner in busOwners(forTag: sender.tag) {
            amiga.agnus.dmaDebugSetColor(owner, r: r, g: g, b: b)
        }
        
        let monitor = parent.renderer.monitors[sender.tag]
        monitor.setColor(sender.color)
        
        refresh()
    }

    @IBAction func busEnableAction(_ sender: NSButton!) {
        
        amiga.agnus.dmaDebugSetEnable(sender.state == .on)
        refresh()
    }
    
    @IBAction func busDisplayAction(_ sender: NSButton!) {
        
        for owner in busOwners(forTag: sender.tag) {
            amiga.agnus.dmaDebugSetVisualize(owner, value: sender.state == .on)
        }
        refresh()
    }
    
    @IBAction func busDisplayModeAction(_ sender: NSPopUpButton!) {
        
        amiga.agnus.dmaDebugSetDisplayMode(sender.selectedTag())
        refresh()
    }
    
    @IBAction func busOpacityAction(_ sender: NSSlider!) {
        
        amiga.agnus.dmaDebugSetOpacity(sender.doubleValue / 100.0)
        refresh()
    }
    
    @IBAction func monEnableAction(_ sender: NSButton!) {
    
        parent.renderer.drawActivityMonitors = sender.state == .on
        refresh()
    }
    
    @IBAction func monDisplayAction(_ sender: NSButton!) {
        
        track("\(sender.tag)")
        parent.renderer.monitorEnabled[sender.tag] = sender.state == .on
        refresh()
    }
    
    @IBAction func monLayoutAction(_ sender: NSPopUpButton!) {
        
        track("\(sender.selectedTag())")
        parent.renderer.monitorLayout = sender.selectedTag()
        refresh()
    }
    
    @IBAction func monRotationAction(_ sender: NSSlider!) {
        
        track()
        for i in 0 ..< parent.renderer.monitors.count {
            parent.renderer.monitors[i].angle = sender.floatValue
        }
        refresh()
    }
    
    @IBAction func monOpacityAction(_ sender: NSSlider!) {
        
        parent.renderer.monitorGlobalAlpha = sender.floatValue / 100.0
        refresh()
    }

    @IBAction func synAction(_ sender: NSButton!) {
        
        updateHiddenLayers()
        refresh()
    }

    @IBAction func synOpacityAction(_ sender: NSSlider!) {
        
        updateHiddenLayers()
        refresh()
    }
}

extension Monitor: NSWindowDelegate {

    func windowWillClose(_ notification: Notification) {

        track("Closing monitor")
    }
}

extension NSColorWell {
    
    func setColor(_ rgb: (Double, Double, Double) ) {
        
        color = NSColor.init(r: rgb.0, g: rgb.1, b: rgb.2)
    }
}
