CC = gcc
CFLAGS = -Wall -Wextra
PORT = 3999

# Alvos principais
all: sfss kernel

# Compilação do Servidor
sfss: sfss.c protocol.h
	$(CC) $(CFLAGS) sfss.c -o sfss

# Compilação do Kernel
kernel: kernel.c protocol.h
	$(CC) $(CFLAGS) kernel.c -o kernel

# --- ALVO MÁGICO PARA RODAR TUDO ---
# 1. Compila tudo (all)
# 2. Inicia o sfss em background (&) e salva o PID dele num arquivo
# 3. Redireciona a saída do sfss para sfss.log (para não poluir a tela do kernel)
# 4. Espera 1 segundo para garantir que o socket abriu
# 5. Roda o kernel
# 6. Ao final, mata o processo do servidor usando o PID salvo
run: all
	@echo "--- [Makefile] Iniciando Servidor SFSS na porta $(PORT) ---"
	@./sfss $(PORT) > sfss.log 2>&1 & echo $$! > sfss.pid
	@sleep 1
	@echo "--- [Makefile] Servidor rodando (PID salvo). Logs em 'sfss.log' ---"
	@echo "--- [Makefile] Iniciando KernelSim agora... ---"
	@echo ""
	@./kernel || (echo "Kernel falhou"; kill `cat sfss.pid`; rm sfss.pid; exit 1)
	@echo ""
	@echo "--- [Makefile] Simulação finalizada. Encerrando Servidor... ---"
	@kill `cat sfss.pid` && rm sfss.pid
	@echo "--- [Makefile] Tudo limpo. Confira 'sfss.log' para ver o que o servidor fez. ---"

# Limpeza dos binários e logs
clean:
	rm -f sfss kernel sfss.log sfss.pid