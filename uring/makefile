all:
	#gcc io_uring_server.c -Wall -Ofast -s -march=native -luring -oio_uring_server
	gcc srv.c -Wall -Ofast -s -march=native -osrv
	gcc cli.c -Wall -Ofast -s -march=native -l:liburing.a -ocli
	docker build -tio .
