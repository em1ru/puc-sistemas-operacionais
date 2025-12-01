#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <errno.h>

#include "protocol.h" // Inclui nossa definição de SFSMessage

#define BUFSIZE 4096
#define ROOT_DIR "./fs_root" // Pasta raiz do servidor

/* Função auxiliar para erros */
void error(char *msg) {
    perror(msg);
    exit(1);
}

/* * Função que processa o pedido de LEITURA (READ)
 * Lê 16 bytes do arquivo solicitado e preenche a mensagem de resposta.
 */
void handle_read(SFSMessage *msg) {
    char full_path[512];
    FILE *fp;
    
    // 1. Constrói o caminho real no sistema de arquivos do Linux
    // O PDF diz que cada processo Ax tem seu home: /A1, /A2, etc.
    // Estrutura física será: ./fs_root/A1/arquivo.txt
    // msg->path já deve vir com o caminho relativo, ex: "subdir/arquivo.txt"
    // O PDF sugere que o owner define o diretório raiz do usuário.
    
    // Formato: ./fs_root/A{owner_id}/{path}
    // Obs: msg->path deve ser limpo de barras iniciais para evitar sair da raiz
    char *clean_path = msg->path;
    if (clean_path[0] == '/') clean_path++; // Remove '/' inicial se houver

    // Nota: O enunciado menciona /A0 como compartilhado.
    // Se owner for 0, acessa A0. Se for 1..5, acessa A1..A5.
    snprintf(full_path, sizeof(full_path), "%s/A%d/%s", ROOT_DIR, msg->owner_id, clean_path);

    printf("[SFSS] Lendo arquivo: %s (Offset: %d)\n", full_path, msg->offset);

    // 2. Tenta abrir o arquivo para leitura binária
    fp = fopen(full_path, "rb");
    
    // Prepara a resposta (copiando dados básicos da requisição)
    msg->type = REP_READ;
    // O owner e path já estão na struct, mantemos igual
    
    if (fp == NULL) {
        // Erro ao abrir (arquivo não existe, permissão, etc)
        printf("[SFSS] Erro ao abrir arquivo: %s\n", strerror(errno));
        msg->status = -1; // Código de erro genérico (poderia ser -errno)
        msg->nrnames = 0; // Zera campos não usados
        memset(msg->data, 0, BLOCK_SIZE);
    } else {
        // 3. Pula para o offset desejado
        if (fseek(fp, msg->offset, SEEK_SET) != 0) {
            // Se o offset for além do tamanho do arquivo
            msg->status = -2; // Erro de seek
            memset(msg->data, 0, BLOCK_SIZE);
        } else {
            // 4. Lê os 16 bytes (BLOCK_SIZE)
            size_t bytes_read = fread(msg->data, 1, BLOCK_SIZE, fp);
            
            if (bytes_read > 0) {
                msg->status = bytes_read; // Sucesso: retorna quantos bytes leu
            } else {
                // Chegou no fim do arquivo (EOF) ou erro de leitura
                msg->status = 0; // 0 bytes lidos (EOF)
            }
        }
        fclose(fp);
    }
}

int main(int argc, char **argv) {
    int sockfd; /* socket */
    int portno; /* porta para escutar */
    int clientlen; /* tamanho do endereço do cliente */
    struct sockaddr_in serveraddr; /* endereço do servidor */
    struct sockaddr_in clientaddr; /* endereço do cliente */
    struct hostent *hostp; /* info do host cliente */
    char *hostaddrp; /* string ip do cliente */
    int optval; /* flag para setsockopt */
    int n; /* bytes recebidos */
    
    SFSMessage msg; // Nossa struct única do protocolo

    /* Checa argumentos: ./sfss <porta> */
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        exit(1);
    }
    portno = atoi(argv[1]);

    /* Cria o diretório raiz se não existir */
    // mkdir retorna -1 se já existe, o que é ok. 0777 dá permissão total.
    if (mkdir(ROOT_DIR, 0777) == -1 && errno != EEXIST) {
        error("ERRO criando diretório raiz fs_root");
    }
    // Cria subdiretórios para A0..A5 para teste inicial
    char subdir[128];
    for(int i=0; i<=5; i++) {
        sprintf(subdir, "%s/A%d", ROOT_DIR, i);
        mkdir(subdir, 0777);
    }

    /* 1. Criar o socket UDP */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
        error("ERRO abrindo socket");

    /* Configura socket para reutilizar endereço (evita erro "Address already in use") */
    optval = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

    /* 2. Configurar endereço do servidor */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); // Aceita qualquer IP
    serveraddr.sin_port = htons((unsigned short)portno);

    /* 3. Bind (Associar socket à porta) */
    if (bind(sockfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
        error("ERRO no bind");

    printf("[SFSS] Servidor de Arquivos iniciado na porta %d\n", portno);
    printf("[SFSS] Raiz do sistema de arquivos: %s\n", ROOT_DIR);

    clientlen = sizeof(clientaddr);
    
    /* 4. Loop Principal */
    while (1) {
        // Zera a mensagem antes de receber
        bzero(&msg, sizeof(SFSMessage));

        // Recebe datagrama (Bloqueante)
        n = recvfrom(sockfd, &msg, sizeof(SFSMessage), 0,
                     (struct sockaddr *) &clientaddr, &clientlen);
        if (n < 0)
            error("ERRO no recvfrom");

        /* Log de quem mandou */
        hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                              sizeof(clientaddr.sin_addr.s_addr), AF_INET);
        hostaddrp = inet_ntoa(clientaddr.sin_addr);
        if (hostp == NULL)
            printf("[SFSS] Requisição de: %s\n", hostaddrp);
        else
            printf("[SFSS] Requisição de: %s (%s)\n", hostp->h_name, hostaddrp);

        /* Processa a mensagem baseado no TYPE */
        switch (msg.type) {
            case REQ_READ:
                handle_read(&msg);
                break;
                
            case REQ_WRITE:
                printf("[SFSS] WRITE solicitado (Ainda não implementado)\n");
                msg.type = REP_WRITE;
                msg.status = -1; // Not implemented
                break;
                
            // Outros cases (REQ_CREATE_DIR, REQ_LISTDIR...) virão aqui
            
            default:
                printf("[SFSS] Tipo de mensagem desconhecido: %d\n", msg.type);
                msg.type = REP_ERROR;
                msg.status = -1;
        }

        /* 5. Envia a resposta de volta */
        n = sendto(sockfd, &msg, sizeof(SFSMessage), 0,
                   (struct sockaddr *) &clientaddr, clientlen);
        if (n < 0)
            error("ERRO no sendto");
    }
}