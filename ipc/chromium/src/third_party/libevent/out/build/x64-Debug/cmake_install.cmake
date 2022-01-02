# Install script for directory: C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Debug")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xlibx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event.lib")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib" TYPE STATIC_LIBRARY FILES "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/lib/event.lib")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/keyvalq_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/listener.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/thread.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/util.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/visibility.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event-config.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_ssl.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/keyvalq_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/listener.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/thread.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/util.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/visibility.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/include/event2/event-config.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_ssl.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xlibx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_core.lib")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib" TYPE STATIC_LIBRARY FILES "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/lib/event_core.lib")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/keyvalq_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/listener.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/thread.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/util.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/visibility.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event-config.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_ssl.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/keyvalq_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/listener.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/thread.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/util.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/visibility.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/include/event2/event-config.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_ssl.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xlibx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib/event_extra.lib")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/lib" TYPE STATIC_LIBRARY FILES "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/lib/event_extra.lib")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/buffer_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/dns_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/http_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/keyvalq_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/listener.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/rpc_struct.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/tag_compat.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/thread.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/util.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/visibility.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/event-config.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2/bufferevent_ssl.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event2" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/buffer_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/dns_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/event_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/http_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/keyvalq_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/listener.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/rpc_struct.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/tag_compat.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/thread.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/util.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/visibility.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/include/event2/event-config.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event2/bufferevent_ssl.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/evdns.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/evrpc.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/event.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/evhttp.h;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include/evutil.h")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/include" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/evdns.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/evrpc.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/event.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/evhttp.h"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/include/evutil.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventConfig.cmake;C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventConfigVersion.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake" TYPE FILE FILES
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug//CMakeFiles/LibeventConfig.cmake"
    "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/LibeventConfigVersion.cmake"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xdevx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake")
    file(DIFFERENT EXPORT_FILE_CHANGED FILES
         "$ENV{DESTDIR}C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake"
         "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/CMakeFiles/Export/C_/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake")
    if(EXPORT_FILE_CHANGED)
      file(GLOB OLD_CONFIG_FILES "$ENV{DESTDIR}C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets-*.cmake")
      if(OLD_CONFIG_FILES)
        message(STATUS "Old export file \"$ENV{DESTDIR}C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake\" will be replaced.  Removing files [${OLD_CONFIG_FILES}].")
        file(REMOVE ${OLD_CONFIG_FILES})
      endif()
    endif()
  endif()
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake" TYPE FILE FILES "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/CMakeFiles/Export/C_/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets.cmake")
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
     "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets-debug.cmake")
    if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
      message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
    endif()
    if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
      message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
    endif()
    file(INSTALL DESTINATION "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake" TYPE FILE FILES "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/CMakeFiles/Export/C_/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/install/x64-Debug/cmake/LibeventTargets-debug.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "C:/mozilla-source/gecko-dev/ipc/chromium/src/third_party/libevent/out/build/x64-Debug/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
