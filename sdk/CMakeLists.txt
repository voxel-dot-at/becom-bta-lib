

include_directories ("${PROJECT_SOURCE_DIR}/inc"  "${PROJECT_SOURCE_DIR}/common" "${PROJECT_SOURCE_DIR}/sdk")

# inherits version information from top-level CmakeLists.txt

# use_c99()

file(GLOB HEADERS ../inc/*.h)

message("headers" ${HEADERS} )

set(USB_SRC "")
if (LIBUSB_FOUND AND NOT BTA_WO_USB) 
  set(USB_SRC bta_usb.c)
endif()

message(" usb sources: " ${USB_SRC} )

add_library(bta SHARED
  bta.c
  bta_discovery_helper.c
  bta_eth.c
  bta_frame_queueing.c
  bta_grabbing.c
  bta_helper.c
  bta_p100.c
  bta_p100_helper.c
  bta_processing.c
  bta_serialization.c
  bta_stream.c
  bta_uart.c
  bta_uart_helper.c
  bta_usb.c
  $<TARGET_OBJECTS:bltapi_fastBf>
  $<TARGET_OBJECTS:bltapi_lzma>
  $<TARGET_OBJECTS:bltapi_common>
  )

target_link_libraries( bta ${LIBS} ${LIBUSB_LIBRARIES})

set_target_properties(bta PROPERTIES VERSION ${bta_VERSION_STRING}
                                          SOVERSION ${bta_VERSION_MAJOR})


# not needed as this is the project set in the main cmake file:
INSTALL(TARGETS bta 
    DESTINATION lib
  INCLUDES
      DESTINATION "${INSTALL_INCLUDE_DIR}"
)

install(DIRECTORY "../inc/" DESTINATION "include/")
