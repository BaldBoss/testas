#include <stdlib.h> /* strlen(), strcmp() etc */
#include <stdio.h>  /* printf()  */
#include <string.h> /* strlen(), strcmp() etc */
#include <errno.h>  /* extern int errno, EINTR, perror() */
#include <signal.h> /* SIGCHLD, sigaction() */
#include <syslog.h>
#include <sys/types.h>  /* pid_t, u_long, u_short */
#include <sys/socket.h> /* struct sockaddr, socket(), etc */
#include <sys/wait.h>   /* waitpid(), WNOHAND */
#include <netinet/in.h> /* struct sockaddr_in, htons(), htonl(), */
                        /* and INADDR_ANY */
#include "stream.h"     /* MAX_BLOCK_SIZE, readn(), writen() */
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>

#define SERV_TCP_PORT 40001 /* default server listening port */
#define logfile "logFile.log"

time_t rawtime;
struct tm *TimeInfor;
void claim_children()
{
     pid_t pid = 1;

     while (pid > 0)
     { /* claim as many zombies as we can */
          pid = waitpid(0, (int *)0, WNOHANG);
     }
}

void daemon_init(void)
{
     pid_t pid;
     struct sigaction act;

     if ((pid = fork()) < 0)
     {
          perror("fork");
          exit(1);
     }
     else if (pid > 0)
     {
          printf("Hey, you'd better remember my PID: %d\n", pid);
          exit(0); /* parent goes bye-bye */
     }

     /* child continues */
     setsid();   /* become session leader */
     chdir("/"); /* change working directory */
     umask(0);   /* clear file mode creation mask */

     /* catch SIGCHLD to remove zombies from system */
     act.sa_handler = claim_children; /* use reliable signal */
     sigemptyset(&act.sa_mask);       /* not to block other signals */
     act.sa_flags = SA_NOCLDSTOP;     /* not catch stopped children */
     sigaction(SIGCHLD, (struct sigaction *)&act, (struct sigaction *)0);

}
// handle dirrectory command
void handle_directory(int sd, FILE *logFile)
{
     DIR *dp;
     struct dirent *direntp;
     char res_num = '1';
     char buf[MAX_BLOCK_SIZE] = "";

     time(&rawtime);
     TimeInfor = localtime(&rawtime);

     if ((dp = opendir(".")) == NULL)
     {
          fprintf(logFile, "[%s]Command pwd\t[ERROR]\n", asctime(TimeInfor));
          res_num = '0';
          writen(sd, &res_num, 1);
          return;
     }
     writen(sd, &res_num, 1);
     while ((direntp = readdir(dp)) != NULL)
     {
          strcat(buf, direntp->d_name);
          strcat(buf, "\t");
     }
     closedir(dp);

     fprintf(logFile, "[%s]Command pwd\t[SUCCESS]\n", asctime(TimeInfor));
     int size = strlen(buf);
     buf[size] = '\0';
     writen(sd, buf, size);
     fflush(stdout);
}
// handle pwd command
void handle_pwd(int sd, FILE *logFile)
{
     char cwd[MAX_BLOCK_SIZE];
     char buf[MAX_BLOCK_SIZE] = "";
     char res_num = '1';
     time(&rawtime);
     TimeInfor = localtime(&rawtime);

     if (getcwd(cwd, sizeof(cwd)) == NULL)
     {
          fprintf(logFile, "[%s]Command pwd\t[ERROR]\n", asctime(TimeInfor));
          res_num = '0';
          writen(sd, &res_num, 1);
          return;
     }
     else
     {
          writen(sd, &res_num, 1);
          fprintf(logFile, "[%s]Command pwd\t[SUCCESS]\n", asctime(TimeInfor));
          strcat(buf, cwd);
          int size = strlen(buf);
          buf[size] = '\0';
          writen(sd, buf, size);
     }
     fflush(stdout);
}
// handle cd command
void handle_cd(int sd, FILE *logFile)
{
     int LengthOfPath;
     int size;
     char res_num = '1';
     //read length of path
     readn(sd, (char *)&LengthOfPath, 4);
     char buf[LengthOfPath + 1];
     readn(sd, buf, LengthOfPath);
     buf[LengthOfPath] = '\0';
     int res_numult = chdir(buf);
     if (res_numult != 0)
     {
          // write to logFile
          fprintf(logFile, "[%s]Command cd\t[ERROR]: Cannot change directory\n", asctime(TimeInfor));
          //write to client
          res_num = '0';
          writen(sd, &res_num, 1);
          return;
     }
     // successful change of directory
     fprintf(logFile, "[%s]Command cd\t[SUCCESS]\n", asctime(TimeInfor));
     //write to client
     writen(sd, &res_num, 1);
     fflush(stdout);
}
//handle get command
void handle_get(int sd, FILE *logFile)
{
     int fd, SizeOfFile, nr;
     int LengthOfName;
     char res_num = '1';
     //read length of file name
     readn(sd, (char *)&LengthOfName, 4);
     fflush(stdout);
     char FileName[LengthOfName];
     //read file name
     readn(sd, FileName, LengthOfName);

     FileName[LengthOfName] = '\0';
     //check if the file is exist
     if ((fd = open(FileName, O_RDONLY)) == -1)
     {
          fprintf(logFile, "[%s]File not exist\t[ERROR]\n", asctime(TimeInfor));
          // write to client
          res_num = '2';
          writen(sd, &res_num, 1);
          return;
     }
     //get the file size
     SizeOfFile = lseek(fd, 0, SEEK_END);
     //assign pointer back to the beginning of the file
     lseek(fd, 0, SEEK_SET);
     //send file size to client
     writen(sd, &res_num, 1);
     writen(sd, (char *)&SizeOfFile, 4);

     char content[MAX_BLOCK_SIZE];
     //read the file and send to client
     while ((nr = read(fd, content, MAX_BLOCK_SIZE)) > 0)
     {
          if (writen(sd, content, nr) <= 0)
          {
               fprintf(logFile, "[%s]Sending file\t[ERROR]\n", asctime(TimeInfor));
               return;
          }
     }
     close(fd);
     fprintf(logFile, "Successfully sending file\n");
     fflush(logFile);
}
//handle put file request
void handle_put(int sd, FILE *logFile)
{
     int fd, LengthOfName;
     char res_num = '1';
     readn(sd, (char *)&LengthOfName, 4);
     fflush(stdout);
     char FileName[LengthOfName + 1];
     readn(sd, FileName, LengthOfName);
     FileName[LengthOfName] = '\0';

     if ((fd = open(FileName, O_RDONLY)) != -1)
     {
          fprintf(logFile, "[%s]File already exist\t[ERROR]\n", asctime(TimeInfor));
          res_num = '2';
     }
     else if ((fd = open(FileName, O_WRONLY | O_CREAT, 0766)) == -1)
     {
          fprintf(logFile, "[%s]Create new file\t[ERROR]\n", asctime(TimeInfor));
          res_num = '0';
     }
     else
     {
          fprintf(logFile, "[%s]Create new file \t[SUCCESS]\n", asctime(TimeInfor));
     }
     writen(sd, &res_num, 1);

     if (res_num == '1')
     {
          int nr, nw;
          int SizeOfFile, blockSize = MAX_BLOCK_SIZE;
          readn(sd, (char *)&SizeOfFile, 4); // get file size from client
          if (blockSize > SizeOfFile)        // get accurate file size for receiving later
          {
               blockSize = SizeOfFile;
          }
          char content[blockSize];
          while (SizeOfFile > 0)
          {
               if (blockSize > SizeOfFile) // get accurate file size to get x bytes from client
               {
                    blockSize = SizeOfFile;
               }
               nr = readn(sd, content, blockSize);     // get filecontent from client
               if ((nw = write(fd, content, nr)) < nr) // write filecontent to file
               {
                    fprintf(logFile, "[%s]\tServer wrote only %d bytes out of %d\n", asctime(TimeInfor), nw, nr); //print message to log
                    close(fd);
                    return;
               }
               SizeOfFile -= nw;
          }
          close(fd);
          fprintf(logFile, "[%s]Creating new file\t[SUCCESS]\n", asctime(TimeInfor));
     }
     else
     {
          fprintf(logFile, "[%s]Creating new file\t[ERROR]\n", asctime(TimeInfor));
     }
     fflush(logFile);
}
// serve a client
void serve_a_client(int sd, FILE *logFile)
{
     int nr, nw;
     char client_code;
     // printt to logFile
     time(&rawtime);
     TimeInfor = localtime(&rawtime);
     fprintf(logFile, "Client[%d] has been connected\t\t\t[time]: %s\n", getpid(), asctime(TimeInfor));

     while (1)
     {
          /* read data from client */
          if (readn(sd, &client_code, 1) <= 0)
          {
               return; /* connection broken down */
          }
          /* process data */
          switch (client_code) // switch for client code received
          {
          case '0':
               fprintf(logFile, "[%s]\tClient %d has disconnect\n", asctime(TimeInfor), getpid()); // write log
               fflush(logFile);                                                                   // flush remaining message to log to ensure nothing is left behind
               break;
          case 'a':
               handle_pwd(sd, logFile);
               break;
          case 'b':
               handle_directory(sd, logFile);
               break;
          case 'c':
               handle_cd(sd, logFile);
               break;
          case 'd':
               handle_get(sd, logFile);
               break;
          case 'e':
               handle_put(sd, logFile);
               break;
          default:
               fprintf(logFile, "Invalid client code received\n");
               fflush(logFile); // flush remaining message to log to ensure nothing is left behind
               break;
          }
     }
     fclose(logFile);
}

int main(int argc, char *argv[])
{
     FILE *logFile;
     int sd, nsd, n;
     pid_t pid;
     unsigned short port; // server listening port
     socklen_t cli_addrlen;
     struct sockaddr_in ser_addr, cli_addr;

     // time for logFile
     time(&rawtime);
     TimeInfor = localtime(&rawtime);

     // create log files
     logFile = fopen(logfile, "w+");
     if (logFile == NULL)
     {
          printf("Error opening log file\n");
          exit(1);
     }

     // print to logFile
     time(&rawtime);
     TimeInfor = localtime(&rawtime);

     fprintf(logFile, "Server is running\t\t\t\t[time]: %s\n", asctime(TimeInfor));
     fflush(logFile);

     /* get the port number */
     if (argc == 1)
     {
          port = SERV_TCP_PORT;
     }
     else if (argc == 2)
     {
          char *init_curr_dir = argv[1];
          printf("Initial current directory: %s\n", init_curr_dir);
          chdir(init_curr_dir);
     }
     else
     {
          printf("Usage: %s [ server listening port ]\n", argv[0]);
          exit(1);
     }

     /* turn the program into a daemon */
     daemon_init();

     /* set up listening socket sd */
     if ((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0)
     {
          perror("server:socket");
          exit(1);
     }

     /* build server Internet socket addres_nums */
     bzero((char *)&ser_addr, sizeof(ser_addr));
     ser_addr.sin_family = AF_INET;
     ser_addr.sin_port = htons(port);
     ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);
     /* note: accept client request sent to any one of the
        network interface(s) on this host.
     */

     /* bind server addres_nums to socket sd */
     if (bind(sd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0)
     {
          perror("server bind");
          exit(1);
     }

     /* become a listening socket */
     listen(sd, 5);

     while (1)
     {

          /* wait to accept a client request for connection */
          cli_addrlen = sizeof(cli_addr);
          nsd = accept(sd, (struct sockaddr *)&cli_addr, &cli_addrlen);
          if (nsd < 0)
          {
               if (errno == EINTR) /* if interrupted by SIGCHLD */
                    continue;
               perror("server:accept");
               exit(1);
          }

          /* create a child process to handle this client */
          if ((pid = fork()) < 0)
          {
               perror("fork");
               exit(1);
          }
          else if (pid > 0)
          {
               close(nsd);
               continue; /* parent to wait for next client */
          }

          /* now in child, serve the current client */
          close(sd);
          serve_a_client(nsd, logFile);
          exit(0);
     }
}
