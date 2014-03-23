//============================================================================
// Name        : ftpclient.cpp
// Author      : xkaras27
// Version     :
// Copyright   : none
// Description : programming assignment 1: list directory content
//============================================================================

#include <iostream>
#include <cstdio>
#include <cerrno>        //error codes
#include <cstring>       //memset()
#include <fstream>

#include <regex.h>
#include <unistd.h>      //sleep()
#include <stdlib.h>
#include  <stdio.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>  //socket(), protocol families(AF_INET = IPv4)
#include <sys/types.h>   //SOCK_STREAM
#include <netdb.h>       //DNS resolver
#define BUFFER_LEN 2048
char msg[BUFFER_LEN];    //buffer for incoming messages

using namespace std;

string sendCommand(int socket, string command, bool wait);
string get_port(string& answer);
string form_string(char* input, int begin, int end);
bool check_code(string& toBeChecked);
int get_code(string& answer);
bool parse(char* argv);
bool change_directory(int socket);

typedef struct credentials {
	string username;
	string password;
	string hostname;
	string path;
	string port;
} Tcredentials;

Tcredentials destination;
FILE* fd_control;


int main(int argc, char** argv) {
	if(argc != 2) {
		cerr << "Bad parameters" << endl;
		return 2;
	}

	if(parse(argv[1]))
		return 2;

	string server_port, server_hostname, server_password, server_username, server_path;
	if(destination.username == "") {
		server_username = "USER anonymous\r\n";
		server_password = "PASS dummy\r\n";
	} else {
		server_username = "USER " + destination.username + "\r\n";
		server_password = "PASS " + destination.password + "\r\n";
	}
	if(destination.port == "")
		server_port = "21";
	else
		server_port = destination.port;
	if(destination.hostname == "") {
		cerr << "Error: Couldn't resolve hostname" << endl;
		return 2;
	} else
		server_hostname = destination.hostname;


	int client_socket, data_socket;                   //sockets
	struct sockaddr_in server_address, data_address;  //IP address info
	struct hostent *server_ip;                        //DNS resolver
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;

	//setting server address for control connection (need to contact DNS)
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons ( atoi(server_port.c_str()) );
	//contact DNS
	server_ip = gethostbyname(server_hostname.c_str());
	if(server_ip == NULL) {
		cerr << "Error: DNS returned NULL" << endl;
		return 2;
	}
	memcpy(&server_address.sin_addr, server_ip->h_addr_list[0], server_ip->h_length);

	//creating socket
	client_socket = socket(PF_INET, SOCK_STREAM, 0);
	if(client_socket < 0) {
		cerr << "socket: " << strerror(errno) << endl;
		return 2;
	}
    if (setsockopt (client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
        cerr << "setsockopt: " << strerror(errno) << endl;
        close(client_socket);
        return 2;
    }

	//establishing control connection
	if((connect(client_socket, (struct sockaddr *)&server_address, sizeof(server_address))) < 0) {
		cerr << "connect: " << strerror(errno) << endl;
		close(client_socket);
		return 2;
	}
	//communicating with the server
	string answer;
	answer = sendCommand(client_socket, server_username, 1);
	if(check_code(answer)) {
		cerr << "Error: Couldn't send command" << endl;
		close(client_socket);
		fclose(fd_control);
		return 2;
	}

	answer = sendCommand(client_socket, server_password, 1);
	if(check_code(answer)) {
		cerr << "Error: Couldn't send command" << endl;
		close(client_socket);
		fclose(fd_control);
		return 2;
	}

	answer = sendCommand(client_socket, "EPSV\r\n", 1);
	if(check_code(answer)) {
		cerr << "Error: Couldn't send command" << endl;
		close(client_socket);
		fclose(fd_control);
		return 2;
	}
	string port = get_port(answer);


	//setting server address for data connection
	memset(&data_address, 0, sizeof(data_address));
	data_address.sin_family = AF_INET;
	data_address.sin_port = htons ( atoi(port.c_str()) );
	memcpy(&data_address.sin_addr, server_ip->h_addr_list[0], server_ip->h_length);
	data_socket = socket(PF_INET, SOCK_STREAM, 0);


	//establishing data connection
	if((connect(data_socket, (struct sockaddr *)&data_address, sizeof(data_address))) < 0) {
		cerr << "connect: " << strerror(errno) << endl;
		close(client_socket);
		close(data_socket);
		fclose(fd_control);
		return 2;
	}

	if(destination.path != "")
		if(change_directory(client_socket)) {
			cerr << "Error: Couldn't change working directory" << endl;
			close(client_socket);
			close(data_socket);
			fclose(fd_control);
			return 2;
		}

	answer = sendCommand(client_socket, "LIST\r\n", 1);
	if(check_code(answer)) {
		cerr << "Error: Couldn't send LIST coomand" << endl;
		close(client_socket);
		close(data_socket);
		fclose(fd_control);
		return 2;
	}

    FILE* fd_data = fdopen(data_socket, "r");
    while(fgets(msg, BUFFER_LEN, fd_data) != NULL) {
        cout << msg;
    }
    fclose(fd_data);

    //ain't waintin' for no goodbye
	answer = sendCommand(client_socket, "QUIT\r\n", 0);

	close(client_socket);
	close(data_socket);
	fclose(fd_control);
	return 0;
}

/*
 * Try to change directory, if fails, true is given
 */
bool change_directory(int socket) {
	string answer = sendCommand(socket, "CWD " + destination.path + "\r\n", 1);
	if(check_code(answer))
		return true;

	return false;
}


/*
 * Get substring from imput string
 */
string form_string(char* input, int begin, int end) {
	string output = "";
	if(begin == -1 || end == -1)
		return output;

	for(int i=begin; i<end; ++i)
		output += input[i];

	return output;
}

/*
 * Check whether hostname & credentials are OK
 * Uses global struct credentials: destination
 */
bool parse(char* argv) {
	regex_t regex;
	size_t nmatch = 11;
	int reti;
	regmatch_t pmatch[11];

	//<Mega readable code!!!>
	if((reti = regcomp(&regex, "^(ftp:\\/\\/)?(([a-zA-Z0-9]+):([a-zA-Z0-9]+)@)?(ftp\\."\
			 "([a-zA-Z0-9_]\\.?)+)(:([0-9]{1,3}))?(/?([a-zA-Z0-9_$-\\.\\+\\!\\*'\\(\\)]+/?))?$", REG_EXTENDED)\
			) != 0) {
		cerr << "Error: regex compile" << endl;
		return true;
	}
	//</Mega readable code!!!>


	reti = regexec(&regex, argv, nmatch, pmatch, 0);

	if(reti == 0 && reti != REG_NOMATCH) {

		destination.username = form_string(argv, pmatch[3].rm_so, pmatch[3].rm_eo); //username
		destination.password = form_string(argv, pmatch[4].rm_so, pmatch[4].rm_eo); //password
		destination.hostname = form_string(argv, pmatch[5].rm_so, pmatch[5].rm_eo); //hostname
		destination.port = form_string(argv, pmatch[8].rm_so, pmatch[8].rm_eo); //port
		destination.path = form_string(argv, pmatch[9].rm_so, pmatch[9].rm_eo); //path

		return false;
	} else {
		cerr << "Error: regex exec" << endl;
		return true;
	}
	return true;
}


/*
 * Recognize an error, if occurs
 */
bool check_code(string& toBeChecked) {
	int code = get_code(toBeChecked);
	bool error = false;

	switch(code) {
	case 150: break;
	case 220: break;
	case 221: break;
	case 226: break;
	case 229: break;
	case 230: break;
	case 331: break;
	case 425: cerr << "Error: Can't open data connection." << endl;      error = true; break;
	case 430: cerr << "Error: Invalid username or password." << endl;    error = true; break;
	case 530: cerr << "Error: Please login with USER and PASS." << endl; error = true; break;
	case 503: cerr << "Error: Bad sequence of commands." << endl;        error = true; break;
	default:
		if(code/100 == 5) {
			cerr << "General failure." << endl;
			error = true;
		}
		break; //general failure
	}
	return error;
}


/*
 * Parse response code from server response
 */
int get_code(string& answer) {
	char code[4];
	for(int i=0; i<3; ++i)
		code[i] = answer[i];
	code[3] = '\0';

	return atoi(code);
}


/*
 * Parse negotiated port from server response
 */
string get_port(string& answer) {
	string port;
	int d = 0;  //number of delimiters '|'

	for(int i=0; i< answer.length(); ++i) {
		if(answer[i] == '|')
			d++;

		if(isdigit(answer[i]) && d == 3)
			port += answer[i];
	}
	return port;
}


/*
 * Send command to a socket
 */
string sendCommand(int socket, string command, bool wait) {
	int in, out;
	fd_control = fdopen(socket, "r");
	out = send(socket, command.c_str(), command.size(), 0);
	if(wait == 0)
		return "";

	usleep(100000);
	while( fgets(msg, BUFFER_LEN, fd_control) != NULL	) {
		// cout << msg;
		if(isdigit(msg[0]) && isdigit(msg[1]) && isdigit(msg[2]) && msg[3] == ' ')
			break;
	}
	//cast buffer to C++ string
	string output = msg;
	return output;
}
    
