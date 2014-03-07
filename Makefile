CC=g++
CFLAGS=-std=c++0x -g

ftpclient: ftpclient.cpp
	$(CC) $(CFLAGS) ftpclient.cpp -o ftpclient
