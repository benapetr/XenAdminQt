TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS = \
    xenlib \
    xenadmin-ui

# Make sure xenlib is built before xenadmin-ui
xenadmin-ui.depends = xenlib