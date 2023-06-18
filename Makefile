TAR =  server
OBJ = main.o   webserverhttp.o  http_conn.o  config.o  timer.o apptcpsocket.o  sharedbuffer.o  sql_connection_pool.o
CC := g++

$(TAR):$(OBJ)
	$(CC) -W -Wall -o $(TAR) $(OBJ) -lpthread  -L /usr/local/lib /usr/local/lib/libjsoncpp.a  -lmysqlclient

http_conn.o: ./http_connection/http_conn.cpp
	$(CC) -c  ./http_connection/http_conn.cpp -o http_conn.o -g	 

timer.o: 	./timer/timer.cpp
	$(CC) -c  ./timer/timer.cpp -o timer.o -g

webserverhttp.o: ./webserverhttp/webserverhttp.cpp  
	$(CC) -c  ./webserverhttp/webserverhttp.cpp  -o webserverhttp.o   -g

config.o: 	./config/config.cpp
	$(CC) -c  ./config/config.cpp -o config.o -g

apptcpsocket.o:  ./apptcpsocket/apptcpsocket.cpp
	$(CC) -c  ./apptcpsocket/apptcpsocket.cpp -o  apptcpsocket.o -g

sharedbuffer.o:  ./sharedbuffer/sharedbuffer.cpp
	$(CC) -c  ./sharedbuffer/sharedbuffer.cpp -o  sharedbuffer.o -g

sql_connection_pool.o:  ./CGImysql/sql_connection_pool.cpp
	$(CC) -c  	./CGImysql/sql_connection_pool.cpp -o  sql_connection_pool.o -g

main.o: main.cpp
	$(CC) -c  main.cpp -o main.o -g

.PHONY:
clean:
	rm $(OBJ)