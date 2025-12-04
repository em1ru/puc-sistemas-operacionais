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
#include <dirent.h>

#include "protocol.h" // Inclui nossa definição de SFSMessage

#define BUFSIZE 4096
#define ROOT_DIR "./fs_root" // Pasta raiz do servidor

/* Função auxiliar para erros */
void error(char *msg) {
    perror(msg);
    exit(1);
}

/* Função auxiliar para construir o caminho completo */
void build_full_path(char *full_path, size_t size, int owner_id, const char *path) {
    const char *clean_path = path;
    if (clean_path[0] == '/') clean_path++; // Remove '/' inicial se houver
    snprintf(full_path, size, "%s/A%d/%s", ROOT_DIR, owner_id, clean_path);
}

/* ========== HANDLE READ ========== */
/* Lê 16 bytes do arquivo solicitado e preenche a mensagem de resposta. */
void handle_read(SFSMessage *msg) {
    char full_path[512];
    FILE *fp;
    
    build_full_path(full_path, sizeof(full_path), msg->owner_id, msg->path);
    printf("[SFSS] READ: %s (Offset: %d)\n", full_path, msg->offset);

    fp = fopen(full_path, "rb");
    msg->type = REP_READ;
    
    if (fp == NULL) {
        // Alterado para mostrar o nome do arquivo (full_path)
        printf("[SFSS] Erro ao abrir arquivo '%s': %s\n", full_path, strerror(errno)); 
        msg->status = -1;
        memset(msg->data, 0, BLOCK_SIZE);
    } else {
        if (fseek(fp, msg->offset, SEEK_SET) != 0) {
            msg->status = -2; // Erro de seek
            memset(msg->data, 0, BLOCK_SIZE);
        } else {
            size_t bytes_read = fread(msg->data, 1, BLOCK_SIZE, fp);
            msg->status = (bytes_read > 0) ? (int)bytes_read : 0;
        }
        fclose(fp);
    }
}

/* ========== HANDLE WRITE ========== */
/* Escreve 16 bytes no arquivo. Cria o arquivo se não existir.
 * Se offset > tamanho atual, preenche com espaços (0x20).
 * Se payload vazio e offset=0, remove o arquivo. */
void handle_write(SFSMessage *msg) {
    char full_path[512];
    FILE *fp;
    
    build_full_path(full_path, sizeof(full_path), msg->owner_id, msg->path);
    printf("[SFSS] WRITE: %s (Offset: %d)\n", full_path, msg->offset);

    msg->type = REP_WRITE;

    // Caso especial: payload vazio e offset=0 significa REMOVER arquivo
    int is_empty_payload = 1;
    for (int i = 0; i < BLOCK_SIZE; i++) {
        if (msg->data[i] != 0) {
            is_empty_payload = 0;
            break;
        }
    }
    
    if (is_empty_payload && msg->offset == 0) {
        // Remove o arquivo
        if (remove(full_path) == 0) {
            printf("[SFSS] Arquivo removido: %s\n", full_path);
            msg->status = 0; // Sucesso
        } else {
            printf("[SFSS] Erro ao remover arquivo '%s': %s\n", full_path, strerror(errno));
            msg->status = -1;
        }
        return;
    }

    // Abre para leitura+escrita, cria se não existir
    fp = fopen(full_path, "r+b");
    if (fp == NULL) {
        // Arquivo não existe, tenta criar
        fp = fopen(full_path, "w+b");
    }
    
    if (fp == NULL) {
        printf("[SFSS] Erro ao abrir/criar arquivo '%s': %s\n", full_path, strerror(errno));
        msg->status = -1;
        return;
    }

    // Verifica tamanho atual do arquivo
    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);

    // Se offset > tamanho atual, preenche com espaços (0x20)
    if (msg->offset > file_size) {
        fseek(fp, file_size, SEEK_SET);
        long gap = msg->offset - file_size;
        for (long i = 0; i < gap; i++) {
            fputc(0x20, fp); // Whitespace character
        }
    }

    // Posiciona e escreve os 16 bytes
    fseek(fp, msg->offset, SEEK_SET);
    size_t written = fwrite(msg->data, 1, BLOCK_SIZE, fp);
    
    if (written == BLOCK_SIZE) {
        msg->status = msg->offset; // Sucesso: retorna o offset
        printf("[SFSS] Escrita OK: %zu bytes no offset %d\n", written, msg->offset);
    } else {
        msg->status = -3; // Erro de escrita
        printf("[SFSS] Erro na escrita: apenas %zu bytes\n", written);
    }
    
    fclose(fp);
}

/* ========== HANDLE CREATE DIR ========== */
/* Cria um novo subdiretório dentro do path especificado. */
void handle_create_dir(SFSMessage *msg) {
    char full_path[512];
    char new_dir_path[512];
    
    build_full_path(full_path, sizeof(full_path), msg->owner_id, msg->path);
    
    // Constrói o caminho do novo diretório: path + "/" + secondary_name
    snprintf(new_dir_path, sizeof(new_dir_path), "%s/%s", full_path, msg->secondary_name);
    
    printf("[SFSS] CREATE_DIR: %s\n", new_dir_path);
    
    msg->type = REP_CREATE_DIR;
    
    if (mkdir(new_dir_path, 0777) == 0) {
        // Sucesso: atualiza o path na resposta para incluir o novo diretório
        char *clean_path = msg->path;
        if (clean_path[0] == '/') clean_path++;
        
        if (strlen(clean_path) > 0) {
            snprintf(msg->path, MAX_PATH_LEN, "%s/%s", clean_path, msg->secondary_name);
        } else {
            snprintf(msg->path, MAX_PATH_LEN, "%s", msg->secondary_name);
        }
        msg->status = strlen(msg->path);
        printf("[SFSS] Diretório criado: %s\n", new_dir_path);
    } else {
        if (errno == EEXIST) {
            printf("[SFSS] Diretório já existe: %s\n", new_dir_path);
            msg->status = -2; // Já existe
        } else {
            printf("[SFSS] Erro ao criar diretório '%s': %s\n", new_dir_path, strerror(errno));
            msg->status = -1; // Erro genérico
        }
    }
}

/* ========== HANDLE REMOVE ========== */
/* Remove um arquivo ou diretório do path especificado. */
void handle_remove(SFSMessage *msg) {
    char full_path[512];
    char target_path[512];
    struct stat st;
    
    build_full_path(full_path, sizeof(full_path), msg->owner_id, msg->path);
    
    // Constrói o caminho do alvo: path + "/" + secondary_name
    snprintf(target_path, sizeof(target_path), "%s/%s", full_path, msg->secondary_name);
    
    printf("[SFSS] REMOVE: %s\n", target_path);
    
    msg->type = REP_REMOVE;
    
    // Verifica se é arquivo ou diretório
    if (stat(target_path, &st) != 0) {
        printf("[SFSS] Alvo não encontrado '%s': %s\n", target_path, strerror(errno));
        msg->status = -1; // Não existe
        return;
    }
    
    int result;
    if (S_ISDIR(st.st_mode)) {
        // É diretório: usa rmdir (só funciona se estiver vazio)
        result = rmdir(target_path);
    } else {
        // É arquivo: usa remove
        result = remove(target_path);
    }
    
    if (result == 0) {
        // Sucesso: retorna o path atualizado (sem o item removido)
        msg->status = strlen(msg->path);
        printf("[SFSS] Removido com sucesso: %s\n", target_path);
    } else {
        if (errno == ENOTEMPTY) {
            printf("[SFSS] Diretório não está vazio: %s\n", target_path);
            msg->status = -3; // Diretório não vazio
        } else {
            printf("[SFSS] Erro ao remover: %s\n", strerror(errno));
            msg->status = -1;
        }
    }
}

/* ========== HANDLE LISTDIR ========== */
/* Lista todos os arquivos e subdiretórios de um diretório.
 * Retorna os nomes concatenados em allfilenames e as posições em fstlstpositions. */
void handle_listdir(SFSMessage *msg) {
    char full_path[512];
    DIR *dir;
    struct dirent *entry;
    struct stat st;
    char entry_path[512];
    
    build_full_path(full_path, sizeof(full_path), msg->owner_id, msg->path);
    
    printf("[SFSS] LISTDIR: %s\n", full_path);
    
    msg->type = REP_LISTDIR;
    
    dir = opendir(full_path);
    if (dir == NULL) {
        printf("[SFSS] Erro ao abrir diretório '%s': %s\n", full_path, strerror(errno));
        msg->status = -1;
        msg->nrnames = -1;
        return;
    }
    
    // Inicializa contadores
    int pos = 0;  // Posição atual no buffer allfilenames
    int count = 0; // Número de entradas
    
    memset(msg->allfilenames, 0, DIR_BUFFER_SIZE);
    memset(msg->fstlstpositions, 0, sizeof(msg->fstlstpositions));
    
    while ((entry = readdir(dir)) != NULL && count < MAX_DIR_ENTRIES) {
        // Ignora "." e ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        int name_len = strlen(entry->d_name);
        
        // Verifica se cabe no buffer
        if (pos + name_len >= DIR_BUFFER_SIZE) {
            printf("[SFSS] Buffer cheio, truncando listagem\n");
            break;
        }
        
        // Copia o nome para o buffer
        strcpy(&msg->allfilenames[pos], entry->d_name);
        
        // Preenche as posições
        msg->fstlstpositions[count].start_pos = pos;
        msg->fstlstpositions[count].end_pos = pos + name_len - 1;
        
        // Verifica se é diretório ou arquivo
        snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
        if (stat(entry_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            msg->fstlstpositions[count].is_dir = IS_DIR;
        } else {
            msg->fstlstpositions[count].is_dir = IS_FILE;
        }
        
        pos += name_len + 1; // +1 para o null terminator entre nomes
        count++;
    }
    
    closedir(dir);
    
    msg->nrnames = count;
    msg->status = count; // Sucesso: retorna número de entradas
    
    printf("[SFSS] Listagem: %d entradas encontradas\n", count);
}

int main(int argc, char **argv) {
    /* --- CORREÇÃO DE LOG: Desativa buffer para garantir escrita imediata no arquivo --- */
    setvbuf(stdout, NULL, _IONBF, 0);

    int sockfd; /* socket */
    int portno; /* porta para escutar */
    socklen_t clientlen; /* tamanho do endereço do cliente */
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
                handle_write(&msg);
                break;
                
            case REQ_CREATE_DIR:
                handle_create_dir(&msg);
                break;
                
            case REQ_REMOVE:
                handle_remove(&msg);
                break;
                
            case REQ_LISTDIR:
                handle_listdir(&msg);
                break;
            
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