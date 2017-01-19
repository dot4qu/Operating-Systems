/*
Written by Brian Team (dot4qu)
Date: 11/29/16
This source file is responsible for implementing a simple FTP server
*/

#include "my_ftpd.h"

//GLOBALS
int client_cmd_fd;
int client_data_fd;
char type;
string cwd;
char cwd_buf[PATH_MAX];

string read_dir_files(string dir) {
	pid_t pids[2];					/* hold pidsof forked process */
	pid_t current_pid;				/* holds pid of just forked process */
	char output[16384];				/* big ass buffer in case listing a huge dir */
	int ls_pipe[2], awk_pipe[2];	/* to pipe output of ls back to awk and awk to parent*/
	int err;						/* temp var to hold retvals */	
	string cmd_str;					/* used to build up full command w/ ls, dir, and awk */
	char const ls_cmd[] = "/bin/ls\0";
	char** ls_args;
	char const awk_cmd[] = "/usr/bin/awk\0";
	char **awk_args;

	//creating in/out pipes
	err = pipe(ls_pipe); CHECK_ERROR(err);
	err = pipe(awk_pipe); CHECK_ERROR(err);

	//build ls args,  /bin/ls -l dir
	ls_args = (char **) malloc( sizeof(char*) * 4 );
	//malloc and copy /bin/ls as first arg
	ls_args[0] = (char *) malloc( sizeof (ls_cmd) );
	strncpy(ls_args[0], ls_cmd, sizeof(ls_cmd));
	//malloc 3 bytes and copy two chars plus null
	ls_args[1] = (char *) malloc( sizeof("-l") + 1);
	strncpy(ls_args[1], "-l\0", 3);
	//malloc length of dir and copy that many chars plus null
	ls_args[2] = (char *) malloc(dir.length() + 1);
	strncpy(ls_args[2], dir.c_str(), dir.length() + 1);
	ls_args[3] = NULL;

	awk_args = (char**) malloc(sizeof(char*) * 3);
	awk_args[0] = (char*) malloc(sizeof(awk_cmd));
	strncpy(awk_args[0], awk_cmd, sizeof(awk_cmd) + 1 );
	awk_args[1] = (char*) malloc( 256 );
			strncpy(awk_args[1], "NR > 1 {print $9, \"\\t\", $5}\0", 28 );	//hardcoded, sloppy but short on time
			awk_args[2] = NULL;

	for (int i = 0; i < 2; i++) {
		//forking child process
		current_pid = fork();
		CHECK_ERROR(pids[i]);

		if (current_pid == 0) {
			//were in the child
			if (i == 0) {
				//dont need to change stdin since were not using it / ls is the first cmd
				//change ls cmds stdout to middle pipe's write
				err = dup2(ls_pipe[1], STDOUT_FILENO); 
				CHECK_ERROR(err);
				err = close(ls_pipe[0]);// CHECK_ERROR(err);
				//exec ls cmd
				err = execv(ls_cmd, ls_args);
			} else {
				//change awk cmds stdin to middle pipe's read
				err = dup2(ls_pipe[0], STDIN_FILENO); 
				CHECK_ERROR(err);
				err = close(ls_pipe[1]); 
				//change awk cmds stdout to end pipe's stdin to return back to main process
				err = dup2(awk_pipe[1], STDOUT_FILENO); 
				CHECK_ERROR(err);
				err = close(awk_pipe[0]); //CHECK_ERROR(err);
				err = execv(awk_args[0], awk_args);
				CHECK_ERROR(err);
			}
		} else {
			//we're in the parent
			pids[i] = current_pid;
		}
	} //for int i < 2
	
	//close all pipes except what we want to read
	err = close(ls_pipe[0]);
	err = close(ls_pipe[1]);
	err = close(awk_pipe[1]);

	waitpid(pids[0], NULL, 0);
	waitpid(pids[1], NULL, 0);

	memset(output, 0, sizeof(output));
	int num_bytes = read(awk_pipe[0], output, sizeof(output));

	delete[] awk_args;
	delete[] ls_args;

	return string(output);
}

int open_data_socket(in_addr_t ip, int port) {
	int err;						/* temp err value for error checking */

	//creates socket for IPv4 communication domain with reliable two way byte stream, and no protocol is necessary to be specified
	int socketfd = socket(AF_INET, SOCK_STREAM, 0);
	CHECK_ERROR(socketfd);

	sockaddr_in addr_to_open;
	//sockaddr_in client_addr;

	//sockaddr_in struct fields and descriptions can be found here 
	//http://www.informit.com/articles/article.aspx?p=169505&seqNum=2
	memset(&addr_to_open, 0, sizeof(sockaddr_in));
	addr_to_open.sin_family = AF_INET;				//IPv4 family
	addr_to_open.sin_port = htons(port);					//translate port from host to network byte order
	addr_to_open.sin_addr.s_addr = ip;				//accept incoming conns specific IP

	//write out command okay for acknowledgement that we recieved PORT cmd
	err = write(client_cmd_fd, command_okay.c_str(), command_okay_size);
	CHECK_ERROR(err);

	//write out a 150 saying we're about to open a data conn
	err = write(client_cmd_fd, open_data_response.c_str(), open_data_response_size);
	CHECK_ERROR(err);

	//connect to new data socket
	err = connect(socketfd, (struct sockaddr *) &addr_to_open, sizeof(addr_to_open));
	CHECK_ERROR(err);

	return socketfd;
}

string parse_input(char* input_buf) {
	string cmd;						/* holds 3 or 4 letter command, retval */
	int index = 0;					/* holds char index to iterate through input string */
	while (input_buf[index] != ' ' && input_buf[index] != '\r' && input_buf[index] != '\n') {
		index++;
	}
	cmd = string(input_buf).substr(0, index);
	return cmd;
}

void parse_ip_and_port(string param, string *ip, int *port1, int *port2) {
	int commas = 0;				/* holds number of commas already iterated over */
	string port;				/* holds temp port with two comma delimited numbers before parsing */

	for (int i = 0; i < param.length(); i++) {
		//first increment commas if were on one
		if (param[i] == ',') {
			commas++;
		}
		if (commas == 4) {
			//if we've hit the 4th comma, we have complete IP
			*ip = param.substr(0, i++);
			port = param.substr(i);
			//replacing commas with periods
			replace(ip->begin(), ip->end(), ',', '.');
			break;
		}
	}
	int j;
	for (int i = 0; i < port.length(); i++) {
		if (port[i] == ',') {
			*port1 = atoi(port.substr(0, i++).c_str());
			j = i;
		} else if (port[i] == '\r' || port[i] == '\n') {
			*port2 = atoi(port.substr(j, i).c_str());
			break;
		}
	}
}


int handle_cmd(string cmd, char* input_buf) {
	string param;					/* set to value following cmd in the input string for manipulation */
	string ip_str;					/* used to break up param even mroe into seperate port and IP's for new socket */
	string port_str;				/* see ip */
	int data_port;					/* holds actual numberical value of finalized port */
	int port1, port2;				/* holds most significant / least significant port fields for temporary conversion */
	in_addr_t data_ip;				/* new ip to open data socket to */
	string temp_dir_string;			/* holds full output string but with LFs instead of CRLFs */
	char* dir;						/* holds full string of directory entries and their filenames formatted according to spec with CRLF's added */
	string local_filename;			/* holds filename of file to stor or retr on server */
	string remote_filename;			/* holds filename of file to stor or retr on client */
	int local_fd;					/* holds fd after 'open'ing a file for putting or sending */
	struct stat file_stats;			/* holds filesize of local file to either put or send */
	int err;						/* tempvalue to check return codes for errors */
	
	if (cmd == "TYPE") {
		//format: TYPE [param char]\r\n
		//substr to get single char param
		param = string(input_buf).substr(5, 1);
		//set type variable to given char. This will be checked to ensure I when stor or retr execd
		type = param[0];

		err = write(client_cmd_fd, command_okay.c_str(), command_okay_size);
		CHECK_ERROR(err);

	} else if (cmd == "PORT") {
		//format: PORT [xxx,xxx,xxx,xxx,xxx,xxx]\r\n. Each x field can be 1 to 3 chars, comma seperated
		//need parse IP and port out of remaining string
		param = string(input_buf).substr(5);
		//pass ip and port by ref to set their values
		parse_ip_and_port(param, &ip_str, &port1, &port2);
		//convert host notation to network order
		data_ip = inet_addr(ip_str.c_str());
		CHECK_ERROR(data_ip);
		//piecing together port and converting to network byte order
		port1 = port1 << 8;
		data_port = port1;
		data_port |= port2;
		//opens new data socket for transfer
		client_data_fd = open_data_socket(data_ip, data_port);
		CHECK_ERROR(client_data_fd);

	} else if (cmd == "USER") {
		//format: USER [username]. Not sure if this needs to be implemented but handle it anyway

	} else if (cmd == "QUIT") {
		//format: QUIT. close ftp session
		err = write(client_cmd_fd, quit_response.c_str(), quit_response_size);
		CHECK_ERROR(err);
		err = close(client_cmd_fd);
		CHECK_ERROR(err);
		//return val forces main loop to exit
		return -1;
	} else if (cmd == "MODE") {
		//format: MODE []

	} else if (cmd == "STRU") {
		//format STRU [param char]. Pull either F or R, deny if R
		//substr to get single char param
		param = string(input_buf).substr(5, 1);
		if (param == "R") {
			err = write(client_cmd_fd, stru_failed_response.c_str(), stru_failed_response_size);
			CHECK_ERROR(err);
		} else if (param == "F") {
			//were good, staying with default file structure
			err = write(client_cmd_fd, command_okay.c_str(), command_okay_size);
			CHECK_ERROR(err);
		}

	} else if (cmd == "RETR") {
		//first things first check type var
		if (type == 'A' || type == 'a') {
			close(client_data_fd);
			err = write(client_cmd_fd, stor_retr_failed_response.c_str(), stor_retr_failed_response_size);
			CHECK_ERROR(err);
			return 0;
		}
		//type is I, good to  move data
		//need to get filename from remaining buffer
		param = string(input_buf).substr(5);
		//pull crlf off of it
		int index;
		for (index = param.length() - 1; index > 0; index--) {
			if (param[index] == '\r')
				break;
		}
		param = param.substr(0, index);

		//open file on server
		local_fd = open(param.c_str(), O_CREAT);
		CHECK_ERROR(local_fd);
		//get filesize
		err = stat(param.c_str(), &file_stats);
		//temp buf for transfer of file
		char file_buf[file_stats.st_size];
		//read file off server into buf
		err = read(local_fd, file_buf, file_stats.st_size);
		CHECK_ERROR(err);
		//write file from buf to data socket
		err = write(client_data_fd, file_buf, file_stats.st_size);
		CHECK_ERROR(err);
		//226 data finished
		err = write(client_cmd_fd, data_finished_response.c_str(), data_finished_response_size);
		CHECK_ERROR(err);
		//close data socket
		err = close(client_data_fd);
		CHECK_ERROR(err);

	} else if (cmd == "STOR") {
		//first things first check type var
		if (type == 'A' || type == 'a') {
			close(client_data_fd);
			err = write(client_cmd_fd, stor_retr_failed_response.c_str(), stor_retr_failed_response_size);
			CHECK_ERROR(err);
			return 0;
		}
		//type is I, good to open socket and move data
		//need to get filename from remaining buffer
		param = string(input_buf).substr(5);
		//pull crlf off of it
		int index;
		for (index = param.length() - 1; index > 0; index--) {
			if (param[index] == '\r')
				break;
		}
		param = param.substr(0, index);

		//open file on server
		local_fd = creat(param.c_str(), 0777);
		CHECK_ERROR(local_fd);
		//8mb temp buf for transfer of file
		char* file_buf = (char *) malloc(sizeof(char) * 8388608);
		int filesize = 0;
		//read file off server into buf byte by byte until errors
		while (err > 0) {
			err = read(client_data_fd, &file_buf[filesize], 1);
			filesize++;
		}
		//compensate for last increment when it exited loop
		filesize--;
		//write file from buf to data socket
		err = write(local_fd, file_buf, filesize);
		CHECK_ERROR(err);
		//226 data finished
		err = write(client_cmd_fd, data_finished_response.c_str(), data_finished_response_size);
		CHECK_ERROR(err);
		//close data socket
		err = close(client_data_fd);
		CHECK_ERROR(err);

	} else if (cmd == "NOOP") {
		//format: NOOP\r\n. only requires okay back
		err = write(client_cmd_fd, command_okay.c_str(), command_okay_size);
		CHECK_ERROR(err);

	} else if (cmd == "LIST") {
		//format: LIST [dir]\r\n
		if (input_buf[4] != '\r') {
			param = string(input_buf).substr(5);
			int i = 0;
			for (; i < param.length(); i++) {
				if (param[i] == '\r' || param[i] == '\n') {
					break;
				}
			}
			param = param.substr(0, i);
		} else if (input_buf[5] == '.') {
			if (input_buf[6] != '.') {
				//parent dir
			} else {
				//current dir
				param = cwd;
			}
		} else {
			//else keep cwd the same as it is since no dir supplied
			param = cwd;
		}

		//build string of given directory files and filesizes
		temp_dir_string = read_dir_files(param);

		//get number of newlines so we know how big to malloc dir buf
		int newlines = count(temp_dir_string.begin(), temp_dir_string.end(), '\n');
		dir = (char *) malloc(temp_dir_string.length() + 2 * newlines);
		for (int i = 0, dir_index = 0; i < temp_dir_string.length(); i++, dir_index++) {
			if (temp_dir_string[i] == '\n') {
				dir[dir_index++] = '\r';
			} 
			dir[dir_index] = temp_dir_string[i];
		}

		err = write(client_data_fd, dir, temp_dir_string.length() + newlines);

		err = write(client_cmd_fd, close_ascii_response.c_str(), close_ascii_response_size);
		CHECK_ERROR(err);

		err = close(client_data_fd);
		CHECK_ERROR(err);

		delete dir;
		
	} else {
		//unknown/unsupported cmd
	}
	return 0;
}



int main(int argc, char **argv) {
	int port = 0;								/* holds the port read in as cmdline param to open server on */
	int socketfd = 0;							/* holds socket filedescriptor once initialized */
	client_cmd_fd = 0;							/* holds fd for incoming client socket connection */
	sockaddr_in server_addr;					/* struct to hold socket information for binding */
	sockaddr_in client_addr;					/* struct to hold socket info for connected client */
	char input_buf[256];						/* buffer to hold command input from client on control line */
	int recvd_bytes;							/* holds number of bytes recieved when reading from client */
	string cmd;									/* holds command pulled from user input string */
	type = 'a';									/* holds current transfer type. A for ascii, I for image/binary */
	int err;									/* used for holding temp return values and checking for erros */
	
	//ensuring only one cmdline param and grabbing port no.
	if (argc != 2) {
		printf("Not enough args.\n"); 
		return -1;
	}
	port = atoi(argv[1]);

	char* temp = getcwd(cwd_buf, PATH_MAX);
	if (temp != NULL) {
		cwd = string(cwd_buf);
	}

	//creates socket for IPv4 communication domain with reliable two way byte stream, and no protocol is necessary to be specified
	socketfd = socket(AF_INET, SOCK_STREAM, 0);
	CHECK_ERROR(socketfd);

	//sockaddr_in struct fields and descriptions can be found here 
	//http://www.informit.com/articles/article.aspx?p=169505&seqNum=2
	memset(&server_addr, 0, sizeof(sockaddr_in));
	server_addr.sin_family = AF_INET;			//IPv4 family
	server_addr.sin_port = htons(port);			//translate port from host to network byte order
	server_addr.sin_addr.s_addr = INADDR_ANY;	//accept incoming conns from all IPs

	//binding new socket to port entered when program initally run
	err = bind(socketfd, (struct sockaddr *) &server_addr, sizeof(server_addr));
	CHECK_ERROR(err);

	while(1) {
		//listen on port to open for connections
		err = listen(socketfd, BACKLOG_MAX);
		CHECK_ERROR(err);

		socklen_t client_addr_size = sizeof(client_addr);
		//accept any incoming connections and save client info into client_addr struct
		client_cmd_fd = accept(socketfd, (struct sockaddr *) &client_addr, &client_addr_size);
		CHECK_ERROR(client_cmd_fd);

		//write success string for recieved password
		err = write(client_cmd_fd, password_response.c_str(), password_response_size);
		CHECK_ERROR(err);

		while(1) {
			//pull in next input string
			memset(input_buf, 0, 256);
			recvd_bytes = read(client_cmd_fd, input_buf, sizeof(input_buf));
			CHECK_ERROR(recvd_bytes);

			//pull out cmd from input string
			cmd = parse_input(input_buf);

			err = handle_cmd(cmd, input_buf);
			//means we recieved a QUIT, socket is closed in handle_cmd func so we just need to exit process
			if (err < 0) {
				close(client_cmd_fd);
				break;
			}
		}
	}
	return 0;
}