// -----------------------------------------------------------------------------
// This file is part of vAmiga
//
// Copyright (C) Dirk W. Hoffmann. www.dirkwhoffmann.de
// Licensed under the GNU General Public License v3
//
// See https://www.gnu.org for license information
// -----------------------------------------------------------------------------

class MyToolbar: NSToolbar {
    
    @IBOutlet weak var parent: MyController!
    
    // Toolbar items
    @IBOutlet weak var controlPort1: NSPopUpButton!
    @IBOutlet weak var controlPort2: NSPopUpButton!
    @IBOutlet weak var powerButton: NSToolbarItem!
    @IBOutlet weak var pauseButton: NSToolbarItem!
    @IBOutlet weak var resetButton: NSToolbarItem!
    @IBOutlet weak var keyboardButton: NSToolbarItem!
    @IBOutlet weak var snapshotSegCtrl: NSSegmentedControl!
    
    override func validateVisibleItems() {

        let amiga = parent.amiga!
        let pause = pauseButton.view as? NSButton
        let reset = resetButton.view as? NSButton

        // Disable the Pause and Reset button if the emulator if powered off
        let poweredOn = amiga.isPoweredOn
        pause?.isEnabled = poweredOn
        reset?.isEnabled = poweredOn

        // Adjust the appearance of the Pause button
        if amiga.isRunning {
            pause?.image = NSImage.init(named: "pauseTemplate")
            pauseButton.label = "Pause"
        } else {
            pause?.image = NSImage.init(named: "continueTemplate")
            pauseButton.label = "Run"
        }

        // Change the label of reset button. If we don't do that, the
        // label color does not change (at least in macOS Mojave)
        resetButton.label = ""
        resetButton.label = "Reset"

        // Update input device selectors
        parent.gamePadManager.refresh(popup: controlPort1)
        parent.gamePadManager.refresh(popup: controlPort2)
        controlPort1.selectItem(withTag: parent.config.gameDevice1)
        controlPort2.selectItem(withTag: parent.config.gameDevice2)
    }
    
    @IBAction func toolbarPrefAction(_ sender: NSSegmentedControl) {

        switch sender.selectedSegment {

        case 0: parent.preferencesAction(sender)
        case 1: parent.configureAction(sender)

        default: assert(false)
        }
    }
}
