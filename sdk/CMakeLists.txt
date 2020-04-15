

include_directories ("${PROJECT_SOURCE_DIR}/inc"  "${PROJECT_SOURCE_DIR}/common" "${PROJECT_SOURCE_DIR}/sdk")

# inherits version information from top-level CmakeLists.txt


  message("plat a " ${PLAT_APPLE})
set(COMMON_SRC ../common/bta_byte_queue.c ../common/bta_jpg.c ../common/bta_oshelper.c 
                ../common/bvq_queue.c ../common/circular_buffer.c ../common/crc16.c ../common/crc32.c ../common/crc7.c ../common/ping.cc ../common/pthread_helper.c ../common/sockets_helper.c ../common/timing_helper.c ../common/undistort.c ../common/utils.c)

file(GLOB HEADERS ../inc/*.h)

message("headers" ${HEADERS} )

set(USB_SRC "")
if (LIBUSB_FOUND) 
  set(USB_SRC bta_usb.c)
endif()

message("usb" ${USB_SRC} )

add_library(bta SHARED ${COMMON_SRC}
   bta.c bta_eth_helper.c	bta_grabbing.c bta_stream.c		bta_uart_helper.c
  bta_eth.c		bta_frame_queueing.c	bta_helper.c bta_uart.c ${USB_SRC} )

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
