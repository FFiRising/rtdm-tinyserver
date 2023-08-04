TAR =  ../bin/server
OBJ = main.o   webserverhttp.o  http_conn.o  config.o  timer.o apptcpsocket.o  sharedbuffer.o  sql_connection_pool.o  mutex.o   futil.o log.o
CC := g++

$(TAR):$(OBJ)
	$(CC) -W -Wall -o $(TAR) $(OBJ) -lpthread  -L /usr/local/lib /usr/local/lib/libjsoncpp.a  -lmysqlclient -lyaml-cpp

http_conn.o: ../src/http_connection/http_conn.cpp
	$(CC) -c  ../src/http_connection/http_conn.cpp -o http_conn.o -g	 

timer.o: 	../src/timer/timer.cpp
	$(CC) -c  ../src/timer/timer.cpp -o timer.o -g

webserverhttp.o: ../src/webserverhttp/webserverhttp.cpp  
	$(CC) -c  ../src/webserverhttp/webserverhttp.cpp  -o webserverhttp.o   -g

config.o: 	../src/config/config.cpp
	$(CC) -c  ../src/config/config.cpp -o config.o -g

apptcpsocket.o:  ../src/apptcpsocket/apptcpsocket.cpp
	$(CC) -c  ../src/apptcpsocket/apptcpsocket.cpp -o  apptcpsocket.o -g

sharedbuffer.o:  ../src/sharedbuffer/sharedbuffer.cpp
	$(CC) -c  ../src/sharedbuffer/sharedbuffer.cpp -o  sharedbuffer.o -g

sql_connection_pool.o:  ../src/CGImysql/sql_connection_pool.cpp
	$(CC) -c  	../src/CGImysql/sql_connection_pool.cpp -o  sql_connection_pool.o -g


mutex.o: ../src/mutex/mutex.cc
	$(CC) -c  ../src/mutex/mutex.cc  -o  mutex.o -g	 

log.o: ../src/log/log.cc
	$(CC) -c  ../src/log/log.cc  -o  log.o -g	
	
futil.o: ../src/fileutil/futil.cc
	$(CC) -c  ../src/fileutil/futil.cc  -o  futil.o -g

main.o: ../src/main.cpp
	$(CC) -c  ../src/main.cpp -o  main.o -g

.PHONY:
clean:
	rm $(OBJ)