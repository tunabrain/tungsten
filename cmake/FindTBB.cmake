find_path(TBB_INCLUDE_DIR NAMES tbb/tbb.h)
find_library(TBB_LIBRARY NAMES tbb)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(TBB REQUIRED_VARS TBB_INCLUDE_DIR TBB_LIBRARY)

set(TBB_LIBRARIES ${TBB_LIBRARY})
set(TBB_INCLUDE_DIRS ${TBB_INCLUDE_DIR})

mark_as_advanced(TBB_INCLUDE_DIR TBB_LIBRARY)
