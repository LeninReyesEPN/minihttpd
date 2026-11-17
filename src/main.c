/*
 *
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "server.h"

#define QLEN	5
#define BUFSZ	1024
#define PUERTO	"8080"
#define PAGINA	"<h1>MiniHTTPd okey</h1>"

int main(int argc, char *argv[]) {
	
	const char *puerto = PUERTO;
	int sd, nsd, largo;
	ssize_t n;
	char peticion[BUFSZ];
	char encabezados[256];
	
	if (argc > 1)
		puerto = argv[1];
	
	sd = passiveTCP(puerto, QLEN);
	if (sd < 0 )
		return 1;

	printf("MiniHTTPd escuchando en el puerto %s\n", puerto);
	fflush(stdout);

	while (1) {

		nsd = accept(sd, NULL, NULL);
		if (nsd < 0)
			continue;

		n = read(nsd, peticion, sizeof(peticion));
		if (n <= 0) {
			close(nsd);
			continue;
		}
		
		largo = snprintf(encabezados, sizeof(encabezados),
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: %zu\r\n"
			"Connection: close\r\n"
			"\r\n",

			strlen(PAGINA));
		if (largo <0 || largo >= (int) sizeof(encabezados)) {
			close(nsd);
			continue;
		}
		
		write(nsd, encabezados, largo);
		write(nsd, PAGINA, strlen(PAGINA));

		close(nsd);
	
	}


}
