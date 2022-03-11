CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2
	
endif

server: my_mian.cpp ./http_conn/http_conn.cpp 
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread  -lpthread

clean:
	rm  -r server