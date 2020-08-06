#include "sockets_helper.h"


#ifdef PLAT_WINDOWS
#   include <winsock2.h>
#   include <Ws2tcpip.h>
#elif defined PLAT_LINUX || defined PLAT_APPLE
#   include <errno.h>
#endif

int getLastSocketError() {
#   ifdef PLAT_WINDOWS
    return WSAGetLastError();
#   elif defined PLAT_LINUX || defined PLAT_APPLE
    return errno;
#   endif
}
