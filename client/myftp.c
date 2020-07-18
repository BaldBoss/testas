
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h> /* struct sockaddr_in, htons, htonl */
#include <netdb.h>      /* struct hostent, gethostbyname() */
#include <string.h>
#include "stream.h" /* MAX_BLOCK_SIZE, readn(), writen() */
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include "token.h"
#include <fcntl.h>

#define SERV_TCP_PORT 40001 /* default server listening port */

void exeCommand(int TokenNumber, char *token[MAX_NUM_TOKENS], int sd, int i);
void handle_help();
void handle_lpwd();
void handle_ldir();
void handle_lcd(char *NameOfPath);
void handle_pwd(int sd, int i);
void handle_dir(int sd, int i);
void handle_cd(int sd, int i, char *NameOfPath);
void handleGetFile(int sd, char *NameOfFile);
void handlePutFile(int sd, char *NameOfFile);

int main(int argc, char *argv[])
{
  int sd, n, nr, nw, i = 0, TokenNumber;
  char buf[MAX_BLOCK_SIZE], host[60], *token[MAX_NUM_TOKENS];
  unsigned short port;
  struct sockaddr_in ser_addr;
  struct hostent *hp;
  handle_help();
	
  /* get server host name and port number */
  if (argc == 1)
  { /* assume server running on the local host and on default port */
    gethostname(host, sizeof(host));
    port = SERV_TCP_PORT;
  }
  else if (argc == 2)
  { /* use the given host name */
    strcpy(host, argv[1]);
    port = SERV_TCP_PORT;
  }
  else if (argc == 3)
  { // use given host and port for server
    strcpy(host, argv[1]);
    int n = atoi(argv[2]);
    if (n >= 1024 && n < 65536)
      port = n;
    else
    {
      printf("Error: server port number must be between 1024 and 65535\n");
      exit(1);
    }
  }
  else
  {
    printf("Usage: %s [ <server host name> [ <server listening port> ] ]\n", argv[0]);
    exit(1);
  }

  /* get host addres_nums, & build a server socket addres_nums */
  bzero((char *)&ser_addr, sizeof(ser_addr));
  ser_addr.sin_family = AF_INET;
  ser_addr.sin_port = htons(port);
  if ((hp = gethostbyname(host)) == NULL)
  {
    printf("host %s not found\n", host);
    exit(1);
  }
  ser_addr.sin_addr.s_addr = *(u_long *)hp->h_addr;

  /* create TCP socket & connect socket to server addres_nums */
  sd = socket(PF_INET, SOCK_STREAM, 0);
  if (connect(sd, (struct sockaddr *)&ser_addr, sizeof(ser_addr)) < 0)
  {
    perror("client connect");
    exit(1);
  }

  while (++i)
  {
    printf("Client Input[%d]: ", i);
    fgets(buf, sizeof(buf), stdin);
    nr = strlen(buf);
    if (buf[nr - 1] == '\n')
    {
      buf[nr - 1] = '\0';
      --nr;
    }
    if (strcmp(buf, "quit") == 0)
    {
      printf("Bye Bye client!!\n");
      exit(0);
    }
    TokenNumber = tokenise(buf, token);
    if (TokenNumber == -1)
    {
      printf("Error\n");
    }
    else
    {
      exeCommand(TokenNumber, token, sd, i);
    }
  }
}

void exeCommand(int TokenNumber, char *token[MAX_NUM_TOKENS], int sd, int i)
{
  int nw, nr, code_Interaction;
  char buffer[MAX_BLOCK_SIZE] = "";
  switch (TokenNumber)
  {
  case 1:
    /// Handle command on client
    if (strcmp(token[0], "lpwd") == 0)
    {
      handle_lpwd();
      break;
    }
    else if (strcmp(token[0], "ldir") == 0)
    {
      handle_ldir();
      break;
    }
    /// Handle command on server
    else if (strcmp(token[0], "pwd") == 0)
    {
      handle_pwd(sd, i);
      break;
    }
    else if (strcmp(token[0], "dir") == 0)
    {
      handle_dir(sd, i);
      break;
    }
    else if (strcmp(token[0], "help") == 0)
    {
      handle_help();
      break;
    }
    else if (strcmp(token[0], "quit") == 0)
    {
      int code_Interaction = 0;
      writen(sd, (char *)&code_Interaction, 4);
      exit(0);
    }
    else
    {
      printf("Wrong command input\n");
      break;
    }
  case 2:
    /// Handle command on client
    if (strcmp(token[0], "lcd") == 0)
    {
      handle_lcd(token[1]);
    }
    else if (strcmp(token[0], "cd") == 0)
    {
      handle_cd(sd, i, token[1]);
    }
    else if (strcmp(token[0], "get") == 0)
    {
      handleGetFile(sd, token[1]);
    }
    else if (strcmp(token[0], "put") == 0)
    {
      handlePutFile(sd, token[1]);
    }
    break;
  default:
    printf("Nothing");
  }
}

void handle_help()
{
	printf("Commands:\n");
	printf("put <filename> - send file to server\n");
	printf("get <filename> - request file from server\n");
	printf("pwd  - display the current directory path on the server\n");
	printf("lpwd - display the current local directory path\n");
	printf("dir  - display current directory listing on the server\n");
	printf("ldir - display current local directory listing\n");
	printf("cd   - change current directory on the server\n");
	printf("lcd  - change current local directory\n");
	printf("quit - terminate session\n");
	printf("help - display this information\n");
}
void handle_lpwd()
{
  char Directory[1024];
  getcwd(Directory, 1024);
  printf("%s\n", Directory);
}

void handle_ldir()
{
  DIR *dp;
  struct dirent *direntp;
  dp = opendir(".");
  if (dp)
  {
    while ((direntp = readdir(dp)) != NULL)
    {
      printf("%s\n", direntp->d_name);
    }
    closedir(dp);
  }
}

void handle_lcd(char *NameOfPath)
{
  int res_numult = chdir(NameOfPath);
  if (res_numult != 0)
  {
    printf("Failed change directory\n");
  }
  else
  {
    printf("Client: Finished change directory\n");
  }
}

void handle_pwd(int sd, int i)
{
  int nr, nw;
  char res_num;
  char buffer[MAX_BLOCK_SIZE] = "";
  char code_Interaction = 'a';

  if ((nw = writen(sd, &code_Interaction, 1)) < 1)
  {
    printf("Client: Send Error\n");
    exit(1);
  }
  readn(sd, &res_num, 1);
  fflush(stdout);
  if (res_num == '0')
  {
    printf("Server: Command pwd\tERROR\n");
  }
  else if (res_num == '1')
  {
    if ((nr = readn(sd, buffer, sizeof(buffer))) <= 0)
    {//server can send but client cannot receive
      printf("Client: Receive Error\n");
      exit(1);
    }
    buffer[nr] = '\0';
    printf("Sever Output[%d]: %s\n", i, buffer);
  }
  else
  {
    printf("Client: Response code Interaction Error\n");
  }

  fflush(stdout);
}

void handle_dir(int sd, int i)
{
  int nr, nw;
  char res_num;
  char buffer[MAX_BLOCK_SIZE] = "";
  char code_Interaction = 'b';
  if ((nw = writen(sd, &code_Interaction, 1)) < 1)
  {
    printf("Client: Send Error\n");
    exit(1);
  }
  readn(sd, &res_num, 1);
  fflush(stdout);
  if (res_num == '0')
  {
    printf("Server: Command pwd\tERROR\n");
  }
  else if (res_num == '1')
  {
    if ((nr = readn(sd, buffer, sizeof(buffer))) <= 0)
    {
      printf("Client: Receive error\n");
      exit(1);
    }
    buffer[nr] = '\0';
    printf("Sever Output[%d]:\n%s\n", i, buffer);
  }
  else
  {
    printf("Client: reponse code Interaction Error\n");
  }

  fflush(stdout);
}

void handle_cd(int sd, int i, char *NameOfPath)
{
  int nr, nw;
  char buffer[MAX_BLOCK_SIZE] = "";
  int pathLength = strlen(NameOfPath); // get path length
  char code_Interaction = 'c';
  char res_num;
  // write to server
  writen(sd, &code_Interaction, 1);
  writen(sd, (char *)&pathLength, 4);
  fflush(stdout);
  writen(sd, NameOfPath, pathLength);
  // receive from server
  readn(sd, &res_num, 1);
  if (res_num == '0')
  {
    strcat(buffer, "Failed change directory\0");
  }
  else if (res_num == '1')
  {
    strcat(buffer, "Finished change directory\0");
  }
  else
  {
    strcat(buffer, "Command cd error\0");
  }
  printf("Sever Output[%d]: %s\n", i, buffer);
  fflush(stdout);
}

void handleGetFile(int sd, char *NameOfFile)
{
  int fd, nr, nw;
  char res_num;
  char code_Interaction = 'd';
  // check for duplicate file
  if ((fd = open(NameOfFile, O_RDONLY)) >= 0)
  {
    printf("File already existed. Please input another file name\n");
    close(fd);
  }
  else
  {
    int length = strlen(NameOfFile);
    writen(sd, &code_Interaction, 1);
    writen(sd, (char *)&length, 4);
    writen(sd, NameOfFile, length);
    fflush(stdout);
    // receive from server
    readn(sd, &res_num, 1);
    if (res_num == '2')
    {
      printf("Client: File requested not exist \n");
    }
    else if (res_num == '1')
    {
      int SizeOfFile, SizeOfBlock = MAX_BLOCK_SIZE;
      if (readn(sd, (char *)&SizeOfFile, 4) <= 0)
      {
        printf("Client: Receive error\n");
        exit(1);
      }
      if (SizeOfFile < MAX_BLOCK_SIZE)
      {
        SizeOfBlock = SizeOfFile;
      }
      char content[SizeOfBlock];
      //create new file using same file name
      fd = open(NameOfFile, O_WRONLY | O_CREAT, 0766);

      while (SizeOfFile > 0)
      {
        if (SizeOfFile < SizeOfBlock)
        {
          SizeOfBlock = SizeOfFile;
        }
        //read file content from socket
        if ((nr = readn(sd, content, SizeOfBlock)) <= 0)
        {
          printf("Client: Receive error\n");
          exit(1);
        }
        //write to new file
        if ((nw = write(fd, content, nr)) < nr)
        {
          close(fd);
          return;
        }
        SizeOfFile -= nw;
      }

      if (SizeOfFile == 0)
      {
        printf("Client: Successfully received file\n");
      }

      memset(content, 0, SizeOfBlock);
      close(fd);
      fflush(stdout);
    }
    else
    {
      printf("Client: reponse code Interaction Error\n");
    }
  }
}

void handlePutFile(int sd, char *NameOfFile)
{
  int fd, nr, nw;
  char res_num;
  char code_Interaction = 'e';
  int length = strlen(NameOfFile);
  writen(sd, &code_Interaction, 1);           // send command code_Interaction to server
  writen(sd, (char *)&length, 4); // send length of file name to server
  writen(sd, NameOfFile, length);   // send file name to server
  fflush(stdout);

  readn(sd, &res_num, 1);

  if (res_num == '2')
  {
  printf("File exists in server\n");
  }
  else if (res_num == '0')
  {
    printf("Client: Error creating file in server\n");
  }
  else if (res_num == '1')
  {
    printf("Client: File accepted\n");

    if ((fd = open(NameOfFile, O_RDONLY)) == -1) //Open the file
    {
      printf("Client: File does not exist.\n");
      return;
    }
    int SizeOfFile;
    SizeOfFile = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    writen(sd, (char *)&SizeOfFile, 4);
    char ContentFile[MAX_BLOCK_SIZE];
    while ((nr = read(fd, ContentFile, MAX_BLOCK_SIZE)) > 0)
    {
      if (writen(sd, ContentFile, nr) == -1)
      {
        printf("Client: Error sending files.\n");
        return;
      }
    }
    printf("Client: Upload file completed.\n");
    memset(ContentFile, 0, MAX_BLOCK_SIZE);
    close(fd);
    fflush(stdout);
  }
  else
  {
    printf("Client: reponse code Interaction Error\n");
  }
}
