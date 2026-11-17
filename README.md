# MiniHTTPd

Servidor HTTP/1.1 basico en C sin bibliotecas HTTP externas.
Sirve los archivos de la carpeta www/ usando un solo hilo con epoll y conexiones persistentes con keep-alive.

## Compilar

```bash
make		#Para generar ejecutable minihttpd
make clean	#Para borrar archivos .o y el ejecutable
```
## Ejecutar

Dentro de la carpeta minihttpd/

```bash
./minihttpd puerto
```

Si no se coloca el puerto, usa el 8080.
## Probar

Pagina en el navegador: http://localhost:8088/

Con curl se agrega Connection porque el curl no lo envia y el servidor exige Host, Connection y User-Agent en HTTP/1.1:
```bash
curl -i -H "Connection: keep-alive" http://localhost:8088/style.css
curl -i -H "Connection: keep-alive" http://localhost:8088/index.html
curl -i -H "Connection: keep-alive" http://localhost:8088/image.png
```

