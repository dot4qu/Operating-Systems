/*
Written by Brian Team (dot4qu)
Date: 11/29/16
This header file is responsible for listing the macros, structs, and functions of its source file
*/

#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <algorithm>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

using namespace std;

#ifndef MY_FTPD_H
#define MY_FTPD_H

#define BACKLOG_MAX 50
#define CHECK_ERROR(err) { \
								if (err < 0) { \
									printf("Error on line %d in function %s!", __LINE__, __func__); \
									perror("!: "); \
									exit(-1); \
								} \
							}



const string command_okay = "200 Command okay.\r\n";
const int command_okay_size = command_okay.length();
const string password_response = "230 User logged in.\r\n";
const int password_response_size = password_response.length();
const string stru_failed_response = "504 Command not implemented for that parameter\r\n";
const int stru_failed_response_size = stru_failed_response.length();
const string stor_retr_failed_response = "451 Requested action aborted: local error in processing\r\n";
const int stor_retr_failed_response_size = stor_retr_failed_response.length();
const string open_data_response = "150 File status okay; about to open data connection.\r\n";
const int open_data_response_size = open_data_response.length();
const string already_open_ascii_response = "125 Data Connection already open; transfer starting.\r\n";
const int already_open_ascii_response_size = already_open_ascii_response.length();
const string close_ascii_response = "226 Listing complete, closing connection.\r\n";
const int close_ascii_response_size = close_ascii_response.length();
const string quit_response = "221 Goodbye.\r\n";
const int quit_response_size = quit_response.length();
const string data_finished_response = "226 Transfer complete, closing data connection.\r\n";
const int data_finished_response_size = data_finished_response.length();

string read_dir_files(string dir);					/* takes in directory string, executes system call for ls on directory and parses return data to correct fmt */
int open_data_socket(in_addr_t ip, int port); 		/* generic function to open a socket given IP and port */
string parse_input(char* input); 					/* takes in client command strings and parses, returning just command */
int handle_cmd(string cmd, char* input);			/* takesin cmd string and remaining input buf to perform necessary actions */
void parse_ip_and_port(string param, string *ip, int *port1, int *port2);	/* takes in param string and pulls part to save as IP and port seperately */

#endif