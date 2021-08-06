all: httpd client add.cgi
LIBS = -lpthread #-lsocket
httpd: httpd.c
	arm-linux-gnueabihf-gcc $< -g -W -Wall $(LIBS) -o $@ 

client: simpleclient.c
	arm-linux-gnueabihf-gcc $< -W -Wall -o $@ 
add.cgi: ./htdocs/add.c
	arm-linux-gnueabihf-gcc $< -o ./htdocs/add.cgi 
clean:
	rm httpd
