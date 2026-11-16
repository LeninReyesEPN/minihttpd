#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "server.h"

/*------------------------------------------------------------------------
 * passiveTCP - crea un socket TCP que escucha en el puerto indicado
 * 
 * puerto - numero de puerto
 * qlen - cuantos clientes pueden esperar en la fila de accept
 *
 *------------------------------------------------------------------------
 */
int
passiveTCP(const char *puerto, int qlen)
{
	struct addrinfo hints, *res, *rp;
	int	s = -1, err, last_errno = 0, one = 1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;		/* IPv4 o IPv6 */
	hints.ai_flags = AI_PASSIVE;		/* IP "cualquiera" (reemplaza INADDR_ANY) */

	hints.ai_socktype = SOCK_STREAM;

	/* host NULL + AI_PASSIVE + servicio -> lista de direcciones donde escuchar */
	if ((err = getaddrinfo(NULL, puerto, &hints, &res)) != 0){
		fprintf(stderr, "getaddrinfo(%s): %s\n", puerto, gai_strerror(err));
		return -1;
	}

	/* usar la primera direccion en la que se pueda hacer bind */
	for (rp = res; rp != NULL; rp = rp->ai_next) {
		s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (s < 0) {
			last_errno = errno;
			continue;
		}
		(void) setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
		if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0)
			break;
		last_errno = errno;
		close(s);
		s = -1;
	}
	freeaddrinfo(res);

	if (s < 0){
		fprintf(stderr, "no se puede usar el puerto %s: %s\n", puerto,
			strerror(last_errno));
		return -1;
	}

	if (listen(s, qlen) < 0) {
		perror("listen");
		close(s);
		return -1;
	}
	return s;
}
