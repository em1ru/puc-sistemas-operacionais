# Simulador de SO Preemptivo

Projeto de Sistemas Operacionais - implementação de um simulador de SO com escalonamento Round Robin.

## Como compilar

```bash
make
```

## Como executar

```bash
./os_simulator
```

ou

```bash
make run
```

## Componentes

- **emulator.c**: processo principal
- **kernel.c**: escalonador e gerenciador
- **interruptionController.c**: gerador de interrupções
- **genericProcess.c**: processos de usuário (A1-A5)

## Funcionamento

O sistema cria 7 processos que se comunicam via sinais e pipes. O kernel escalona os 5 processos de usuário usando Round Robin com quantum de 2 segundos.

Veja `relatorio.txt` para mais detalhes.
