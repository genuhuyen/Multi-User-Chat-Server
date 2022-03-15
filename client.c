/*
  *Huyen Tran
  *10/18/20
  * client.c - a chat server that uses sockets to communicate w/ server
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/select.h>
#include <stdbool.h>
#define PORTNO "12000" //default port #
#define BUFMAX 1000

/**
 * nonblock - a function that makes a file descriptor non-blocking
 * @param fd file descriptor
 */
void nonblock(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL, 0)) == -1) {
    perror("fcntl (get):");
    exit(1);
  }
  if (fcntl(fd, F_SETFL, flags | FNDELAY) == -1) {
    perror("fcntl (set):");
    exit(1);
  }

}


int main(int argc, char **argv){
  int ch;
  int c_fd; //c_fd = client fd
  char buf[BUFMAX];
  struct addrinfo hints, *s_info; //sinfo = server info
  char *s_port = strdup(PORTNO); //default server port
  char *host = strdup("localhost"); //default host
  bool n = false; //nickname flag
  char *nkname; //nickname
  int val = 1;
  ssize_t byteread;
  ssize_t namelen;


  while((ch = getopt(argc, argv, "h:p:n:")) != -1){
    switch (ch){
      case 'h':{
        host = strdup(optarg);
        break;
      }
      case 'p':
        s_port = strdup(optarg);
        break;
      case 'n':
        nkname = strdup(optarg);
        n = true;
        break;
      default:
        printf("usage: ./client [-h help menu] [-p port #] [-n]\n");
        break;
    }
  }

  //set hints
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  //get server ip address
  if(getaddrinfo(host, s_port, &hints, &s_info) != 0){
    perror("getaddrinfo failed.\n");
    exit(1);
  }


  //make socket
  c_fd = socket(AF_INET, SOCK_STREAM, 0);
  //make socket reusable
  setsockopt(c_fd, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  if(c_fd == -1){
    perror("socket creation failed.\n");
    exit(1);
  }

  //connect to first ip address of server
  if(connect(c_fd, s_info->ai_addr, s_info->ai_addrlen) != 0){
    close(c_fd);
    perror("client: connection failed\n");
    exit(1);
  }
  else{
    printf("connected to server ...\n");
  }

  fd_set readfds;
  int fdmax;
  fdmax = 10;
  //force nonblock
  nonblock(c_fd);

  while(1){
    //initialize
    FD_ZERO(&readfds);
    //add input to read set
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(c_fd, &readfds);


    int fdready = select(fdmax+1, &readfds, NULL, NULL, NULL);
    //nothing is ready
    if(fdready < 0){
      perror("select failed");
      exit(1);
    }
    for(int i=0; i < fdmax; i++){
      if(FD_ISSET(i, &readfds)){
        if(i == c_fd){
          //read from server
          byteread = read(c_fd, buf, BUFMAX);
          if(byteread == -1 && errno != 35){ //errno 35= EAGAIN
            perror("read from server failed");
            exit(1);
          }
          else if(byteread == 0){
            close(c_fd);
            break;
          }
          //write to screen
          if(write(STDOUT_FILENO, buf, byteread) == -1){
            perror("write to server failed");
            exit(1);
            }
        }
        else{
          //read data from keyboard w/ nkname
          //write nickname to buffer
          if(n == true){
            namelen = sprintf(buf, "[%s]: ", nkname);
            byteread = read(STDIN_FILENO, &buf[namelen], BUFMAX);
            if(byteread == -1){
              perror("read failed");
              exit(1);
            }
            else if(byteread == 0){
              break;
            }
            //write to server
            if(write(c_fd, buf, byteread + namelen) == -1){
              perror("write to server failed");
              exit(1);
              }
          }
          //no nickname
          else{
            //read data from keyboard w/o nkname
            byteread = read(STDIN_FILENO, buf, BUFMAX);
            if(byteread == -1){
              perror("read failed");
              exit(1);
            }
            else if(byteread == 0){
              break;
            }
            //write to server
            if(write(c_fd, buf, byteread) == -1){
              perror("write to server failed");
              exit(1);
              }
          }
        }
      }
    }
  }

  printf("hanging up\n");
  free(s_info);

  return 0;
}
