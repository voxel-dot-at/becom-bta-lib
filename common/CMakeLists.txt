

add_library(bltapi_fastBf OBJECT 
    fastBF/fspecial_gauss.c fastBF/imfilter.c       fastBF/maxFilter.c      fastBF/shiftableBF.c
)

add_library(bltapi_lzma OBJECT 
    lzma/Alloc.c      lzma/LzFind.c           lzma/LzFindOpt.c        lzma/LzmaEnc.c
    lzma/Threads.c    lzma/CpuArch.c          lzma/LzFindMt.c         lzma/LzmaDec.c          lzma/LzmaLib.c
)

add_library(bltapi_common OBJECT 
    bcb_circular_buffer.c   bvq_queue.c             crc32.c                 ping.c                  uart_helper.c
    bitconverter.c          calcXYZ.c               crc7.c                  pthread_helper.c        undistort.c
     bta_jpg.c               calc_bilateral.c        fifo.c                  sockets_helper.c        utils.c
     bta_oshelper.c          crc16.c                 memory_area.c           timing_helper.c
    )
