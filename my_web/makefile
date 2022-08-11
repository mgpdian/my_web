CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
	
endif

server: main.cpp my_web.cpp ./http_conn/http_conn.cpp ./strdecode/strdecode.cpp ./timer/lis_timer.cpp ./log/log.cpp ./log/new_log.cpp ./log/util.cpp ./sql_conn/sql_conn.cpp ./timer/min_heap.cpp
	$(CXX) -g -o server  $^ $(CXXFLAGS) -lpthread -L/usr/lib64/mysql -lmysqlclient -lpthread

clean:
	rm  -r server