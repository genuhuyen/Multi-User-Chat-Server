/*Huyen Tran
 *10/18/20
  *note:
          nicknames : can be signaled on the client side by using -n (will show nicknames of clients);
                      if -n is signaled on server command line, it will show [monitor]:
          connections: only announce ip addresses and not nicknames with it
                      (i had problem with printf printing out of order when using nicknames + ip)
                      can be signlaed with -c
          overall program issues: also have issues with disconnect using ^D, but works with ^C
 * server.c - a chat server (and monitor) that uses pipes and sockets
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/file.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <stdbool.h>

#define MAX_CLIENTS 10

// constants for pipe FDs
#define WFD 1
#define RFD 0
#define PORTNO 12000
#define SA struct sockaddr
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

/*
 * monitor - provides a local chat window
 * @param srfd - server read file descriptor
 * @param swfd - server write file descriptor
 */
void monitor(int srfd, int swfd, bool n) {
  char buf[BUFMAX];
  ssize_t byteread;
  fd_set readfds;
  int fdmax = 100;
  int fdready;
  nonblock(srfd);
  nonblock(swfd);
  ssize_t monlen;
  while(1){
    //initialize
    FD_ZERO(&readfds);

    //add keyboard and server read fd to fd read set
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(srfd, &readfds);

    //select whatever's ready to be read
    fdready = select(fdmax+1, &readfds, NULL, NULL, NULL);
    //nothing is ready
    if(fdready < 0){
      perror("select failed");
      exit(1);
    }
    for(int i=0; i < fdmax; i++){
      if(FD_ISSET(i, &readfds)){
        if(i == srfd){
          //read from server
          byteread = read(srfd, buf, BUFMAX);
          if(byteread == -1 && errno !=35){
            perror("read failed");
            exit(1);
          }
          else if(byteread == 0){
            close(srfd);
            break;
          }

          //write to screen
          if(write(STDOUT_FILENO, buf, byteread) == -1){
            perror("write to server failed");
            exit(1);
          }

        }
        else if(i == 0){
          //if nickname is signaled, we print out [monitor]:
          if(n == true){
            //read from keyboard
            monlen = sprintf(buf, "[monitor]: ");
            byteread = read(STDIN_FILENO, &buf[monlen], BUFMAX);
            if(byteread == -1){
              perror("read from keyboard failed");
              exit(1);
            }
            else if(byteread == 0){
              break;
            }
            //write to server
            if(write(swfd, buf, byteread + monlen) == -1){
              perror("write to server failed");
              exit(1);
            }
          }
          else{
            //read from keyboard
            //monlen = sprintf(buf, "[monitor]: ");
            byteread = read(STDIN_FILENO, buf, BUFMAX);
            if(byteread == -1){
              perror("read from keyboard failed");
              exit(1);
            }
            else if(byteread == 0){
              break;
            }
            //write to server
            if(write(swfd, buf, byteread) == -1){
              perror("write to server failed");
              exit(1);
            }
          }
        }
      }
    }
  }
  close(srfd);
  close(swfd);
}

/*
 * server - relays chat messages
 * @param mrfd - monitor read file descriptor
 * @param mwfd - monitor write file descriptor
 * @param portno - TCP port number to use for client connections
 */
void server(int mrfd, int mwfd, int portno, bool c) {
  //create socket & bind  to all tcp interfaces
  int s_socket, new_sock, sd; //new_sock = client conn, s_socket = server sock
  int active = 0;
  socklen_t len;
  struct sockaddr_in sinfo, client; //server info
  char buf[BUFMAX];
  int max_clients = 10;
  int clist[max_clients]; //list of client sockets fds
  fd_set readfds;
  int val = 1;
  ssize_t byteread;
  int max_fd = 100; //temp, we'll check later
  struct timeval timeout;

  //initialize timeout
  timeout.tv_sec = 0; // seconds
  timeout.tv_usec = 100000; // 0 microseconds
  //initialize client list of fd to all 0's
  for(int i = 0; i < max_clients; i++){
    clist[i] = 0;
  }

  //create server socket to be used when clients want to connect
  s_socket = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(s_socket, SOL_SOCKET, SO_REUSEPORT, &val, sizeof(val));
  if(s_socket == -1){
    perror("socket creation failed.\n");
    exit(1);
  }

  //assign server info
  sinfo.sin_family = AF_INET;
  sinfo.sin_addr.s_addr = htonl(INADDR_ANY);
  sinfo.sin_port = htons(portno);

  //bind
  if((bind(s_socket, (SA*)&sinfo, sizeof(sinfo))) != 0){
    perror("bind failed");
    exit(1);
  }

  //listen for incoming messages
  if(listen(s_socket, 5) != 0){
    perror("listen failed");
    exit(1);
  }


  //force nonblock
  nonblock(s_socket);
  nonblock(mwfd);
  nonblock(mrfd);
  while(1){
    //clear socket set
    FD_ZERO(&readfds);
    //add server socket to setsockopt
    FD_SET(s_socket, &readfds);
    FD_SET(mrfd, &readfds);
    max_fd = s_socket;


    for(int i = 0; i < max_clients; i++){
      sd = clist[i];
      nonblock(sd);
      if(sd > 0){
        FD_SET(sd, &readfds);
        active++;
      }
      if(sd > max_fd){
        max_fd = sd;
      }
    }
    //select active clients
    active = select(max_fd + 1, &readfds, NULL, NULL, &timeout);

    if((active < 0) && (errno != EINTR)){
      perror("select failed");
    }

    //if someone is trying to connect ... then try accepting
    if(FD_ISSET(s_socket, &readfds)){
      if((new_sock = accept(s_socket, (SA*)&client, &len)) < 0){
        perror("accept failed");
        exit(1);
      }
      nonblock(new_sock);
      //add new client to list of clients
      for(int i = 0; i < max_clients; i++){
        if(clist[i] == 0){
          clist[i] = new_sock;

          if(c == true){
            printf("Client (%s) has connected\n", inet_ntoa(client.sin_addr));
          }
          break;
        }
      }
    }
    //monitor has a message
    else if(FD_ISSET(mrfd, &readfds)){
      //wait for respond from monitor
      //read from monitor
      byteread = read(mrfd, buf, BUFMAX);
      if(byteread == -1 && errno != 35){
        perror("read from monitor failed");
        exit(1);
      }
      else if(byteread == 0){
        break;
      }
      //write msg to every all clients
      for(int i = 0; i < max_clients; i++){
        if(clist[i] != 0){
          write(clist[i], buf, byteread);
        }
      }
    }

    //check to see if someone disconnected// recv message and broadcast
    for(int i = 0; i < max_clients; i++){
      sd = clist[i];
      if(FD_ISSET(sd, &readfds)){
        //is someone trying to disconnect??
        if((byteread = read(sd, buf, 1024)) == 0){
          if(c == true){
            printf("Client (%s) has disconnected\n", inet_ntoa(client.sin_addr));
          }
          close(sd);
          clist[i] = 0; //update that pos in the client list to reuse
        }

        //send msg to mon
        else{
          read(sd, buf, BUFMAX);
          if(byteread == -1 && errno != 35){
            perror("read failed");
            exit(1);
          }
          else if(byteread == 0){
            break;
          }

          //write to monitor
          if(write(mwfd, buf, byteread) == -1){
            perror("write to monitor failed");
            exit(1);
          }
          //write to other clients too!
          for(int i = 0; i < max_clients; i++){
            if(clist[i] != 0 && clist[i] != sd){
              write(clist[i], buf, byteread);
            }
        }
      }
    }
  }
}
    close(s_socket);
    close(new_sock);
    close(mrfd);
    close(mwfd);
  }


int main(int argc, char **argv) {
  int ch; //menu options
  int portno = PORTNO; //default server port
  int s2m[2]; //relay server to monitor = parent
  int m2s[2]; //monitor to relay server = child
  pid_t p;
  bool nkname = false; //nickname
  bool cannounce = false; //display connections & ip

  while((ch = getopt(argc, argv, "h:p:nc")) != -1){
    switch (ch){
      case 'h':{
        printf("usage: ./server [-h] [-p port #] [-n] [-c]\n");
        printf("-h - this help message\n");
        printf("-p # - the port to use when connecting to the server\n");
        printf("-n - signals nicknames");
        printf("-c - this will announce connections and disconnections\n");
        exit(1);
      }
      case 'p':
        portno = atoi(optarg);
        break;
      case 'n':
        nkname = true;
        break;
      case 'c':
        cannounce = true;
        break;
      default:
        printf("usage: ./server [-h help menu] [-p port #] [-n]  [-c]\n");
        break;
    }
  }
  //create pipes
  if(pipe(s2m) == -1){
    perror("pipe failed");
    exit(1);
  }
  if(pipe(m2s) == -1){
    perror("pipe failed");
    exit(1);
  }

  //create parent(relay server) & child(monitor) process
  p = fork();

  if(p < 0){
    perror("fork failed");
  }
  //call nonblock on all socket returned from pipe
  nonblock(s2m[RFD]);
  nonblock(s2m[WFD]);
  nonblock(m2s[RFD]);
  nonblock(m2s[WFD]);

  //parent = server
  if(p > 0){
    //close reading end of first pip
    close(s2m[RFD]);
    server(m2s[RFD], s2m[WFD], portno, cannounce);
    wait(NULL);
    close(m2s[RFD]);
  }

  //child = monitor
  else{
    //close writing end of first pipe
    close(m2s[RFD]);
    close(s2m[WFD]);
    monitor(s2m[RFD], m2s[WFD], nkname);
    exit(0);
  }

  return 0;
}
