# Status of code migration from original C#

# Needs work
* Tree view - should show Virtual Disks, Networks in objects view
* Pool HA tab missing
* Templates are being displayed in same way as VMs (all tabs, including console)
* Actions and commands, see actions todo
* Performance tab
* Search tab has unfinished options panel
* VM import / export
* Folder and tag views
* Network tab (host) - needs finish and test, especially wizards and properties
* New pool wizard
* New storage wizard
* VM deleting
* HA tab
* NIC tab - bonding
* Clone VM
* Create template from VM
* Create VM from template

# Needs polish
* Memory tabs - they already work, but could look better, especially list of VMs
* Console - it works most of the time, but there are random scaling issues during boot, RDP not supported
* UI - menus and toolbar buttons sometime don't refresh on events (unpause -> still shows force shutdown)
* Menu items - some of them may still be missing, but they are mostly fine now
* Events not showing progress bar for each item

# Needs testing
* VM disk resize
* VM disk move
* VM cross pool migration
* Properties of Hosts, VMs and Pools
* VM deleting
* Options
* Maintenance mode

# Finished and tested
* Add server
* Connection to pool - redirect from slave to master
* Connection persistence
* Basic VM controls (start / reboot / shutdown)
* New VM wizard
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

# Known issues
* When quitting the app sometimes the "pending ops" window appears that's empty