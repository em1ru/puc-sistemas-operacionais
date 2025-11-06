#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // sleep()

#define INDICE_PRODUTOR         0
#define INDICE_CONSUMIDOR       1
#define TAMANHO_FILA            8
#define MARCA_POSICAO_LIVRE     -1
#define ERRO                    0
#define SUCESSO                 1
#define ERRO_REMOCAO            -1
#define MAX_ITERACOES           64
#define DELAY_CONSUMIDOR        2
#define DELAY_PRODUTOR          1

#define NUM_PRODUTORES          2  
#define NUM_CONSUMIDORES        3

typedef struct {
    int *p_in;          // referente à posição do próximo a entrar na fila
    int *p_out;         // referente à posição do próximo a sair da fila 
    int v[TAMANHO_FILA]; 
} Fila;

typedef struct {
    int id;       // ID da thread (0, 1, 2...)
    Fila *fila;   // Ponteiro para a fila compartilhada
} ThreadArgs;

pthread_mutex_t lock;
pthread_cond_t cond_full;  // Buffer está cheio?
pthread_cond_t cond_empty; // Buffer está vazio?
Fila *fila;

Fila* init_fila(void) {
    Fila *f = (Fila*)malloc(sizeof(Fila));    
    // Fazemos os ponteiros p_in e p_out apontarem para o início do vetor v.
    f->p_in = f->v;
    f->p_out = f->v;

    // Marcando posições livres (lembrete: fila circular)
    for (int i = 0; i < TAMANHO_FILA; i++) {
        f->v[i] = MARCA_POSICAO_LIVRE;
    }

    // Retorna fila criada
    return f; 
}

int is_fila_cheia(Fila* f) {
    return *(f->p_in) != MARCA_POSICAO_LIVRE;
}

int is_fila_vazia(Fila* f) {
    return *(f->p_out) == MARCA_POSICAO_LIVRE;
}

int add_fila(Fila *f, int val) {
    // verifica se próxima posição está livre 
    if (!is_fila_cheia(f)) {
        *(f->p_in) = val;
        f->p_in = f->v + ((int)(f->p_in - f->v) + 1) % TAMANHO_FILA; // fila circular
        return SUCESSO;
    }

    return ERRO;
}

int remove_fila(Fila *f) {
    // verifica se fila não é vazia
    int ret = ERRO_REMOCAO;

    if (!is_fila_vazia(f)) {
        ret = *(f->p_out);
        *(f->p_out) = -1; // posição livre
        f->p_out = f->v + ((int)(f->p_out - f->v) + 1) % TAMANHO_FILA; // fila circular
    }

    return ret;
}

void *produzir(void* arg) {
    // desempacotar argumentos
    ThreadArgs *args = (ThreadArgs*) arg;
    Fila* f = args->fila;
    int id = args->id;

    for (int i = 0; i < MAX_ITERACOES; i++) {
        sleep(DELAY_PRODUTOR);
        int random = 1 + rand() % 64; // gerando números de 1 a 64

        // manipulação da fila deve ser travada pelo mutex para evitar condição de corrida
        pthread_mutex_lock(&lock);
        
        // Espera (liberando o mutex) se o buffer estiver cheio
        while (is_fila_cheia(f)) {
            fprintf(stderr, "[PRODUTOR %d] \t Buffer cheio. Esperando...\n", id);
            pthread_cond_wait(&cond_full, &lock);
        }
        
        // Seção crítica adicionando elemento
        if (add_fila(f, random) != SUCESSO) {
            fprintf(stderr, "[PRODUTOR %d] \t Erro catastrófico, abortar!!\n", id);
            exit(1);
        }
        else {
            int indice_escrito = ((int)(f->p_in - f->v) - 1 + TAMANHO_FILA) % TAMANHO_FILA;
            fprintf(stderr, "[PRODUTOR %d] \t Adicionei %d em fila->v[%d]\n", id, random, indice_escrito);
        }

        // Sinaliza ao consumidor que o buffer não está mais vazio, depois libera o mutex
        pthread_cond_broadcast(&cond_empty);
        pthread_mutex_unlock(&lock);
    }
}

void *consumir(void* arg) {
    // desempacotar os argumentos
    ThreadArgs *args = (ThreadArgs*) arg;
    Fila* f = args->fila;
    int id = args->id;
    
    int val_lido;
    for (int i = 0; i < MAX_ITERACOES; i++) {
        sleep(DELAY_CONSUMIDOR);

        // manipulação da fila deve ser travada pelo mutex para evitar condição de corrida
        pthread_mutex_lock(&lock);

        // Espera (liberando o mutex) se o buffer estiver vazio
        while (is_fila_vazia(f)) {
            fprintf(stderr, "[CONSUMIDOR %d] \t Buffer vazio. Esperando...\n", id);
            pthread_cond_wait(&cond_empty, &lock);
        }
        
        val_lido = remove_fila(f);
        // Seção crítica consumindo elemento
        if (val_lido == ERRO_REMOCAO) {
            fprintf(stderr, "[CONSUMIDOR %d] \t Erro catastrófico, abortar!!\n", id);
            exit(1);
        }
        else {
            int indice_lido = ((int)(f->p_out - f->v) - 1 + TAMANHO_FILA) % TAMANHO_FILA;
            fprintf(stderr, "[CONSUMIDOR %d] \t Li %d em fila->v[%d]\n", id, val_lido, indice_lido);
        }

        // Sinaliza ao consumidor que o buffer não está mais vazio, depois libera o mutex
        pthread_cond_broadcast(&cond_full);
        pthread_mutex_unlock(&lock);
    }
}


int main(void) {
    fila = init_fila();
    int total_threads = NUM_PRODUTORES + NUM_CONSUMIDORES;
    pthread_t threads[total_threads];

    // array de structs de argumentos, um para cada thread, para que cada um tenha seu ID.
    ThreadArgs args[total_threads];

    // Inicializa mutex e variáveis de condição
    pthread_mutex_init(&lock, NULL); 
    pthread_cond_init(&cond_full, NULL); 
    pthread_cond_init(&cond_empty, NULL); 

    fprintf(stderr, "Criando %d produtores e %d consumidores...\n", NUM_PRODUTORES, NUM_CONSUMIDORES);

    int i;
    // Criando Produtores 
    for(i = 0; i < NUM_PRODUTORES; i++) {
        args[i].id = i;
        args[i].fila = fila;
        pthread_create(&threads[i], NULL, produzir, (void*) &args[i]); // Passa o endereço do arg
    }

    // Criando Consumidores 
    // (Note que 'i' continua de onde parou)
    for(; i < total_threads; i++) {
        args[i].id = i - NUM_PRODUTORES; // IDs dos consumidores recomeçam de 0
        args[i].fila = fila;
        pthread_create(&threads[i], NULL, consumir, (void*) &args[i]); // Passa o endereço do arg
    }
    
    // Esperando todas as threads
    for(i = 0; i < total_threads; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destrói
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond_full);
    pthread_cond_destroy(&cond_empty);
    
    fprintf(stderr, "Trabalho concluído!!\n");

    return 0;
}