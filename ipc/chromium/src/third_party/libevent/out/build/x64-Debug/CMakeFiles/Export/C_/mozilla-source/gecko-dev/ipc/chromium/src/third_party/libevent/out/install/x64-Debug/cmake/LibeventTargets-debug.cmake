#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "event" for configuration "Debug"
set_property(TARGET event APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(event PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS event )
list(APPEND _IMPORT_CHECK_FILES_FOR_event "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event.lib" )

# Import target "event_core" for configuration "Debug"
set_property(TARGET event_core APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(event_core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_core.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS event_core )
list(APPEND _IMPORT_CHECK_FILES_FOR_event_core "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_core.lib" )

# Import target "event_extra" for configuration "Debug"
set_property(TARGET event_extra APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(event_extra PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_DEBUG "C"
  IMPORTED_LOCATION_DEBUG "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_extra.lib"
  )

list(APPEND _IMPORT_CHECK_TARGETS event_extra )
list(APPEND _IMPORT_CHECK_FILES_FOR_event_extra "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_extra.lib" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
