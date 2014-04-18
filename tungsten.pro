TEMPLATE = subdirs
CONFIG += ordered
SUBDIRS = initvars.pro src/core src/editor src/obj2json

editor.depends = core
obj2json.depends = core