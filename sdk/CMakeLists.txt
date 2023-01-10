

include_directories ("${PROJECT_SOURCE_DIR}/inc"  "${PROJECT_SOURCE_DIR}/common" "${PROJECT_SOURCE_DIR}/sdk")

# inherits version information from top-level CmakeLists.txt

# use_c99()

set(COMMON_SRC
  ../common/bcb_circular_buffer.c
  ../common/bitconverter.c
  ../common/bta_jpg.c
  ../common/bta_oshelper.c 
  ../common/bvq_queue.c
  ../common/calcXYZ.c
  ../common/calc_bilateral.c
  ../common/crc16.c
  ../common/crc32.c
  ../common/crc7.c
  ../common/fifo.c
  ../common/ping.c
  ../common/pthread_helper.c
  ../common/sockets_helper.c
  ../common/timing_helper.c
  ../common/undistort.c
  ../common/utils.c
  ../common/fastBF/fspecial_gauss.c
  ../common/fastBF/imfilter.c
  ../common/fastBF/maxFilter.c
  ../common/fastBF/shiftableBF.c
)

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
  bta_eth_helper.c
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
  ${COMMON_SRC}
  ${USB_SRC}
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
