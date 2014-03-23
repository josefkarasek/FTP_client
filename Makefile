CC=g++
CFLAGS=-std=c++98

ftpclient: ftpclient.cpp
	$(CC) $(CFLAGS) ftpclient.cpp -o ftpclient

