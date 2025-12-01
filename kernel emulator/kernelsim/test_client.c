#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h> 
#include <arpa/inet.h>

#include "protocol.h"

void error(char *msg) {
    perror(msg);
    exit(0);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    
    SFSMessage req, rep;

    if (argc != 4) {
       fprintf(stderr,"Uso: %s <hostname> <port> <arquivo_para_ler>\n", argv[0]);
       exit(0);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    char *filename = argv[3];

    /* 1. Criar Socket */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) 
        error("ERROR opening socket");

    /* 2. Configurar destino (Servidor) */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(0);
    }

    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, 
          (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* 3. Preparar a Requisição (Protocolo SFP) */
    bzero(&req, sizeof(SFSMessage));
    req.type = REQ_READ;
    req.owner_id = 1;        // Simulando ser o Processo A1
    req.offset = 0;          // Ler do início
    strncpy(req.path, filename, MAX_PATH_LEN - 1); // Nome do arquivo

    printf("Cliente: Solicitando leitura de '%s' (Owner A1, Offset 0)\n", filename);

    /* 4. Enviar Requisição */
    serverlen = sizeof(serveraddr);
    n = sendto(sockfd, &req, sizeof(SFSMessage), 0, (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
      error("ERROR in sendto");
    
    /* 5. Aguardar Resposta (Bloqueante) */
    printf("Cliente: Aguardando resposta...\n");
    
    bzero(&rep, sizeof(SFSMessage));
    n = recvfrom(sockfd, &rep, sizeof(SFSMessage), 0, (struct sockaddr *) &serveraddr, &serverlen);
    if (n < 0) 
      error("ERROR in recvfrom");

    /* 6. Exibir Resultado */
    if (rep.type == REP_READ) {
        if (rep.status > 0) {
            printf("\n--- SUCESSO ---\n");
            printf("Bytes lidos: %d\n", rep.status);
            printf("Conteúdo (Hex): ");
            for(int i=0; i<16; i++) printf("%02X ", rep.data[i]);
            printf("\nConteúdo (Txt): '%.16s'\n", rep.data);
            printf("-----------------\n");
        } else if (rep.status == 0) {
            printf("\n--- EOF (Fim de Arquivo) ---\n");
        } else {
            printf("\n--- ERRO NO SERVIDOR ---\n");
            printf("Código de erro: %d\n", rep.status);
        }
    } else {
        printf("Erro: Recebido tipo de mensagem inesperado (%d)\n", rep.type);
    }

    close(sockfd);
    return 0;
}