/* socketpair.c
Copyright 2007, 2010 by Nathan C. Myers <ncm@cantrip.org>
Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

    The name of the author must not be used to endorse or promote products
    derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Changes:
 * 2022-11-04: align types of arguments and return value of Windows Sockets 2
 *             functions with Microsoft documentation
 * 2014-02-12: merge David Woodhouse, Ger Hobbelt improvements
 *     git.infradead.org/users/dwmw2/openconnect.git/commitdiff/bdeefa54
 *     github.com/GerHobbelt/selectable-socketpair
 *   always init the socks[] to -1/INVALID_SOCKET on error, both on Win32/64
 *   and UNIX/other platforms
 * 2013-07-18: Change to BSD 3-clause license
 * 2010-03-31:
 *   set addr to 127.0.0.1 because win32 getsockname does not always set it.
 * 2010-02-25:
 *   set SO_REUSEADDR option to avoid leaking some windows resource.
 *   Windows System Error 10049, "Event ID 4226 TCP/IP has reached
 *   the security limit imposed on the number of concurrent TCP connect
 *   attempts."  Bleah.
 * 2007-04-25:
 *   preserve value of WSAGetLastError() on all error returns.
 * 2007-04-22:  (Thanks to Matthew Gregan <kinetik@flim.org>)
 *   s/EINVAL/WSAEINVAL/ fix trivial compile failure
 *   s/socket/WSASocket/ enable creation of sockets suitable as stdin/stdout
 *     of a child process.
 *   add argument make_overlapped
 */

#include <string.h>

#ifdef WIN32
# include <winsock2.h>
# include <windows.h>
# include <io.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <errno.h>
#endif

#ifdef WIN32

/* dumb_socketpair:
 *   If make_overlapped is nonzero, both sockets created will be usable for
 *   "overlapped" operations via WSASend etc.  If make_overlapped is zero,
 *   socks[0] (only) will be usable with regular ReadFile etc., and thus
 *   suitable for use as stdin or stdout of a child process.  Note that the
 *   sockets must be closed with closesocket() regardless.
 */

int dumb_socketpair(SOCKET socks[2], int make_overlapped)
{
    union {
       struct sockaddr_in inaddr;
       struct sockaddr addr;
    } a;
    SOCKET listener;
    int addrlen = (int) sizeof(a.inaddr);
    DWORD flags = (make_overlapped ? WSA_FLAG_OVERLAPPED : 0);
    DWORD reuse = 1;
    int e;

    if (socks == 0) {
      WSASetLastError(WSAEINVAL);
      return SOCKET_ERROR;
    }
    socks[0] = socks[1] = INVALID_SOCKET;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET)
        return SOCKET_ERROR;

    memset(&a, 0, sizeof(a));
    a.inaddr.sin_family = AF_INET;
    a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.inaddr.sin_port = 0;

    for (;;) {
        if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
               (const char*) &reuse, (int) sizeof(reuse)) == SOCKET_ERROR)
            break;
        if  (bind(listener, &a.addr, (int) sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        memset(&a, 0, sizeof(a));
        if  (getsockname(listener, &a.addr, &addrlen) == SOCKET_ERROR)
            break;
        // win32 getsockname may only set the port number, p=0.0005.
        // ( http://msdn.microsoft.com/library/ms738543.aspx ):
        a.inaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        a.inaddr.sin_family = AF_INET;

        if (listen(listener, 1) == SOCKET_ERROR)
            break;

        socks[0] = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, flags);
        if (socks[0] == INVALID_SOCKET)
            break;
        if (connect(socks[0], &a.addr, (int) sizeof(a.inaddr)) == SOCKET_ERROR)
            break;

        socks[1] = accept(listener, NULL, NULL);
        if (socks[1] == INVALID_SOCKET)
            break;

        closesocket(listener);
        return 0;
    }

    e = WSAGetLastError();
    closesocket(listener);
    closesocket(socks[0]);
    closesocket(socks[1]);
    WSASetLastError(e);
    socks[0] = socks[1] = INVALID_SOCKET;
    return SOCKET_ERROR;
}
#else
int dumb_socketpair(int socks[2], int dummy)
{
    if (socks == 0) {
        errno = EINVAL;
        return -1;
    }
    dummy = socketpair(AF_LOCAL, SOCK_STREAM, 0, socks);
    if (dummy)
        socks[0] = socks[1] = -1;
    return dummy;
}
#endif
