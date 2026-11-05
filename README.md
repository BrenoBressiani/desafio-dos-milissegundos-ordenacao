# Competição de Ordenação: O Desafio dos Milissegundos

Implementação em **C11** de alta performance desenvolvida para a atividade acadêmica **"Competição de Ordenação: O Desafio dos Milissegundos"** (disciplina de Estrutura de Dados).

O objetivo é ordenar aproximadamente **10 milhões de números inteiros** (~60 MB em disco) no **menor tempo de CPU possível**, medido exclusivamente durante a execução do algoritmo de ordenação por meio de `clock()` (`<time.h>`).

---

## Sumário

1. [Arquitetura e Estratégia Híbrida Adaptativa](#1-arquitetura-e-estratégia-híbrida-adaptativa)
2. [Critérios de Escolha Automática](#2-critérios-de-escolha-automática)
3. [Otimizações de Hardware (macOS Apple Silicon NEON & x86_64 AVX2)](#3-otimizações-de-hardware-macos-apple-silicon-neon--x86_64-avx2)
4. [Leitura do Arquivo e Medição Isolada de Tempo](#4-leitura-do-arquivo-e-medição-isolada-de-tempo)
5. [Validação Independente de Corretude e Integridade](#5-validação-independente-de-corretude-e-integridade)
6. [Como Compilar (macOS, Linux e Windows)](#6-como-compilar-macos-linux-e-windows)
7. [Como Executar](#7-como-executar)
8. [Resultados de Desempenho](#8-resultados-de-desempenho)

---

## 1. Arquitetura e Estratégia Híbrida Adaptativa

O programa utiliza uma estratégia **Híbrida Adaptativa Puramente Linear ($O(N)$)**, sem recorrer a algoritmos baseados em comparação ($O(N \log N)$ como `qsort`, Quick Sort, Merge Sort ou Introsort).

A decisão ocorre automaticamente em tempo de execução combinando dois algoritmos lineares complementares:

### A. Counting Sort Acelerado por SIMD (`countingSortPasseUnico` / `countingSortExato`)
Utilizado quando a amplitude de valores $K = \text{máximo} - \text{mínimo} + 1$ é proporcional à quantidade de elementos $N$ e a tabela de frequências cabe confortavelmente dentro do orçamento de memória (64 MB).
- **Deslocamento por valor mínimo (`base = minimo`)**: trata números negativos e positivos uniformemente sem desperdício de memória.
- **Amostragem rápida + Passe Único com Verificação SIMD**: estima a janela $[\text{mínimo}, \text{máximo}]$ em $O(1)$ e realiza a contagem em **uma única leitura** do vetor, validando os limites em blocos de 16 inteiros via instruções vetoriais (**ARM NEON** no Mac Apple Silicon ou **AVX2** em x86_64). Se qualquer elemento escapar da janela amostrada, faz fallback imediato e transparente para a varredura exata.
- **Histograma L1 de 4 Vias (`curto[4][64]`) e Coalescência de Intervalos Contíguos**: mantém chaves de alta frequência e progressões contíguas (passo $+1$ ou $-1$) diretamente nos registradores e no cache L1 (1 KB), evitando *store-to-load forwarding stalls* e eliminando milhares de *page faults* do sistema operacional quando a tabela principal não precisa ser tocada.
- **Escrita Vetorizada em Blocos**: reconstrói o vetor ordenado gravando de 16 a 32 inteiros por iteração com instruções SIMD.

### B. Radix Sort LSD com Dígitos de 11 Bits (`radixSortLSD`)
Utilizado automaticamente quando a amplitude de valores $K$ é grande demais para o Counting Sort (por exemplo, valores espalhados entre `INT32_MIN` e `INT32_MAX`).
- **Base $2^{11} = 2048$ baldes**: cada tabela de histograma ocupa exatamente $2048 \times 4\text{ bytes} = 8\text{ KB}$, cabendo integralmente no **cache L1 de dados** de qualquer processador moderno (Apple M1/M2/M3/M4, Intel Core ou AMD Ryzen) e cobrindo inteiros de 32 bits em no máximo **3 passes** (contra 4 passes do Radix Sort tradicional de 8 bits).
- **Histogramas em Passe Único**: calcula os histogramas de todos os passes simultaneamente em uma única leitura sequencial do vetor de entrada.
- **Corte Dinâmico de Passes Redundantes (*Bit-Width Pruning*)**: calcula a quantidade exata de bits ativos de $(\text{máximo} - \text{mínimo})$ e executa apenas os passes estritamente necessários (1, 2 ou 3 passes), além de pular qualquer passe em que todos os elementos compartilhem o mesmo dígito.
- **Suporte Nativo a Negativos e Extremos (`INT32_MIN` a `INT32_MAX`)**: aplica a transformação bijetora sem sinal `chave = (uint32_t)v[i] - (uint32_t)minimo`, evitando *signed integer overflow* (comportamento indefinido em C).

---

## 2. Critérios de Escolha Automática

| Condição Detectada | Algoritmo Escolhido | Complexidade de Tempo | Memória Auxiliar |
| :--- | :--- | :--- | :--- |
| $N \le 1$, lista já ordenada ou $\text{faixa} \le 1$ | **Detecção Imediata (Saída Rápida)** | $O(N)$ | $O(1)$ |
| $\text{faixa} \le 2 \times N$ **e** $\text{faixa} \times 4\text{ B} \le 64\text{ MB}$ | **Counting Sort SIMD** | $O(N + K)$ | Até $64\text{ MB}$ (ou $< 2\text{ KB}$ em L1) |
| $\text{faixa} > 2 \times N$ **ou** $\text{faixa} \times 4\text{ B} > 64\text{ MB}$ | **Radix Sort LSD (11 bits)** | $O(p \cdot N)$, $p \in \{1,2,3\}$ | $N \times 4\text{ B} + 24\text{ KB}$ (L1) |

---

## 3. Otimizações de Hardware (macOS Apple Silicon NEON & x86_64 AVX2)

O código-fonte possui detecção automática de arquitetura em tempo de compilação:
- **Apple Silicon Mac (`__ARM_NEON` / `__aarch64__` — M1, M2, M3, M4)**: utiliza intrínsecos `<arm_neon.h>` (`vld1q_s32`, `vst1q_s32`, `vsubq_s32`, `vaddq_s32`, `veorq_s32`, `vorrq_u32`, `vmaxvq_u32`) para processar 16 inteiros por iteração usando 4 registradores NEON de 128 bits em paralelo, aproveitando as 4 unidades de leitura/escrita simultâneas e a largura de banda de memória unificada do Apple Silicon.
- **Intel Mac / Linux / Windows x86_64 (`__AVX2__` / `_MSC_VER`)**: utiliza intrínsecos `<immintrin.h>` (`_mm256_loadu_si256`, `_mm256_storeu_si256`, `_mm256_testz_si256`) em registradores YMM de 256 bits.
- **Fallback Portável C11**: compila e funciona em qualquer outra arquitetura padrão C11 sem dependências externas.

---

## 4. Leitura do Arquivo e Medição Isolada de Tempo

- **Leitura em Bloco Único (`fread`)**: o arquivo é carregado integralmente em memória com uma única chamada `fread()` e convertido por um parser léxico dedicado (`converterBufferParaInteiros`), que aceita espaços (`' '`), tabulações (`'\t'`) e quebras de linha (`'\n'`, `'\r'`), além de números negativos e positivos.
- **Medição Exclusiva com `clock()`**: conforme exigido pelas regras da competição, o cronômetro de CPU envolve **exclusivamente** a chamada `ordenarVetor`:

```c
clock_t inicio = clock();

bool sucesso = ordenarVetor(vetor, quantidade, &estrategia);

clock_t fim = clock();

double tempo = ((double)(fim - inicio)) / CLOCKS_PER_SEC;
```

Nenhuma operação de leitura de disco, parsing de texto, cálculo de assinatura prévia, validação pós-ordenação ou impressão no terminal é incluída na janela medida.

---

## 5. Validação Independente de Corretude e Integridade

Após o término de `clock()`, a função `validarOrdenacao` realiza três verificações independentes:
1. **Ordem Crescente (Não Decrescente)**: confirma que `vetor[i - 1] <= vetor[i]` para todo $1 \le i < N$.
2. **Preservação de Quantidade e Elementos (Assinatura Algébrica e Bitwise de 64 bits)**: compara a quantidade $N$, a soma $\sum x_i$, a soma dos quadrados $\sum x_i^2$ (módulo $2^{64}$) e o XOR acumulado $\bigoplus x_i$ antes e depois da ordenação, garantindo que nenhum número foi perdido, duplicado ou adulterado.
3. **Preservação e Sinalização de Repetidos**: preserva integralmente todas as ocorrências de valores repetidos e exibe no relatório final a quantidade exata de repetições adjacentes encontradas.

---

## 6. Como Compilar (macOS, Linux e Windows)

### No macOS (MacBook Apple Silicon M1/M2/M3/M4 ou Intel — Recomendado)
```bash
clang -O3 -mcpu=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
```
*(Ou utilizando o comando `gcc` padrão do Xcode Command Line Tools no macOS)*:
```bash
gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
```

### No Linux ou Windows (GCC / MinGW-w64)
```bash
gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
```

### No Windows (MSVC — Developer Command Prompt for VS)
```cmd
cl /O2 /arch:AVX2 /W4 ordenacao.c /Fe:ordenacao.exe
```

---

## 7. Como Executar

Por padrão, o programa busca o arquivo `listaOdenada.txt` no diretório atual:

```bash
# macOS / Linux
./ordenacao

# Windows
.\ordenacao.exe
```

Também é possível informar o caminho do arquivo de entrada (e, opcionalmente, um arquivo de saída para gravar a lista ordenada):

```bash
# Informando o caminho do arquivo de entrada
./ordenacao ../listaOdenada.txt

# Informando arquivo de entrada e gravando o resultado ordenado em disco
./ordenacao ../listaOdenada.txt resultadoOrdenado.txt
```

---

## 8. Resultados de Desempenho

Teste real executado com o arquivo oficial `listaOdenada.txt` (**10.000.000 de inteiros**, ~60,6 MB):

```text
============================================================
  COMPETICAO DE ORDENACAO: O DESAFIO DOS MILISSEGUNDOS
============================================================
Arquivo de entrada : ..\listaOdenada.txt
Elementos lidos    : 10000000
Algoritmo aplicado : Hibrido Adaptativo -> Counting Sort
Tempo de ordenação : 0.010000 segundos (10.000 ms)
------------------------------------------------------------
Ordem crescente    : OK (Confirmada)
Integridade dados  : OK (zero perdas/adulteracoes)
Valores repetidos  : SIM (7500000 ocorrencias adjacentes preservadas)
Verificação final  : OK
============================================================
```
