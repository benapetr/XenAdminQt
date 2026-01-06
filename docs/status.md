# Status of code migration from original C#

# Needs work
* Menu items
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
* New VM wizard
* VM deleting
* HA tab
* NIC tab - bonding
* Clone VM
* Create template from VM
* Create VM from template

# Needs polish
* General tab - shows data, but access to data is weird (should use native XenObjects and their properties instead of scrapping QVarianMaps), overall layout is also not good
* Memory tabs - they already work, but could look better
* Console - it works most of the time, but there are random scaling issues during boot, RDP not supported
* UI - menus and toolbar buttons sometime don't refresh on events (unpause -> still shows force shutdown)

# Needs testing
* VM disk resize
* VM disk move
* VM live migration
* VM cross pool migration
* Properties of Hosts, VMs and Pools
* Options
* Maintenance mode

# Finished and tested
* Add server
* Connection to pool - redirect from slave to master
* Connection persistence
* Basic VM controls (start / reboot / shutdown)
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
* Host command -> Restart toolstack

