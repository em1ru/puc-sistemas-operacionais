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

void print_usage(char *prog) {
    fprintf(stderr, "Uso: %s <hostname> <port> <comando> [args...]\n", prog);
    fprintf(stderr, "\nComandos:\n");
    fprintf(stderr, "  read <owner> <arquivo> [offset]    - Ler 16 bytes de um arquivo\n");
    fprintf(stderr, "  write <owner> <arquivo> <dados> [offset] - Escrever dados em arquivo\n");
    fprintf(stderr, "  mkdir <owner> <path> <nome_dir>    - Criar diretório\n");
    fprintf(stderr, "  rmdir <owner> <path> <nome>        - Remover arquivo/diretório\n");
    fprintf(stderr, "  ls <owner> <path>                  - Listar diretório\n");
    fprintf(stderr, "\nExemplos:\n");
    fprintf(stderr, "  %s localhost 3999 read 1 teste.txt\n", prog);
    fprintf(stderr, "  %s localhost 3999 write 1 teste.txt \"Hello World!\" 0\n", prog);
    fprintf(stderr, "  %s localhost 3999 mkdir 1 \"\" subdir\n", prog);
    fprintf(stderr, "  %s localhost 3999 ls 1 \"\"\n", prog);
    exit(1);
}

int main(int argc, char **argv) {
    int sockfd, portno, n;
    socklen_t serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname;
    
    SFSMessage req, rep;

    if (argc < 5) {
        print_usage(argv[0]);
    }
    
    hostname = argv[1];
    portno = atoi(argv[2]);
    char *comando = argv[3];

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
    serverlen = sizeof(serveraddr);

    /* 3. Preparar a Requisição baseada no comando */
    bzero(&req, sizeof(SFSMessage));
    
    if (strcmp(comando, "read") == 0) {
        if (argc < 6) print_usage(argv[0]);
        req.type = REQ_READ;
        req.owner_id = atoi(argv[4]);
        strncpy(req.path, argv[5], MAX_PATH_LEN - 1);
        req.offset = (argc > 6) ? atoi(argv[6]) : 0;
        printf("[Cliente] READ: owner=A%d, path='%s', offset=%d\n", 
               req.owner_id, req.path, req.offset);
    }
    else if (strcmp(comando, "write") == 0) {
        if (argc < 7) print_usage(argv[0]);
        req.type = REQ_WRITE;
        req.owner_id = atoi(argv[4]);
        strncpy(req.path, argv[5], MAX_PATH_LEN - 1);
        strncpy((char*)req.data, argv[6], BLOCK_SIZE);
        req.offset = (argc > 7) ? atoi(argv[7]) : 0;
        printf("[Cliente] WRITE: owner=A%d, path='%s', dados='%.16s', offset=%d\n", 
               req.owner_id, req.path, req.data, req.offset);
    }
    else if (strcmp(comando, "mkdir") == 0) {
        if (argc < 7) print_usage(argv[0]);
        req.type = REQ_CREATE_DIR;
        req.owner_id = atoi(argv[4]);
        strncpy(req.path, argv[5], MAX_PATH_LEN - 1);
        strncpy(req.secondary_name, argv[6], MAX_NAME_LEN - 1);
        printf("[Cliente] MKDIR: owner=A%d, path='%s', nome='%s'\n", 
               req.owner_id, req.path, req.secondary_name);
    }
    else if (strcmp(comando, "rmdir") == 0 || strcmp(comando, "rm") == 0) {
        if (argc < 7) print_usage(argv[0]);
        req.type = REQ_REMOVE;
        req.owner_id = atoi(argv[4]);
        strncpy(req.path, argv[5], MAX_PATH_LEN - 1);
        strncpy(req.secondary_name, argv[6], MAX_NAME_LEN - 1);
        printf("[Cliente] REMOVE: owner=A%d, path='%s', nome='%s'\n", 
               req.owner_id, req.path, req.secondary_name);
    }
    else if (strcmp(comando, "ls") == 0) {
        if (argc < 5) print_usage(argv[0]);
        req.type = REQ_LISTDIR;
        req.owner_id = atoi(argv[4]);
        strncpy(req.path, (argc > 5) ? argv[5] : "", MAX_PATH_LEN - 1);
        printf("[Cliente] LISTDIR: owner=A%d, path='%s'\n", 
               req.owner_id, req.path);
    }
    else {
        fprintf(stderr, "Comando desconhecido: %s\n", comando);
        print_usage(argv[0]);
    }

    /* 4. Enviar Requisição */
    n = sendto(sockfd, &req, sizeof(SFSMessage), 0, 
               (struct sockaddr *) &serveraddr, serverlen);
    if (n < 0) 
        error("ERROR in sendto");
    
    /* 5. Aguardar Resposta */
    printf("[Cliente] Aguardando resposta...\n");
    
    bzero(&rep, sizeof(SFSMessage));
    n = recvfrom(sockfd, &rep, sizeof(SFSMessage), 0, 
                 (struct sockaddr *) &serveraddr, &serverlen);
    if (n < 0) 
        error("ERROR in recvfrom");

    /* 6. Exibir Resultado */
    printf("\n========== RESPOSTA ==========\n");
    
    switch (rep.type) {
        case REP_READ:
            if (rep.status > 0) {
                printf("Status: SUCESSO (Bytes lidos: %d)\n", rep.status);
                printf("Conteúdo (Hex): ");
                for(int i = 0; i < BLOCK_SIZE; i++) printf("%02X ", rep.data[i]);
                printf("\nConteúdo (Txt): '%.16s'\n", rep.data);
            } else if (rep.status == 0) {
                printf("Status: EOF (Fim de Arquivo)\n");
            } else {
                printf("Status: ERRO (código %d)\n", rep.status);
            }
            break;
            
        case REP_WRITE:
            if (rep.status >= 0) {
                printf("Status: SUCESSO (offset escrito: %d)\n", rep.status);
            } else {
                printf("Status: ERRO (código %d)\n", rep.status);
            }
            break;
            
        case REP_CREATE_DIR:
            if (rep.status > 0) {
                printf("Status: SUCESSO\n");
                printf("Novo path: '%s' (len=%d)\n", rep.path, rep.status);
            } else {
                printf("Status: ERRO (código %d)\n", rep.status);
                if (rep.status == -2) printf("  -> Diretório já existe\n");
            }
            break;
            
        case REP_REMOVE:
            if (rep.status >= 0) {
                printf("Status: SUCESSO\n");
            } else {
                printf("Status: ERRO (código %d)\n", rep.status);
                if (rep.status == -3) printf("  -> Diretório não está vazio\n");
            }
            break;
            
        case REP_LISTDIR:
            if (rep.nrnames >= 0) {
                printf("Status: SUCESSO (%d entradas)\n", rep.nrnames);
                printf("Conteúdo do diretório:\n");
                for (int i = 0; i < rep.nrnames; i++) {
                    int start = rep.fstlstpositions[i].start_pos;
                    int is_dir = rep.fstlstpositions[i].is_dir;
                    char *name = &rep.allfilenames[start];
                    printf("  %s %s\n", is_dir ? "[DIR]" : "[ARQ]", name);
                }
            } else {
                printf("Status: ERRO (código %d)\n", rep.nrnames);
            }
            break;
            
        default:
            printf("Tipo de resposta inesperado: %d\n", rep.type);
    }
    
    printf("==============================\n");

    close(sockfd);
    return 0;
}