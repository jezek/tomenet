/*
 * This is a general "fill-in-the-blanks" network module based on SDL_net.
 *
 * It is designed to work cross-platform (Linux, Windows, etc.) using the SDL2 networking library.
 * All functions have been implemented using SDL_net calls. Note that some socket options (e.g. buffer sizes,
 * TCP_NODELAY) are not directly supported by SDL_net. In these cases, stub implementations are provided.
 */

#define _SOCKLIB_LIBSOURCE

#include "angband.h"
#include "version.h"
#include "net-sdl2.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <errno.h>

/* Debug macro */
#ifdef DEBUG
 #define DEB(x) x
#else
 #define DEB(x)
#endif

/* Default timeout values */
#define DEFAULT_S_TIMEOUT_VALUE        10
#define DEFAULT_US_TIMEOUT_VALUE       0

/* Default retry value */
#define DEFAULT_RETRIES                5

/* Global socklib errno variable */
int sl_errno = 0;

/* Global timeout variables (seconds and microseconds) */
int sl_timeout_s = DEFAULT_S_TIMEOUT_VALUE;
int sl_timeout_us = DEFAULT_US_TIMEOUT_VALUE;
/* Mapping table for TCPsocket pointers */
#define MAX_TCP_FD 1024
static TCPsocket tcp_socket_table[MAX_TCP_FD] = { NULL };
static int tcp_error_table[MAX_TCP_FD] = { 0 };
static int invalid_fd_error = 0;



/* Forward declarations */
static int addTCPsocket(TCPsocket sock);
static TCPsocket fd2TCPsocket(int fd);
static void delTCPsocket(int fd);

/*
 *******************************************************************************
 *
 *	SetTimeout()
 *
 *******************************************************************************
 *
 * Description:
 *	Sets the global timeout value to s seconds and us microseconds.
 *
 * Input Parameters:
 *	s   - Timeout value in seconds.
 *	us  - Timeout value in microseconds.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	void.
 *
 */
void SetTimeout(int s, int us)
{
	sl_timeout_s = s;
	sl_timeout_us = us;
} /* SetTimeout */


/*
 *******************************************************************************
 *
 *	CreateClientSocket()
 *
 *******************************************************************************
 * Description:
 *	Creates a client TCP/IP socket in the Internet domain.
 *
 * Input Parameters:
 *	host	- Pointer to a string containing the peer host name.
 *	port	- The port number to connect to.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	Returns the file descriptor (index in our mapping table) or -1 on error.
 *
 * Globals Referenced
 *	sl_errno	- If errors occured: SL_EHOSTNAME, SL_ESOCKET, SL_ECONNECT (when running out of internal fd).
 *	invalid_fd_error - If errors occured, same as sl_errno.
 *	tcp_error_table - If no error, clears error for fd.
 *
 * External Calls
 *	SDLNet_ResolveHost
 *	SDLNet_TCP_Open
 *	SDLNet_TCP_Close
 *
 */
int CreateClientSocket(char *host, int port)
{
	IPaddress ip;
	TCPsocket sock;
	int fd;

	invalid_fd_error = 0;
	if (SDLNet_ResolveHost(&ip, host, port) < 0) {
		sl_errno = SL_EHOSTNAME;
		invalid_fd_error = sl_errno;
		return -1;
	}

	sock = SDLNet_TCP_Open(&ip);
	if (sock == NULL) {
		sl_errno = SL_ESOCKET;
		invalid_fd_error = sl_errno;
		return -1;
	}

	fd = addTCPsocket(sock);
	if (fd == -1) {
		SDLNet_TCP_Close(sock);
		sl_errno = SL_ECONNECT;
		invalid_fd_error = sl_errno;
		return -1;
	}
	tcp_error_table[fd] = 0;
	return fd;
} /* CreateClientSocket */


/*
 *******************************************************************************
 *
 *	GetPortNum()
 *
 *******************************************************************************
 * Description:
 *	Returns the port number of a TCP socket connection.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our mapping table).
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	The port number in host byte order, or -1 on failure.
 *
 * Globals Referenced
 *	tcp_error_table	- If errors occured sets the SL_ESOCKET, SL_ECONNECT error for fd.
 *	invalid_fd_error	- If fd is invalid, sets the SL_ESOCKET error.
 *
 * External Calls
 *	SDLNet_TCP_GetPeerAddress
 *	SDL_SwapBE16
 */
int GetPortNum(int fd)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		if ((fd >= 0) && (fd < MAX_TCP_FD)) tcp_error_table[fd] = SL_ESOCKET;
		else invalid_fd_error = SL_ESOCKET;
		return -1;
	}
	IPaddress *ip = SDLNet_TCP_GetPeerAddress(sock);
	if (ip == NULL) {
		tcp_error_table[fd] = SL_ECONNECT;
		return -1;
	}
	/* ip->port is in network byte order; convert to host order */
	Uint16 port = SDL_SwapBE16(ip->port);
	tcp_error_table[fd] = 0;
	return port;
} /* GetPortNum */





/*
 *******************************************************************************
 *
 *	GetSocketError()
 *
 *******************************************************************************
 * Description:
 *	Clear the error status for the socket and return the error in errno.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our mapping table).
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	-1 if there was an error stored for fd, 0 otherwise.
 *
 * Globals Referenced
 *	errno	- stores the error or 0 if none.
 *	tcp_error_table	- clears the error for fd after storing to errno.
 *	invalid_fd_error	- clears the error for fd after storing to errno.
 *
 */
int GetSocketError(int fd)
{
	if ((fd >= 0) && (fd < MAX_TCP_FD)) {
		errno = tcp_error_table[fd];
		tcp_error_table[fd] = 0;
	} else {
		errno = invalid_fd_error;
		invalid_fd_error = 0;
	}
	if (errno != 0) return -1;
	return 0;
} /* GetSocketError */


/*
 *******************************************************************************
 *
 *	SocketReadable()
 *
 *******************************************************************************
 * Description:
 *	Checks if data is available on the TCP/IP socket.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our mapping table).
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	1 if readable, 0 if not, -1 on error.
 *
 * Globals Referenced
 *	tcp_error_table	- If errors occured for fd: SL_ESOCKET, SL_ECONNECT, SL_EBIND, SL_ERECEIVE.
 *
 * External Calls
 *	SDLNet_AllocSocketSet
 *	SDLNet_TCP_AddSocket
 *	SDLNet_CheckSockets
 *	SDLNet_FreeSocketSet
 *	SDLNet_DelSocket
 *
 */
int SocketReadable(int fd)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		if ((fd >= 0) && (fd < MAX_TCP_FD)) tcp_error_table[fd] = SL_ESOCKET;
		else invalid_fd_error = SL_ESOCKET;
		return -1;
	}
	SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
	if (set == NULL) {
		tcp_error_table[fd] = SL_ECONNECT;
		return -1;
	}
	if (SDLNet_TCP_AddSocket(set, sock) == -1) {
		SDLNet_FreeSocketSet(set);
		tcp_error_table[fd] = SL_EBIND;
		return -1;
	}
	/* Calculate timeout in milliseconds */
	int timeout_ms = sl_timeout_s * 1000 + sl_timeout_us / 1000;
	int ret = SDLNet_CheckSockets(set, timeout_ms);
	SDLNet_DelSocket(set, (SDLNet_GenericSocket)sock);
	SDLNet_FreeSocketSet(set);
	if (ret > 0) {
		tcp_error_table[fd] = 0;
		return 1;
	} else if (ret == 0) {
		tcp_error_table[fd] = 0;
		return 0;
	} else {
		tcp_error_table[fd] = SL_ERECEIVE;
		return -1;
	}
} /* SocketReadable */


/*
 *******************************************************************************
 *
 *	SocketRead()
 *
 *******************************************************************************
 * Description:
 *	Receives up to <size> bytes from the TCP/IP socket into <buf>.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our mapping table).
 *	buf	- Pointer to the buffer to store received data.
 *	size	- Maximum number of bytes to read.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	Number of bytes received, or -1 on error.
 *
 */
int SocketRead(int fd, char *buf, int size)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		tcp_error_table[fd] = SL_ESOCKET;
		return -1;
	}
	int bytes = SDLNet_TCP_Recv(sock, buf, size);
	if (bytes <= 0) {
		if (bytes == 0) errno = EAGAIN;
		else errno = EIO;
		tcp_error_table[fd] = SL_ERECEIVE;
		return -1;
	}
	tcp_error_table[fd] = 0;
	return bytes;
} /* SocketRead */


/*
 *******************************************************************************
 *
 *  SocketWrite()
 *
 *******************************************************************************
 * Description:
 *  Sends data on a connected TCP/IP socket.
 *
 * Input Parameters:
 *  fd    - The file descriptor (index in our mapping table).
 *  wbuf  - Pointer to the message buffer to send.
 *  size  - Size of the data to send.
 *
 * Output Parameters:
 *  None.
 *
 * Return Value:
 *  Number of bytes sent, or -1 on error.
 *
 */
int SocketWrite(int fd, char *wbuf, int size)
{
	/* Retrieve the TCP socket from the mapping table */
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		if ((fd >= 0) && (fd < MAX_TCP_FD)) tcp_error_table[fd] = SL_ESOCKET;
		else invalid_fd_error = SL_ESOCKET;
		return -1;
	}

	/* Send data over the TCP socket */
	int bytes = SDLNet_TCP_Send(sock, wbuf, size);

	/* Check if the send operation was successful.
	 * If bytes sent is less than the requested size, treat it as an error.
	 */
	if (bytes <= 0) {
		if (bytes == 0) errno = EAGAIN;
		else errno = EIO;
		tcp_error_table[fd] = SL_ECONNECT;
		return -1;
	} else if (bytes < size) {
		errno = EIO;
		tcp_error_table[fd] = SL_ECONNECT;
		return bytes;
	}

	tcp_error_table[fd] = 0;
	return bytes;
} /* SocketWrite */


/*
 *******************************************************************************
 *
 *	GetLocalHostName()
 *
 *******************************************************************************
 * Description:
 *	Returns the Fully Qualified Domain Name (FQDN) for the local host.
 *
 * Input Parameters:
 *	name	- Pointer to an array to store the hostname.
 *	size	- Size of the array.
 *
 * Output Parameters:
 *	The hostname is copied into the provided array.
 *
 * Return Value:
 *	None.
 *
 * Note:
 *	As SDL_net does not provide a native method to obtain the local host name,
 *	this implementation uses a fallback value.
 *
 */
void GetLocalHostName(char *name, unsigned size)
{
	strncpy(name, "127.0.0.1", size);
	name[size - 1] = '\0';
} /* GetLocalHostName */


/*
 *******************************************************************************
 *
 *	SocketClose()
 *
 *******************************************************************************
 * Description:
 *	Performs a graceful shutdown and close on a TCP/IP socket.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our mapping table).
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	1 on success, -1 on error.
 *
 */
int SocketClose(int fd)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		if ((fd >= 0) && (fd < MAX_TCP_FD)) tcp_error_table[fd] = SL_ECLOSE;
		else invalid_fd_error = SL_ECLOSE;
		return -1;
	}
	SDLNet_TCP_Close(sock);
	delTCPsocket(fd);
	tcp_error_table[fd] = 0;
	return 1;
} /* SocketClose */

/*
 *******************************************************************************
 *
 * TCP socket helper functions.
 *
 *******************************************************************************
 */


/*
 *******************************************************************************
 *
 * addTCPsocket()
 *
 *******************************************************************************
 * Description:
 *	Adds a TCPsocket to the mapping table.
 *	Returns the file descriptor (index) if successful, or -1 if the table is full.
 *
 */
static int addTCPsocket(TCPsocket sock)
{
	for (int i = 0; i < MAX_TCP_FD; i++) {
		if (tcp_socket_table[i] == NULL) {
			tcp_socket_table[i] = sock;
			tcp_error_table[i] = 0;
			return i;
		}
	}
	return -1; /* Table is full */
}

/*
 * fd2TCPsocket:
 *  Retrieves the TCPsocket pointer corresponding to the given file descriptor.
 *  Returns NULL if the fd is out of range or if no socket is stored at that index.
 */
static TCPsocket fd2TCPsocket(int fd)
{
	if ((fd < 0) || (fd >= MAX_TCP_FD)) {
		return NULL;
	}
	return tcp_socket_table[fd];
}

/*
 * delTCPsocket:
 *  Removes the TCPsocket associated with the given file descriptor from the mapping table.
 */
static void delTCPsocket(int fd)
{
	if ((fd >= 0) && (fd < MAX_TCP_FD)) {
		tcp_socket_table[fd] = NULL;
		tcp_error_table[fd] = 0;
	}
}

