#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>

#define PORT "1303"

char folderPath[1000];

void decodeURL(char *dest, const char *src, int srcLength) {
  char a, b;
  int i = 0, j = 0;
  while (i < srcLength) {
    if (src[i] == '%' && i + 2 < srcLength &&
      isxdigit(src[i+1]) && isxdigit(src[i+2])) {
      a = tolower(src[i+1]);
      b = tolower(src[i+2]);
      a = (a >= 'a') ? a - 'a' + 10 : a - '0';
      b = (b >= 'a') ? b - 'a' + 10 : b - '0';
      dest[j] = 16 * a + b;
      j++;
      i += 3;
    } else if (src[i] == '+') {
      dest[j] = ' ';
      j++;
      i++;
    } else {
      dest[j] = src[i];
      j++;
      i++;
    }
  }
  dest[j] = '\0';
}

void sendData(int socket, char *buffer, int bufferLength) {
  int total = 0;
  char *aux = buffer;
  while(total < bufferLength) {
    int bytesSent = send(socket, aux + total, bufferLength - total, 0);
    total += bytesSent;
  }
}

void *handle_client(void *arg) {
  int clientFD = *(int*)arg;
  free(arg);
  char buffer[8000];
  char method[10];
  char path[1000];
  char protocol[20];
  
  int bytes = recv(clientFD, buffer, sizeof(buffer) - 1, 0);
  if(bytes <= 0) {
    close(clientFD);
    return NULL;
  }

  buffer[bytes] = '\0';
  if(sscanf(buffer, "%s %s %s", method, path, protocol) != 3) {
    char response[] = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nRequisição não apresenta as informações necessárias no cabeçalho.";
    send(clientFD, response, strlen(response), 0);
    close(clientFD);
    return NULL;
  }

  if(strcmp(method, "GET") != 0) {
    char status[] = "HTTP/1.1 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nMétodo não permitido.";
    send(clientFD, status, strlen(status), 0);
    close(clientFD);
    return NULL;
  }
    
  char decodedPath[1000] = {0};
  decodeURL(decodedPath, path, strlen(path));
  char arquivoPath[2000];
  snprintf(arquivoPath, sizeof(arquivoPath), "%s%s", folderPath, decodedPath);
  char resolvedPath[2000];

  if(!realpath(arquivoPath, resolvedPath)) {
    char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
    send(clientFD, response, strlen(response), 0);
    close(clientFD);
    return NULL;
  }

  if(strncmp(resolvedPath, folderPath, strlen(folderPath)) != 0) {
    char response[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nAcesso não autorizado.";
    send(clientFD, response, strlen(response), 0);
    close(clientFD);
    return NULL;
  }

  struct stat pathStat;
  if(stat(resolvedPath, &pathStat) == -1) {
    char response[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/plain\r\n\r\nArquivo não existe ou não foi encontrado.";
    send(clientFD, response, strlen(response), 0);
    close(clientFD);
    return NULL;
  }
  
  if(S_ISDIR(pathStat.st_mode)) {
    char indexPath[2048];
    snprintf(indexPath, sizeof(indexPath), "%s/index.html", resolvedPath);

    if(access(indexPath, F_OK) == 0) {
      if(access(indexPath, R_OK) < 0) {
        char response[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nArquivo não autorizado.";
        send(clientFD, response, strlen(response), 0);
        close(clientFD);
        return NULL;
      } else {
        strcat(resolvedPath, "/index.html");
        stat(resolvedPath, &pathStat);
      }
    }
  }

  if(S_ISDIR(pathStat.st_mode)){
    DIR *dir = opendir(resolvedPath);
    if(!dir) {
      char response[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\n\r\nDiretório não autorizado.";
      send(clientFD, response, strlen(response), 0);
      close(clientFD);
      return NULL;
    }
    char html[16384];
    char css[] = "body{margin:10px;background-color:light-grey;font-family:Arial;}";
    snprintf(html, sizeof(html),
      "<html><head><title>Diretório</title><meta charset=\"utf-8\"><style> %s</style></head>"
      "<body><h2>Arquivos do diretório:</h2><ul>",
    css);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
      if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

      char item[2048];
      snprintf(item, sizeof(item), "%s%s%s",
        path, path[strlen(path) - 1] == '/' ? "" : "/", entry->d_name);
      strcat(html, "<li><a href=\"");
      strcat(html, item);
      if(entry->d_type == DT_DIR) strcat(html, "/");
      strcat(html, "\">");
      strcat(html, entry->d_name);
      if(entry->d_type == DT_DIR) strcat(html, "/");
      strcat(html, "</a></li>");
    }

    closedir(dir);
    strcat(html, "</ul></body></html>");
    char header[256];
    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %zu\r\n\r\n", strlen(html));
    sendData(clientFD, header, strlen(header));
    sendData(clientFD, html, strlen(html));
  } else {
      FILE *f = fopen(resolvedPath, "rb");
      if(!f) {
        char response[] = "HTTP/1.1 404 Not Found\r\n\r\n";
        sendData(clientFD, response, strlen(response));
        close(clientFD);
        return NULL;
      }
      char *ext = strrchr(resolvedPath, '.');
      const char *contentType = "application/octet-stream";
      if(ext) {
        if(strcmp(ext, ".html") == 0)
          contentType = "text/html";
        else if(strcmp(ext, ".css") == 0)
          contentType = "text/css";
        else if(strcmp(ext, ".js") == 0)
          contentType = "application/javascript";
        else if(strcmp(ext, ".pdf") == 0)
          contentType = "application/pdf";
        else if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
          contentType = "image/jpeg";
        else if(strcmp(ext, ".png") == 0)
          contentType = "image/png";
        else if(strcmp(ext, ".gif") == 0)
          contentType = "image/gif";
        else if(strcmp(ext, ".txt") == 0)
          contentType = "text/plain";
      }
    
      fseek(f, 0, SEEK_END);
      long fileSize = ftell(f);
      rewind(f);
    
      char header[1024];
      sprintf(header,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n\r\n",
      contentType, fileSize);
      sendData(clientFD, header, strlen(header));
      char fileBuffer[8000];
      size_t bytesRead;
      while ((bytesRead = fread(fileBuffer, 1, sizeof(fileBuffer), f)) > 0) {
        sendData(clientFD, fileBuffer, bytesRead);
      }
      fclose(f);
  }
  close(clientFD);
  return NULL;
}

int main(int argc, char *argv[]){
  if(argc < 2) {
    printf("Parâmetros incorretos, execute: ./server /minha-pasta\n");
    return 1;
  }

  realpath(argv[1], folderPath);
  printf("Servindo os arquivos a partir de: %s\n", folderPath);

  struct addrinfo hints, *res;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  int info = getaddrinfo(NULL, PORT, &hints, &res);
  if(info != 0) {
    printf("Ocorreu um erro. %s", gai_strerror(info));
    return 1;
  }

  int socketFD;
  struct addrinfo *aux = res;
  while(aux != NULL) {
    socketFD = socket(aux->ai_family, aux->ai_socktype, aux->ai_protocol);
    if(socketFD == -1) {
      perror("server: socket");
      aux = aux->ai_next;
      continue;
    }
    int opt = 1;
    setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int bindStatus = bind(socketFD, aux->ai_addr, aux->ai_addrlen);
    if(bindStatus == -1) {
      close(socketFD);
      perror("server: bind");
      aux = aux->ai_next;
      continue;
    }
    break;
  }

  if(aux == NULL) {
    printf("Não foi possível criar o socket.\n");
    return 1;
  }

  freeaddrinfo(res);

  if(listen(socketFD, 10) < 0) {
    close(socketFD);
    perror("listen");
    return 1;
  }

  printf("Servidor escutando a porta %s.\n", PORT);

  struct sockaddr_storage clientAddr;

  while(1) {
    int *clientFD = (int*)malloc(sizeof(int));
    socklen_t lenAddr = sizeof clientAddr;
    *clientFD = accept(socketFD, (struct sockaddr *)&clientAddr, &lenAddr);
    if(*clientFD < 0) {
      perror("accept");
      free(clientFD);
      continue;
    }

    pthread_t tid;
    if(pthread_create(&tid, NULL, handle_client, clientFD) != 0) {
      perror("pthread_create");
      close(*clientFD);
      free(clientFD);
      continue;
    }

    pthread_detach(tid);
  }

  close(socketFD);
  return 0;
}
