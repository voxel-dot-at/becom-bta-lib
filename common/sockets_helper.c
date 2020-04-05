#include "sockets_helper.h"


#ifdef PLAT_WINDOWS
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#elif defined PLAT_LINUX
#   include <errno.h>
#endif

int getLastSocketError() {
#   ifdef PLAT_WINDOWS
    return WSAGetLastError();
#   elif defined PLAT_LINUX
    return errno;
#   endif
}