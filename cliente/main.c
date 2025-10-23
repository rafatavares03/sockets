#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

void getDomainAndPort(char *url, char **domain, char **port, char **path) {
  char *barra = strchr(url, '/');
  if(barra) {
    *barra = '\0';
    *path = barra + 1;
  } else {
    *path = "";
  } 
  *domain = strtok(url, ":");
  *port = strtok(NULL, ":");
  if(*port == NULL) *port = "80";
}

char *getFilenameFromPath(const char *path) {
   if (path == NULL || strlen(path) == 0) return strdup("index.html");
   const char *slash = strrchr(path, '/');
   if (slash && *(slash + 1) != '\0')
    return strdup(slash + 1);
   else
    return strdup("index.html");
}

int main(int argc, char *argv[]) {
  if(argc < 2) {
    printf("Parâmetros incorretos, execute: ./client www.site.com\n");
    return 1;
  }
  char *domain = NULL, *port = NULL, *path = NULL;
  char *url = (char*)malloc(strlen(argv[1]) + 1);
  strcpy(url, argv[1]);
  getDomainAndPort(url, &domain, &port, &path);
  printf("%s %s %s %s \n", domain, port, path, argv[1]);
  
  int status;
  struct addrinfo hints, *res;
  char ipstr[INET6_ADDRSTRLEN];
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM;
  //hints.ai_protocol = IPPROTO_TCP;
  
  status = getaddrinfo(domain, port, &hints, &res);
  if(status != 0) {
    printf("Infelizmente um erro ocorreu. %s\n", gai_strerror(status));
    return 1;
  }
  int socketFD;
  struct addrinfo *server = res;
  while(server != NULL) {
    socketFD = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if(socketFD != -1) {
      int connection = connect(socketFD, server->ai_addr, server->ai_addrlen);
      if(connection == -1) {
        perror("connect");
        close(socketFD);
      } else {
        break;
      }
    }

    server = server->ai_next;
  }
  if(server == NULL) {
    printf("Não foi possível conectar ao site.\n");
    return 1;
  }
  printf("Conexão bem sucedida!\n");
  char request[1000];
  snprintf(request, sizeof(request), 
    "GET /%s HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\n",
    (path == NULL) ? "" : path, domain, port
  );
  send(socketFD, request, strlen(request), 0);
  char buffer[8192];
  int bytes;
  int header = 1;
  FILE *f = NULL;
  bytes = recv(socketFD, buffer, sizeof(buffer), 0);
  printf("%d\n", bytes);
  while(bytes > 0) {
    if(header) {
      buffer[bytes] = '\0';
      char *body = strstr(buffer, "\r\n\r\n");
      if(body) {
        header = 0;
        body += 4;
        size_t tamanhoDoHeader = body - buffer;
        size_t tamanhoDoBody = bytes - tamanhoDoHeader;
        f = fopen(getFilenameFromPath(path), "wb");
        if(!f) {
          printf("Não foi possível criar o arquivo.\n");
          free(url);
          close(socketFD);
          return 1;
        }
        fwrite(buffer + tamanhoDoHeader, 1, tamanhoDoBody, f);
      }
    } else {
      fwrite(buffer, 1, bytes, f);
    }
    bytes = recv(socketFD, buffer, sizeof(buffer), 0);
  }
  if(f) fclose(f);
  free(url);
  freeaddrinfo(res);
  close(socketFD);
  return 0;
}