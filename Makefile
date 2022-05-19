CC = g++
CFLAG = -g -ggdb

OBJ = Server.o nodeserver.o net.o proxyserver.o base64.o cmd.o

.SUFFIXES: .cpp                                                                                
.cpp.o:                                                                                        
	$(CC) $(CFLAG) -c $< -o $@                                                      

Server :  $(OBJ)                             
	$(CC) $(CFLAG) $(OBJ) -o $@

clean:                                                                                         
	-rm *.o