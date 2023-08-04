TAR =  server
OBJ = main.o  mutex.o  log.o config.o futil.o
CC := g++

$(TAR):$(OBJ)
	$(CC) -W -Wall -o $(TAR) $(OBJ) -lpthread  -L /usr/local/lib /usr/local/lib/libjsoncpp.a  -lmysqlclient -lyaml-cpp

mutex.o: ../src/mutex/mutex.cc
	$(CC) -c  ../src/mutex/mutex.cc  -o  mutex.o -g	 

log.o: ../src/log/log.cc
	$(CC) -c  ../src/log/log.cc  -o  log.o -g	

config.o: ../src/config/config.cpp
	$(CC) -c  ../src/config/config.cpp  -o  config.o -g
	
futil.o: ../src/fileutil/futil.cc
	$(CC) -c  ../src/fileutil/futil.cc  -o  futil.o -g

main.o: testmain.cc
	$(CC) -c  testmain.cc -o main.o -g

.PHONY:
clean:
	rm $(OBJ)