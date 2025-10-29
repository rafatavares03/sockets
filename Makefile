build:
	gcc servidor/main.c -o ./server
	gcc cliente/main.c -o ./client 

clean:
	rm ./client
	rm ./server