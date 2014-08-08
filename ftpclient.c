/*
* a2client.c
*
* PURPOSE: An ftp client.
* INSTRUCTIONS:
*
*		There will be 3 arguments in command line
*		a2client <ip_address> <port_number>
*
*		choose ip_address to be 127.0.0.1
*		choose port number to be 10000
*
*/


#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h> 
#include <netinet/in.h>
#include <fcntl.h>

#define MAX_LENGTH 50
#define SEPCHARS " \t\r\n"

enum BOOLEAN{
	FALSE = 0,
	TRUE = 1
};

typedef enum BOOLEAN boolean;




/* PURPOSE: This is to get the filename that is specified in a command.	
 */
void get_filename(char command[], char filename[]){

	char *word;
	
	//parse
	word = strtok(command, SEPCHARS);
	if(word == NULL){
		perror("\nError in PUT command");
		exit(1);
	}
	//get the filename
	word = strtok(NULL, SEPCHARS);
	if(word == NULL){
		strncpy(filename, "", MAX_LENGTH);
	}
	else{
		strncpy(filename, word, MAX_LENGTH);
	}


}

/* PURPOSE: This is for checking if a file really is in the client directory.
 *          This is done by actually trying to open the file. If we can't open
 *          the file then we know it doesn't exist.
 */
boolean check_directory(char command[]){

	int file;
    	char filename[MAX_LENGTH];
    	boolean has_write = TRUE;
    
    	get_filename(command, filename);

	//read the file
	file = open(filename, O_RDONLY);
	if(file == -1){

		perror("\nError in open file");
		close(file);
		has_write = FALSE;

	}
	
	return has_write;

}

/* PURPOSE: Process the LIST command entered by client. Server will send us a list
 *		  of it's current directory.	
 */
void process_list(int sock_data){

	//read the data from the server
	char list[MAX_LENGTH];
	int n;
	while((n = read(sock_data, list, MAX_LENGTH-1))){

		list[n] = '\0';
		fprintf(stderr, "%s", list);
	}
}


/* PURPOSE: Process the GET command entered by client. This will make a new
 *          file and put it in our directory.	
 */
void process_get(char command[], int sock_data){

	int file;
    	int bytes_received = 0;
     char temp[MAX_LENGTH];
     char filename[MAX_LENGTH];
     int n;
     
     get_filename(command, filename);
     //create new file
     file = open(filename,O_WRONLY|O_CREAT,S_IRWXU);
     if(file == -1){
	
		perror("\nError in open file");
		exit(1);
	}
    	
    	//read data in from server and write these into new file
    	n = read(sock_data, temp, MAX_LENGTH);
    	while(n > 0){
    		bytes_received += n;
    		write(file, temp, n);
    		n = read(sock_data, temp, MAX_LENGTH);
    		
    	}
	if(n == 0){
		close(file);
		close(sock_data);
		
		fprintf(stderr, "\nBytes received: %d\n", bytes_received);
	}
	else if(n < 0){
		fprintf(stderr, "\nError in read()");
		exit(1);
	}
    		
}

/* PURPOSE: Process the PUT command entered by client
 *	
 */
void process_put(char command[], int sock_data){

	int file;
	char filename[MAX_LENGTH];
	char temp[MAX_LENGTH];
	
	get_filename(command, filename);
	
	//read the file
	file = open(filename, O_RDONLY);
	if(file == -1){
	
		perror("\nError in open file");
		exit(1);
	}

	
	//read the file line by line, send each line to the server
	int byte = read(file, temp, MAX_LENGTH);
	while(byte > 0){

		//fprintf(stderr, "%s", temp);	//if we want to see what we send
		write(sock_data, temp, byte);
		byte = read(file, temp, MAX_LENGTH);
	} 

	//the client will close the connection once it has transferred all the data
	//(sent all the lines)
	if(byte == 0){
		close(file);
		close(sock_data);
		fprintf(stderr, "\nDone transferring file. Closed data connection.\n\n");

	}   		
	else if(byte < 0){
		perror("\nError in read file");
		exit(1);
	}
	
}




/* PURPOSE: Creates a data connection whenever a LIST, GET or PUT command
 *		  is entered by client.	
 */
void create_data_connect(char command[], char ip[], char portnum[]){

	int sock_data;
	struct sockaddr_in inf;
	
	//create a tcp socket
	if ((sock_data = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    
		perror("\nError in socket()");
		exit(1);
     }
     
    //construct the struct
    memset(&inf, 0, sizeof(inf));       		/* Clear struct */
    inf.sin_family = AF_INET;                  	/* Internet/IP */
    inf.sin_addr.s_addr = inet_addr(ip);  		/* IP address */
    //here we use the port number from the control connection + 1
    inf.sin_port = htons(atoi(portnum) + 1);      /* server port */

    //connect to the server
    if (connect(sock_data, (struct sockaddr *) &inf, sizeof(inf)) < 0) {
		perror("\nError in connect()");
		exit(1);
    }
    
    //if we issued a PUT command
    if(strncmp("PUT", command, 3) == 0){

		process_put(command, sock_data);

    }

    else if(strncmp("GET", command, 3) == 0){

    		process_get(command, sock_data);
    }

    //we issued a LIST command
    else if(strncmp("LIST", command, 4) == 0){
		process_list(sock_data);    

    }
	
    else{
		fprintf(stderr, "\nNot a valid command.\n\n");
    }

}

int main(int argc, char *argv[]) {

	int sock_control;
	boolean has_quit = 0;
	struct sockaddr_in address;
	
	if (argc != 3) {
	
     	fprintf(stderr, "USAGE: a2client <ip_address> <port_number>\n");
    		exit(1);
    }


    //create a tcp socket
    if ((sock_control = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    
		perror("\nError in socket()");
		exit(1);
    }
    
    //construct the struct
    memset(&address, 0, sizeof(address));       		/* Clear struct */
    address.sin_family = AF_INET;                  	/* Internet/IP */
    address.sin_addr.s_addr = inet_addr(argv[1]);  	/* IP address */
    address.sin_port = htons(atoi(argv[2]));       	/* server port */
    
    //connect to the server
    if (connect(sock_control, (struct sockaddr *) &address, sizeof(address)) < 0) {
		perror("\nError in connect()");
		exit(1);
    }


    //keep getting input from client until they enter QUIT command
    //these input from client will be sent as commands to server
    while(has_quit == FALSE){
    
	    char command[MAX_LENGTH];
	    char command_temp[MAX_LENGTH];
	    fgets(command, MAX_LENGTH, stdin);	//get input from user 
	    strcat(command, "\r\n");
	    strcpy(command_temp, command);
	    
	    boolean has_write = TRUE;
	    
	    //if doing a PUT command, check if the file in your directory does in fact exist
	    //if the file does not exists on our (client) directory, do not bother sending 
	    //this PUT command to the server
	    //(since on a PUT, the server actually makes an empty file in its directory assuming that
	    //it is going to get data to put in it)
	    if(strncmp("PUT", command, 3) == 0){
	    		has_write = check_directory(command_temp);
	    }

		
	    if(has_write == TRUE){
	    
		    //send command to server
		    if(write(sock_control, command, strlen(command)) < 0){
		    		perror("\nError in write()");
		    		exit(1);
		    }
		    
		    char response[MAX_LENGTH];
		    int n;
		    //read response from server
		    if((n = read(sock_control, response, MAX_LENGTH-1))){

		          response[n] = '\0';
		    		fprintf(stderr, "%s", response);
		    }
		    
		    //if user enters GET, LIST or PUT
		    //will not make a data connection if a response is anything other
		    //than 103 Command okay	
		    if(strncmp("103", response, 3) == 0){
		    		
		    		
		    	     //if the command is ok, the client will connect to the server to form
		    		//a tcp data connection

		    		create_data_connect(command, argv[1], argv[2]);
		    
		    
		    }
		    if(strncmp("QUIT", command, 4) == 0){
				has_quit = TRUE;
		    }
		    
	    
	    }

     }










	//when client enters QUIT we quit here
	printf("\n\nProcessing complete\n\n");
  	exit(0);
  	
	
  	
}
