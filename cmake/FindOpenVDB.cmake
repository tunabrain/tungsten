find_path(OPENVDB_INCLUDE_DIR NAMES openvdb/openvdb.h)
find_library(OPENVDB_LIBRARY NAMES openvdb)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(OpenVDB REQUIRED_VARS OPENVDB_INCLUDE_DIR OPENVDB_LIBRARY)

set(OPENVDB_LIBRARIES ${OPENVDB_LIBRARY})
set(OPENVDB_INCLUDE_DIRS ${OPENVDB_INCLUDE_DIR})

mark_as_advanced(OPENVDB_INCLUDE_DIR OPENVDB_LIBRARY)
