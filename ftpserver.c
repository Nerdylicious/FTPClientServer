/*Oct 13, 2011*/
/*When processing a GET command, it ensures that the filename is that of a regular file.
*/

/*Oct 3, 2011*/
/*Sample server for A2*/
/*meant for testing your client only.  Some error checking is done*/
/*The server also quits (exits) on various errors. Obviously, this will not do for a real server, but show
suffice for testing your client
*/


#include<stdlib.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<errno.h>
#include<dirent.h>

#define SERV_BACKLOG 5
#define MAX_SIZE 256
#define SEPCHARS " \t\r\n"

enum states {STATE_NOT_CONNECTED, STATE_CONNECTED, STATE_WAITING_FOR_PASS, STATE_LOGGED_IN};

int listfd; /*for accepting data connections*/


void write_message(int controlfd, char *strMsg)
{
    if ( write(controlfd,strMsg,strlen(strMsg)) != strlen(strMsg))
    {
        fprintf(stderr,"Error writing to client. Server exiting.\n");
        exit(EXIT_FAILURE);
    }
}

void process_user(char *username, enum states *ptrState, int controlfd)
{
    char tempStr[MAX_SIZE];

    if ((strlen(username) == strlen("comp3720")) && (strncmp("comp3720",username,strlen(username)) ==0))
    {
        strcpy(tempStr,"101 User ok, please provide password.\r\n");
        write_message(controlfd,tempStr);
        *ptrState = STATE_WAITING_FOR_PASS;
    }
    else
    {
        strcpy(tempStr,"401 User unknown, please try again.\r\n");
        write_message(controlfd,tempStr);
    }
}

void process_pass(char *password, enum states *ptrState, int controlfd)
{
    char tempStr[MAX_SIZE];
    if (*ptrState == STATE_WAITING_FOR_PASS)
    {
        if ( (strlen(password) == strlen("test3720")) && (strncmp("test3720",password,strlen(password))==0))
        {
            strcpy(tempStr,"102 Password ok.\r\n");
            write_message(controlfd,tempStr);
            *ptrState = STATE_LOGGED_IN;
        }
        else
        {
            strcpy(tempStr,"402 Password incorrect.\r\n");
            write_message(controlfd,tempStr);
        }

    }
    else if (*ptrState == STATE_CONNECTED)
    {
        strcpy(tempStr,"498 User not specified.\r\n");
        write_message(controlfd,tempStr);
    }
}

void print_not_logged_in(int controlfd)
{
    char tempStr[MAX_SIZE];

    strcpy(tempStr,"499 User not logged in.\r\n");
    write_message(controlfd,tempStr);
}

void process_quit(int controlfd, enum states *ptrState)
{
    char tempStr[MAX_SIZE];

    strcpy(tempStr,"200 Goodbye.\r\n");
    write_message(controlfd,tempStr);
    *ptrState = STATE_NOT_CONNECTED;
}

void print_command_not_okay(int controlfd)
{
    char tempStr[MAX_SIZE];
    strcpy(tempStr,"403 Command not okay.\r\n");
    write_message(controlfd,tempStr);
}

void print_command_not_okay_permission(int controlfd)
{
    char tempStr[MAX_SIZE];
    strcpy(tempStr,"405 Command not okay, permission denied.\r\n");
    write_message(controlfd,tempStr);
}


void print_command_okay(int controlfd)
{
    char tempStr[MAX_SIZE];
    strcpy(tempStr,"103 Command okay.\r\n");
    write_message(controlfd,tempStr);
}

void print_user_already_logged_in(int controlfd)
{
    char tempStr[MAX_SIZE];
    strcpy(tempStr,"497 User already logged in.\r\n");
    write_message(controlfd,tempStr);
}

void  print_unknown_command(int controlfd)
{
    char tempStr[MAX_SIZE];
    strcpy(tempStr,"403 Unknown command.\r\n");
    write_message(controlfd,tempStr);
}

int open_data_connection()
{
    int  datafd;


    if ((datafd = accept(listfd, (struct sockaddr*) NULL,NULL)) < 0)
    {
        perror("Error calling accept().");
        exit(EXIT_FAILURE);
    }

    fprintf(stderr,"Data connection established.\n");
    return datafd;

}


void process_list(int controlfd)
{
    char pathName[MAX_SIZE];
    char tempStr[MAX_SIZE];
    struct stat statBuf;
    struct dirent *dirp;
    DIR *dp;
    int fd;



    //pathName = get_current_dir_name();
    if (getcwd(pathName,MAX_SIZE-1) == NULL)
    {
        print_command_not_okay(controlfd);
        return;
    }

    if ( (dp=opendir(pathName)) == NULL)
    {
        print_command_not_okay(controlfd);
    }
    else
    {
        print_command_okay(controlfd);
        fd = open_data_connection();

        while ((dirp = readdir(dp)) != NULL)
        {
            if (strcmp(dirp->d_name,".") == 0 || strcmp(dirp->d_name,"..") == 0)
                continue;
            if (lstat(dirp->d_name,&statBuf) == 0)
            {
                if (S_ISREG(statBuf.st_mode))
                {
                    snprintf(tempStr,MAX_SIZE-1, "%s,%d\r\n",dirp->d_name,statBuf.st_size);
                    write_message(fd,tempStr);
                }

            }

        }
        close(fd);
    }

}

void process_get(int controlfd, char *filename)
{
    int filefd,fd;

    char tempStr[MAX_SIZE];
    int n;

    struct stat sb;

    if (stat(filename,&sb) == -1)
    {
        print_command_not_okay_permission(controlfd);
        return;
    }

    if (sb.st_mode & S_IFMT != S_IFREG)
    {
        print_command_not_okay_permission(controlfd);
        return;
    }

    filefd= open(filename,O_RDONLY);
    if (filefd == -1)
    {

        print_command_not_okay_permission(controlfd);
        return;
    }
    else
    {
        print_command_okay(controlfd);

    }

    fd = open_data_connection();
    n = read(filefd,tempStr,MAX_SIZE);

    while (n > 0)
    {
        write(fd,tempStr,n);
        n = read(filefd,tempStr,MAX_SIZE);
    }

    if (n == 0)
    {
        close(fd);
        close(filefd);
    }
    else if (n < 0)
    {
        fprintf(stderr,"Error reading file.\n");
        exit(EXIT_FAILURE);
    }


}

void process_put(int controlfd, char *filename)
{
    int filefd,fd;
    int bytesReceived = 0;

    char tempStr[MAX_SIZE];
    int n;

    filefd= open(filename,O_WRONLY|O_CREAT,S_IRWXU);
    if (filefd == -1)
    {
        print_command_not_okay_permission(controlfd);
        return;
    }
    else
    {
        print_command_okay(controlfd);

    }

    fd = open_data_connection();
    n = read(fd,tempStr,MAX_SIZE);

    while (n > 0)
    {
        bytesReceived += n;

        write(filefd,tempStr,n);
        n = read(fd,tempStr,MAX_SIZE);
    }

    if (n == 0)
    {
        close(fd);
        close(filefd);

        fprintf(stderr,"Bytes received:%d\n",bytesReceived);
    }
    else if (n < 0)
    {
        fprintf(stderr,"Error reading from socket.\n");
        exit(EXIT_FAILURE);
    }

}



int process(int controlfd, enum states *ptrState)
{
    int datafd;
    char cmdstring[MAX_SIZE];
    char *word;
    char command[MAX_SIZE];
    char arg[MAX_SIZE];
    int n;

    for(;;)
    {
        /*read command*/
        n = read(controlfd,cmdstring,MAX_SIZE-1); /*technically I should read until I see \r\n*/

        if (n==0)
        {
            fprintf(stderr,"Client disconnected.\n");
            close(controlfd);
            return 0;

        }
        cmdstring[n] = '\0';
        fprintf(stderr,"C:%s\n",cmdstring);

        /*parse*/
        word = strtok(cmdstring,SEPCHARS);
        if (word == NULL)
        {
            fprintf(stderr,"Error, no command given\n");
            close(controlfd);
            return (-1);
        }
        strncpy(command,word,MAX_SIZE);

        word = strtok(NULL,SEPCHARS);
        if (word == NULL)
        {
            strncpy(arg,"",MAX_SIZE);
        }
        else
        {
            strncpy(arg,word,MAX_SIZE);
        }

        fprintf(stderr,"command:%s,length:%d\n",command,strlen(command));
        fprintf(stderr,"arg:%s,length:%d\n",arg,strlen(arg));



        if (strncmp("QUIT",command,4) == 0)
        {
            process_quit(controlfd,ptrState);
            close(controlfd);
            return 0;
        }

        switch (*ptrState)
        {
            case STATE_CONNECTED :
                if ((strlen(command) == 4) && (strncmp("USER",command,4) == 0))  //command is USER
                {
                    process_user(arg,ptrState,controlfd);
                }
                else if ((strlen(command) == 4) && (strncmp("PASS",command,4) == 0))  //command is PASS
                {
                    process_pass(arg,ptrState,controlfd);
                }
                else /*no other command is viable*/
                {
                        print_not_logged_in(controlfd);

                }

                break;

            case STATE_WAITING_FOR_PASS:
                if ((strlen(command) == 4) && (strncmp("PASS",command,4) == 0))  //command is PASS
                {
                    process_pass(arg,ptrState,controlfd);
                }
                else if ((strlen(command) == 4) && (strncmp("USER",command,4) == 0))  //command is USER
                {
                    process_user(arg,ptrState,controlfd);
                }
                else
                {
                        print_not_logged_in(controlfd);
                }
                break;

            case STATE_LOGGED_IN:
                if ((strlen(command) == 4) && (strncmp("LIST",command,4) == 0) ) //command is LIST
                {
                    process_list(controlfd);

                }
                else if ((strlen(command) == 3) && (strncmp("GET",command,3) == 0))
                {
                    process_get(controlfd,arg);

                }
                else if ((strlen(command) == 3) && strncmp("PUT", command, 3) == 0)
                {
                    process_put(controlfd, arg);
                }
                else if ((strlen(command) == 4) &&  ((strncmp("PASS",command,4) == 0) || (strncmp("USER",command,4) == 0) ))
                {
                    print_user_already_logged_in(controlfd);
                }
                else
                {
                    print_unknown_command(controlfd);

                }

        } /*switch*/

    }
}

int main(int argc, char **argv)
{
    int listenfd, controlfd, datafd;    /*sockets*/
    uint16_t portNo;
    enum states state;

    struct sockaddr_in servaddr;


    if (argc != 2)
    {
        fprintf(stderr,"Usage: a2serv port\n");
        exit(EXIT_FAILURE);
    }

    portNo = atoi(argv[1]);


    listenfd = socket(AF_INET, SOCK_STREAM,0);
    if (listenfd == 1)
    {
        perror("Error calling socket().");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(portNo);

    if (bind(listenfd, (struct sockaddr *) &servaddr,
                               sizeof(servaddr)) < 0) {
        perror("Error calling bind().");
        exit(EXIT_FAILURE);
    }

    if (listen(listenfd,SERV_BACKLOG) < 0)
    {
        perror("Error calling listen().");
        exit(EXIT_FAILURE);
    }


    listfd = socket(AF_INET, SOCK_STREAM,0);
    if (listfd == 1)
    {
        perror("Error calling socket().");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(portNo+1);

    if (bind(listfd, (struct sockaddr *) &servaddr,
                               sizeof(servaddr)) < 0) {
        perror("Error calling bind().");
        exit(EXIT_FAILURE);
    }

    if (listen(listfd,SERV_BACKLOG) < 0)
    {
        perror("Error calling listen().");
        exit(EXIT_FAILURE);
    }


    for (;;)
    {
        state = STATE_NOT_CONNECTED;
        if ((controlfd = accept(listenfd, (struct sockaddr*) NULL,NULL)) < 0)
        {
            perror("Error calling accept().");
            exit(EXIT_FAILURE);
        }
        state = STATE_CONNECTED;
        process(controlfd,&state);

    }

    close(listfd);
    close(listenfd);
    return 0;

}
