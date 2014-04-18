TEMPLATE= subdirs
SUBDIRS= # don't build anything, we're just generating the .qmake.cache file
QMAKE_SUBSTITUTES += .qmake.cache.in
