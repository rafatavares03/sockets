#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include<sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>

void getDomainAndPort(char *url, char **domain, char **port, char **path) {
  char *inicio = strstr(url, "://");
  if(inicio) url = inicio + 3;

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

void handleDomainErrors(int error) {
  switch (error){
  case EAI_AGAIN:
    printf("Não foi possível encontrar o domínio no momento, tente novamente mais tarde.\n");
    break;
  case EAI_FAIL:
    printf("Ocorreu uma falha ao tentar encontrar este domínio.\n");
    break;
  default:
    printf("ERROR: %s\n", gai_strerror(error));
    break;
  }
}

void handleHTTPStatus(int status) {
  switch (status) {
  case 300:
    printf("HTTP status code 300 - Há múltiplos recursos para serem baixados nesse endereço, por favor escolha um deles.\n");
    break;
  case 301:
    printf("HTTP status code 301 - O endereço do recurso mudou permanentemente.\n");
    break;
  case 302:
    printf("HTTP status code 302 - O endereço do recurso mudou temporariamente. Tente acessá-lo novamente mais tarde.\n");
    break;
  case 400:
    printf("HTTP status code 400 - Houve erro na solitação do cliente.\n");
    break;
  case 401:
    printf("HTTP status code 401 - Recurso não autorizado.\n");
    break;
  case 403:
    printf("HTTP status code 403 - Cliente não tem autorização para acessar o recurso.\n");
    break;
  case 404:
    printf("HTTP status code 404 - Recurso não encontrado.\n");
    break;
  case 408:
    printf("HTTP status code 408 - Servidor encerrou a comunicação por limite de tempo.\n");
    break;
  case 500:
    printf("HTTP status code 500 - Houve um erro no servidor.\n");
    break;
  case 501:
    printf("HTTP status code 501 - Solicitação ainda não é suportada pelo servidor.\n");
    break;
  case 502:
    printf("HTTP status code 502 - Servidor obteve uma resposta inválida para o recurso solicitado.\n");
    break;
  case 503:
    printf("HTTP status code 503 - Servidor não pode atender a solicitação no momento.\n");
    break;
  case 504:
    printf("HTTP status code 504 - Servidor demorou muito para responder, portanto a comunicação foi encerrada.\n");
    break;
  default:
    if(status > 300) {
      printf("HTTP status code %d\n", status);
    }
    break;
  }
}

char *getFilenameFromPath(const char *path) {
  if (path == NULL || strlen(path) == 0) {
    return strdup("index.html");
  }
  const char *barra = strrchr(path, '/');
  if (barra && *(barra + 1) != '\0') {
    return strdup(barra + 1);
  }
    return strdup(path);
}

void encodeURL(char *dest, const char *src, int srcLength) {
  const char *hex = "0123456789ABCDEF";
  int j = 0;
  for(int i = 0; i < srcLength; i++) {
    unsigned char c = (unsigned char)src[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      dest[j] = c;
      j++;
    } else {
      dest[j] = '%';
      dest[j+1] = hex[c >> 4];
      dest[j+2] = hex[c & 15];
      j += 3;
    }
  }
  dest[j] = '\0';
}

int main(int argc, char *argv[]) {
  if(argc < 2) {
    printf("Parâmetros incorretos, execute: ./client http://www.site.com\n");
    return 1;
  }
  char *domain = NULL, *port = NULL, *path = NULL;
  char *url = (char*)malloc(strlen(argv[1]) + 1);
  strcpy(url, argv[1]);
  getDomainAndPort(url, &domain, &port, &path);
  
  int status;
  struct addrinfo hints, *res;
  char ipstr[INET6_ADDRSTRLEN];
  
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC; 
  hints.ai_socktype = SOCK_STREAM;
  
  status = getaddrinfo(domain, port, &hints, &res);
  if(status != 0) {
    handleDomainErrors(status);
    return 1;
  }
  int socketFD;
  struct addrinfo *server = res;
  while(server != NULL) {
    socketFD = socket(server->ai_family, server->ai_socktype, server->ai_protocol);
    if(socketFD != -1) {
      int connection = connect(socketFD, server->ai_addr, server->ai_addrlen);
      if(connection == -1) {
        perror("ERROR");
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
  char request[1000];
  char reqPort[10] = "", encodedPath[500];
  if(strcmp(port, "80") != 0) {
    snprintf(reqPort, sizeof(reqPort), ":%s", port);
  }
  if(path == NULL) {
    path = "";
  }
  encodeURL(encodedPath, path, strlen(path));
  snprintf(request, sizeof(request), 
    "GET /%s HTTP/1.1\r\nHost: %s%s\r\nConnection: close\r\n\r\n",
    encodedPath, domain, reqPort
  );

  send(socketFD, request, strlen(request), 0);
  char buffer[8192];
  char headerBuffer[8192] = {0};
  size_t headerLen = 0;
  int header = 1;
  int httpStatus = 0;
  FILE *f = NULL;
  char *filename = getFilenameFromPath(path);
  ssize_t bytes;
  while ((bytes = recv(socketFD, buffer, sizeof(buffer), 0)) > 0) {
    if (header) {
      if (headerLen + bytes < sizeof(headerBuffer)) {
        memcpy(headerBuffer + headerLen, buffer, bytes);
        headerLen += bytes;
        headerBuffer[headerLen] = '\0';
      }
      char *body = strstr(headerBuffer, "\r\n\r\n");
      if (body) {
        header = 0;
        body += 4;
        size_t headerSize = body - headerBuffer;
        size_t bodySize = headerLen - headerSize;
        sscanf(headerBuffer, "HTTP/%*s %d", &httpStatus);
        f = fopen(filename, "wb");
        if (!f) {
          perror("Erro ao criar arquivo");
          break;
        }
        fwrite(body, 1, bodySize, f);
      }
    } else {
      fwrite(buffer, 1, bytes, f);
    }
  }
  handleHTTPStatus(httpStatus);
  if(f) {
    printf("Arquivo %s foi salvo.\n", filename);
    fclose(f);
  } else {
    printf("Infelizmente não foi possível baixar o recurso.\n");
  }
  free(url);
  free(filename);
  freeaddrinfo(res);
  close(socketFD);
  return 0;
}