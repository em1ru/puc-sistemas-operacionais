# Makefile para KernelSim
# Trabalho de Sistemas Operacionais

CC = gcc
CFLAGS = -Wall -Wextra -std=gnu99 -D_DEFAULT_SOURCE
TARGET = kernelsim
SRC = kernel.c

# Compilação padrão
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	@echo ""
	@echo "✓ Compilação concluída com sucesso!"
	@echo "  Execute com: ./$(TARGET)"
	@echo ""

# Compilação com otimização
release: CFLAGS += -O2
release: clean $(TARGET)

# Compilação para debug (com símbolos)
debug: CFLAGS += -g -DDEBUG=1
debug: clean $(TARGET)

# Limpeza
clean:
	rm -f $(TARGET)
	@echo "✓ Arquivos de compilação removidos"

# Execução
run: $(TARGET)
	./$(TARGET)

# Verificação de vazamentos de memória (requer valgrind)
valgrind: debug
	valgrind --leak-check=full --show-leak-kinds=all ./$(TARGET)

# Ajuda
help:
	@echo "Makefile para KernelSim"
	@echo ""
	@echo "Alvos disponíveis:"
	@echo "  make          - Compila o simulador (padrão)"
	@echo "  make release  - Compila com otimizações"
	@echo "  make debug    - Compila com símbolos de debug"
	@echo "  make clean    - Remove arquivos compilados"
	@echo "  make run      - Compila e executa"
	@echo "  make valgrind - Executa com valgrind (debug de memória)"
	@echo "  make help     - Exibe esta ajuda"
	@echo ""

.PHONY: all release debug clean run valgrind help
