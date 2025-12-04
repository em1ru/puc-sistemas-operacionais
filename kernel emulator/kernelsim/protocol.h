#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

/* ========== CONSTANTES E LIMITES ========== */

// Tamanho do bloco de leitura/escrita definido no enunciado 
#define BLOCK_SIZE 16

// Limite máximo para caminhos e nomes de arquivos
// O UDP suporta até ~65KB, mas vamos manter razoável para a simulação
#define MAX_PATH_LEN 256
#define MAX_NAME_LEN 64

// Limite de entradas em um diretório para o comando ListDir [cite: 38]
#define MAX_DIR_ENTRIES 40

// Tamanho do buffer para retornar todos os nomes de arquivos no ListDir
// Estimativa: 40 arquivos * 64 chars = 2560 bytes (tranquilo para UDP)
#define DIR_BUFFER_SIZE 4096

#define IS_FILE 0 
#define IS_DIR  1 

/* ========== TIPOS DE MENSAGEM (OPERAÇÕES) ========== */
// Define o que o servidor deve fazer ou o que ele está respondendo
typedef enum {
    REQ_READ,       // Solicita leitura de arquivo [cite: 38]
    REP_READ,       // Resposta da leitura [cite: 38]
    
    REQ_WRITE,      // Solicita escrita em arquivo [cite: 38]
    REP_WRITE,      // Resposta da escrita [cite: 124]
    
    REQ_CREATE_DIR, // Solicita criação de subdiretório (add) [cite: 38]
    REP_CREATE_DIR, // Resposta da criação [cite: 133]
    
    REQ_REMOVE,     // Solicita remoção de arquivo/dir (rem) [cite: 38]
    REP_REMOVE,     // Resposta da remoção [cite: 139]
    
    REQ_LISTDIR,    // Solicita lista de diretório [cite: 38]
    REP_LISTDIR,    // Resposta da lista (estrutura complexa) [cite: 141]
    
    REP_ERROR       // Tipo genérico para erros graves
} MessageType;

/* ========== ESTRUTURAS AUXILIARES ========== */

// Estrutura para indexar os nomes no buffer do ListDir [cite: 141]
// O PDF especifica struct {int, int, int} para posição inicial, final e tipo
typedef struct {
    int start_pos;  // Índice inicial no buffer allfilenames
    int end_pos;    // Índice final no buffer allfilenames
    int is_dir;     // 1 se for diretório (IS_DIR), 0 se for arquivo (IS_FILE)
} DirEntryPosition;

/* ========== ESTRUTURA PRINCIPAL DA MENSAGEM (SFP) ========== */

// Esta é a struct que será enviada via sendto e recebida via recvfrom.
// Ela é uma "união" lógica de todos os campos necessários para todas as operações.
typedef struct {
    // Cabeçalho Comum
    MessageType type;           // Tipo da mensagem (REQ_xx ou REP_xx)
    int owner_id;               // ID do processo (1..5 para A1..A5) [cite: 119]
    int status;                 // Usado nas RESPOSTAS. Se < 0, é erro. Se >= 0, é sucesso/strlen [cite: 120]
    
    // Campos de Identificação
    char path[MAX_PATH_LEN];    // Caminho completo do arquivo/diretório alvo [cite: 119]
    
    // Campos para Read/Write
    int offset;                 // Posição para leitura/escrita (0, 16, 32...) [cite: 38]
    unsigned char data[BLOCK_SIZE]; // Payload de 16 bytes para RW [cite: 30, 119]
    
    // Campos para Manipulação de Diretório (Create/Remove)
    char secondary_name[MAX_NAME_LEN]; // Para 'add' e 'rem', o nome do novo dir ou do alvo a remover [cite: 38]

    // Campos Específicos para Resposta de ListDir (DL-REP) [cite: 141]
    int nrnames;                // Número de arquivos/dirs retornados
    DirEntryPosition fstlstpositions[MAX_DIR_ENTRIES]; // Metadados dos nomes
    char allfilenames[DIR_BUFFER_SIZE]; // String única contendo todos os nomes concatenados
    
} SFSMessage;

// Traduz o tipo de mensagem do protocolo para texto legível
const char* get_op_name(int type);


#endif // PROTOCOL_H