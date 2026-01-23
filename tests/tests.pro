TEMPLATE = subdirs
CONFIG += ordered

SUBDIRS = \
    xenlib \
    xenadmin-ui

xenlib.file = xenlib/xenlib-tests.pro
xenadmin-ui.file = xenadmin-ui/xenadmin-ui-tests.pro
