#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "cubeb::cubeb" for configuration "Debug"
set_property(TARGET cubeb::cubeb APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(cubeb::cubeb PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C;CXX"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/cubeb.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS cubeb::cubeb )
list(APPEND _IMPORT_CHECK_FILES_FOR_cubeb::cubeb "${_IMPORT_PREFIX}/lib/cubeb.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
