# Status of code migration from original C#

# Needs work
* Tree view - should show Virtual Disks, Networks in objects view
* Pool HA tab missing
* Performance tab
* Search tab has unfinished options panel
* VM import / export
* Folder and tag views
* Network tab (host) - needs finish and test, especially wizards and properties
* New storage wizard
* HA tab
* Create template from VM
* Create VM from template

# Needs polish
* Console - it works most of the time, but there are random scaling issues during boot, RDP not supported
* UI - menus and toolbar buttons sometime don't refresh on events (unpause -> still shows force shutdown)
* Menu items - some of them may still be missing, but they are mostly fine now
* Events not showing progress bar for each item

# Needs testing
* VM disk resize
* VM disk move
* VM cross pool migration
* Properties of Hosts, VMs and Pools
* Options
* Maintenance mode

# Finished and tested
* Add server
* Connection to pool - redirect from slave to master
* Connection persistence
* Basic VM controls (start / reboot / shutdown)
* New VM wizard
* Clone VM
* VM deleting
* VM live migration
* Pause / Unpause
* Suspend / Resume
* Snapshots
* VM disk management (create / delete / attach / detach)
* SR attach / detach / repair stack
* CD management
* Grouping and search filtering (clicking hosts / VMs in tree view top level)
* Tree view (infrastructure / objects)
* Events history
* Network tab (VM)
* General tab
* Host command -> Restart toolstack
* Memory tabs
* New pool wizard
* NIC tab - bonding

# Known issues
* When quitting the app sometimes the "pending ops" window appears that's empty
* SR in list of VDI we see "base copy" which is hidden in original version