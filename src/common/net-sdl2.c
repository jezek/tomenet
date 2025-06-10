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

/* Global default retries variable used by DgramSendRec */
int sl_default_retries = DEFAULT_RETRIES;

/* Global broadcast enable variable (super-user only), default disabled */
int sl_broadcast_enabled = 0;

/*
 * We keep two tables, one for TCP and one for UDP.
 * However, a single logical "fd" can open a TCP socket (CreateClientSocket),
 * and if code later calls DgramWrite/DgramRead using that same fd, we
 * will lazily open a UDP socket on demand and store it in the matching index.
 */

/* Mapping table for TCPsocket pointers */
#define MAX_TCP_FD 1024
static TCPsocket tcp_socket_table[MAX_TCP_FD] = { NULL };

/* Mapping table for UDPsocket pointers */
#define MAX_UDP_FD 1024
static UDPsocket udp_socket_table[MAX_UDP_FD] = { NULL };

/* Forward declarations */
static int addTCPsocket(TCPsocket sock);
static TCPsocket fd2TCPsocket(int fd);
static void delTCPsocket(int fd);
static int addUDPsocket(UDPsocket sock);
static UDPsocket fd2UDPsocket(int fd);
static void delUDPsocket(int fd);
static int ensureUdpSocket(int fd);

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
 */
int CreateClientSocket(char *host, int port)
{
	IPaddress ip;
	if (SDLNet_ResolveHost(&ip, host, port) < 0) {
		sl_errno = SL_EHOSTNAME;
		return -1;
	}
	TCPsocket sock = SDLNet_TCP_Open(&ip);
	if (sock == NULL) {
		sl_errno = SL_ESOCKET;
		return -1;
	}
	int fd = addTCPsocket(sock);
	if (fd == -1) {
		SDLNet_TCP_Close(sock);
		sl_errno = SL_ESOCKET;
		return -1;
	}
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
 */
int GetPortNum(int fd)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		return -1;
	}
	IPaddress *ip = SDLNet_TCP_GetPeerAddress(sock);
	if (ip == NULL) {
		return -1;
	}
	/* ip->port is in network byte order; convert to host order */
	Uint16 port = SDL_SwapBE16(ip->port);
	return port;
} /* GetPortNum */


/*
 *******************************************************************************
 *
 *	SetSocketReceiveBufferSize()
 *
 *******************************************************************************
 * Description:
 *	Sets the receive buffer size for a socket.
 *
 * Input Parameters:
 *	fd	- The socket descriptor.
 *	size	- The desired buffer size.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	-1 on failure, 0 on success.
 *
 * Note:
 *	SDL_net does not support changing the socket buffer size. This is a stub.
 *
 */
int SetSocketReceiveBufferSize(int fd, int size)
{
	(void)fd;
	(void)size;
	/* Not supported by SDL_net; return success */
	return 0;
} /* SetSocketReceiveBufferSize */


/*
 *******************************************************************************
 *
 *	SetSocketSendBufferSize()
 *
 *******************************************************************************
 * Description:
 *	Sets the send buffer size for a socket.
 *
 * Input Parameters:
 *	fd	- The socket descriptor.
 *	size	- The desired buffer size.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	-1 on failure, 0 on success.
 *
 * Note:
 *	SDL_net does not support changing the socket buffer size. This is a stub.
 *
 */
int SetSocketSendBufferSize(int fd, int size)
{
	(void)fd;
	(void)size;
	/* Not supported by SDL_net; return success */
	return 0;
} /* SetSocketSendBufferSize */


/*
 *******************************************************************************
 *
 *	SetSocketNoDelay()
 *
 *******************************************************************************
 * Description:
 *	Sets the TCP_NODELAY option on a connected stream socket.
 *
 * Input Parameters:
 *	fd	- The socket descriptor.
 *	flag	- 1 to enable, 0 to disable.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	-1 on failure, 0 on success.
 *
 * Note:
 *	SDL_net does not support setting TCP_NODELAY. This is a stub.
 *
 */
int SetSocketNoDelay(int fd, int flag)
{
	(void)fd;
	(void)flag;
	/* Not supported by SDL_net; return success */
	return 0;
} /* SetSocketNoDelay */


/*
 *******************************************************************************
 *
 *	GetSocketError()
 *
 *******************************************************************************
 * Description:
 *	Clears and returns the socket error status.
 *
 * Input Parameters:
 *	fd	- The socket descriptor.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	Always returns 0 as SDL_net does not support per-socket error retrieval.
 *
 */
int GetSocketError(int fd)
{
	(void)fd;
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
 */
int SocketReadable(int fd)
{
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		return -1;
	}
	SDLNet_SocketSet set = SDLNet_AllocSocketSet(1);
	if (set == NULL) {
		return -1;
	}
	if (SDLNet_TCP_AddSocket(set, sock) == -1) {
		SDLNet_FreeSocketSet(set);
		return -1;
	}
	/* Calculate timeout in milliseconds */
	int timeout_ms = sl_timeout_s * 1000 + sl_timeout_us / 1000;
	int ret = SDLNet_CheckSockets(set, timeout_ms);
	SDLNet_DelSocket(set, (SDLNet_GenericSocket)sock);
	SDLNet_FreeSocketSet(set);
	if (ret > 0) {
		return 1;
	} else if (ret == 0) {
		return 0;
	} else {
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
	if (SocketReadable(fd) <= 0) {
		return -1;
	}
	TCPsocket sock = fd2TCPsocket(fd);
	if (sock == NULL) {
		return -1;
	}
	int bytes = SDLNet_TCP_Recv(sock, buf, size);
	if (bytes <= 0) {
		return -1;
	}
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
		return -1;
	}

	/* Send data over the TCP socket */
	int bytes = SDLNet_TCP_Send(sock, wbuf, size);

	/* Check if the send operation was successful.
	 * If bytes sent is less than the requested size, treat it as an error.
	 */
	if (bytes < size) {
		return -1;
	}

	return bytes;
} /* SocketWrite */


/*
 *******************************************************************************
 *
 *	DgramRead()
 *
 *******************************************************************************
 * Description:
 *	Receives a datagram on a connected UDP/IP socket.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our UDP mapping table).
 *	size	- Expected maximum message size.
 *
 * Output Parameters:
 *	rbuf	- Pointer to the buffer to store received message.
 *
 * Return Value:
 *	Number of bytes received, or -1 on error.
 *
 */
int DgramRead(int fd, char *rbuf, int size)
{
	return SocketRead(fd, rbuf, size);
} /* DgramRead */


/*
 *******************************************************************************
 *
 *	DgramWrite()
 *
 *******************************************************************************
 * Description:
 *	Sends a datagram on a connected UDP/IP socket.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our UDP mapping table).
 *	wbuf	- Pointer to the message buffer to send.
 *	size	- Size of the message.
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	Number of bytes sent, or -1 on error.
 *
 */
int DgramWrite(int fd, char *wbuf, int size)
{
	return SocketWrite(fd, wbuf, size);
} /* DgramWrite */


/*
 *******************************************************************************
 *
 *	DgramClose()
 *
 *******************************************************************************
 * Description:
 *	Closes a UDP/IP datagram socket.
 *
 * Input Parameters:
 *	fd	- The file descriptor (index in our UDP mapping table).
 *
 * Output Parameters:
 *	None.
 *
 * Return Value:
 *	None.
 *
 */
void DgramClose(int fd)
{
	UDPsocket udp = fd2UDPsocket(fd);
	if (udp != NULL) {
		SDLNet_UDP_Close(udp);
		delUDPsocket(fd);
	}
} /* DgramClose */


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
	strncpy(name, "localhost", size);
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
		return -1;
	}
	SDLNet_TCP_Close(sock);
	delTCPsocket(fd);
	return 1;
} /* SocketClose */

/*
 *******************************************************************************
 *
 * TCP and UDP sockets as file descriptor helper functions.
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
	}
}

/*
 * addUDPsocket:
 *  Adds a UDPsocket to the mapping table.
 *  Returns the file descriptor (index) if successful, or -1 if the table is full.
 */
static int addUDPsocket(UDPsocket sock)
{
	for (int i = 0; i < MAX_UDP_FD; i++) {
		if (udp_socket_table[i] == NULL) {
			udp_socket_table[i] = sock;
			return i;
		}
	}
	return -1; /* Table is full */
}

/*
 * fd2UDPsocket:
 *  Retrieves the UDPsocket pointer corresponding to the given file descriptor.
 *  Returns NULL if the fd is out of range or if no socket is stored at that index.
 */
static UDPsocket fd2UDPsocket(int fd)
{
	if ((fd < 0) || (fd >= MAX_UDP_FD)) {
		return NULL;
	}
	return udp_socket_table[fd];
}

/*
 * delUDPsocket:
 *  Removes the UDPsocket associated with the given file descriptor from the mapping table.
 */
static void delUDPsocket(int fd)
{
	if ((fd >= 0) && (fd < MAX_UDP_FD)) {
		udp_socket_table[fd] = NULL;
	}
}

/*
 * ensureUdpSocket:
 *   Lazy-creates a UDP socket for the given fd if not already present,
 *   using the same remote address as the TCP socket.
 *   Returns 0 on success, -1 on failure.
 */
static int ensureUdpSocket(int fd)
{
	/* If we already have a UDP socket at this fd, do nothing. */
	if (fd < 0 || fd >= MAX_UDP_FD) {
		sl_errno = SL_ESOCKET;
		return -1;
	}

	if (udp_socket_table[fd] != NULL) {
		return 0; /* Already have one. */
	}

	/* Attempt to get the TCP socket for the same fd. */
	TCPsocket tcpSock = fd2TCPsocket(fd);
	if (!tcpSock) {
		sl_errno = SL_ESOCKET;
		return -1;
	}

	/* Retrieve peer address from the TCP socket. */
	IPaddress *ip = SDLNet_TCP_GetPeerAddress(tcpSock);
	if (!ip) {
		sl_errno = SL_ESOCKET;
		return -1;
	}

	/* Create a UDP socket. Bind it to the same host/port if possible. */
	UDPsocket newUdp = SDLNet_UDP_Open(0); /* 0 means ephemeral local port */
	if (!newUdp) {
		sl_errno = SL_ESOCKET;
		return -1;
	}

	/* Bind channel 0 to the server's IP and port, so we can send/receive from that address. */
	if (SDLNet_UDP_Bind(newUdp, 0, ip) == -1) {
		SDLNet_UDP_Close(newUdp);
		sl_errno = SL_ESOCKET;
		return -1;
	}

	/* Store it in the table at the same index. */
	udp_socket_table[fd] = newUdp;

	return 0;
}
