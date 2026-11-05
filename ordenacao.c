/*
 * ============================================================================
 * Competicao de Ordenacao: O Desafio dos Milissegundos
 * Disciplina: Estrutura de Dados
 *
 * Estrategia: Hibrido Adaptativo (Counting Sort + Radix Sort LSD)
 *   - Counting Sort acelerado por SIMD (Apple Silicon ARM NEON e x86 AVX2)
 *     quando a amplitude de valores e compativel com o tamanho da entrada
 *     e o limite de memoria.
 *   - Radix Sort LSD (digitos de 11 bits / 2048 baldes em L1, ate 3 passes)
 *     com histogramas calculados em passe unico quando a amplitude e grande.
 *
 * Compilacao no macOS (Apple Silicon M1/M2/M3/M4 ou Intel):
 *   clang -O3 -mcpu=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
 *   (ou: gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c)
 *
 * Compilacao no Linux / Windows (GCC / MinGW):
 *   gcc -O3 -march=native -std=c11 -Wall -Wextra -o ordenacao ordenacao.c
 *
 * Compilacao no Windows (MSVC / Visual Studio Build Tools x64):
 *   cl /O2 /arch:AVX2 /W4 ordenacao.c /Fe:ordenacao.exe
 *
 * Execucao:
 *   ./ordenacao [arquivo_entrada] [arquivo_saida_opcional]
 *   (Arquivo padrao: listaOdenada.txt)
 * ============================================================================
 */

#define _CRT_SECURE_NO_WARNINGS

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Deteccao automatica de SIMD: ARM NEON (Mac Apple Silicon) ou AVX2 (x86_64) */
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define TEM_NEON 1
#define TEM_AVX2 0
#elif defined(__AVX2__) || (defined(_MSC_VER) && (defined(_M_X64) || defined(_M_AMD64)))
#include <immintrin.h>
#define TEM_NEON 0
#define TEM_AVX2 1
#else
#define TEM_NEON 0
#define TEM_AVX2 0
#endif

/* Compatibilidade do qualificativo restrict entre C99/C11 e MSVC */
#if defined(_MSC_VER) && !defined(__clang__)
#define RESTRITO __restrict
#else
#define RESTRITO restrict
#endif

/* Configuracoes e limites da estrategia hibrida */
#define ARQUIVO_PADRAO "listaOdenada.txt"
#define LIMITE_MEMORIA_CONTAGEM (64ULL * 1024ULL * 1024ULL) /* Max 64 MB na tabela de contagem */
#define FATOR_MAX_FAIXA 2ULL                                /* Usa Counting Sort se faixa <= 2 * n */

/* Configuracoes do Radix Sort LSD (11 bits = 2048 baldes = 8 KB no cache L1) */
#define RADIX_BITS 11u
#define RADIX_BALDES (1u << RADIX_BITS)
#define RADIX_MASCARA (RADIX_BALDES - 1u)
#define RADIX_MAX_PASSES 3u

/* Tamanho do buffer de escrita para arquivo de saida (64 KB) */
#define TAM_BUFFER_ESCRITA (1u << 16)

/* Capacidade do buffer L1 de intervalos contiguos (passo +1 ou -1) */
#define MAX_INTERVALOS 64u

/* -------------------------------------------------------------------------- */
/* Estruturas de Dados                                                        */
/* -------------------------------------------------------------------------- */

typedef enum {
    ESTRATEGIA_JA_ORDENADO = 0,
    ESTRATEGIA_COUNTING_SORT = 1,
    ESTRATEGIA_RADIX_SORT_LSD = 2
} TipoEstrategia;

typedef struct {
    int32_t minimo;
    int32_t maximo;
    uint64_t faixa;
    bool jaOrdenado;
    bool limitesExatos;
} CaracteristicasLista;

typedef struct {
    uint64_t quantidade;
    uint64_t soma;
    uint64_t somaQuadrados;
    uint64_t xorAcumulado;
} AssinaturaDados;

typedef struct {
    bool ordenadoCrescente;
    bool integridadePreservada;
    bool possuiRepetidos;
    size_t quantidadeRepetidos;
} ResultadoValidacao;

typedef struct {
    uint32_t ini;
    uint32_t fim;
} IntervaloContiguo;

typedef struct {
    uint32_t pos;
    int32_t delta;
} EventoIntervalo;

/* -------------------------------------------------------------------------- */
/* 1. Leitura Eficiente do Arquivo (fora da medicao de clock)                 */
/* -------------------------------------------------------------------------- */

/*
 * Converte um bloco de bytes em memoria para inteiros de 32 bits com sinal.
 * Aceita separadores de espaco (' '), tabulacao ('\t') e quebras de linha
 * ('\n', '\r'). Verifica estouro de int32_t e caracteres invalidos.
 */
static bool converterBufferParaInteiros(const unsigned char * RESTRITO buffer,
                                        size_t tamanhoBytes,
                                        int32_t * RESTRITO destino,
                                        size_t capacidade,
                                        size_t * RESTRITO totalLidos)
{
    size_t n = 0;
    size_t i = 0;

    while (i < tamanhoBytes) {
        unsigned char c = buffer[i];

        /* Pula espacos, tabulacoes e quebras de linha */
        while (c == '\n' || c == '\r' || c == ' ' || c == '\t') {
            i++;
            if (i >= tamanhoBytes) {
                *totalLidos = n;
                return true;
            }
            c = buffer[i];
        }

        bool negativo = false;
        if (c == '-') {
            negativo = true;
            i++;
            if (i >= tamanhoBytes) {
                fprintf(stderr, "Erro de leitura: sinal '-' isolado no fim do arquivo.\n");
                return false;
            }
            c = buffer[i];
        } else if (c == '+') {
            i++;
            if (i >= tamanhoBytes) {
                fprintf(stderr, "Erro de leitura: sinal '+' isolado no fim do arquivo.\n");
                return false;
            }
            c = buffer[i];
        }

        if (c < '0' || c > '9') {
            fprintf(stderr, "Erro de leitura: caractere invalido (0x%02X) no byte %zu.\n",
                    (unsigned int)c, i);
            return false;
        }

        uint64_t acumulador = 0;
        const uint64_t limite = negativo ? ((uint64_t)INT32_MAX + 1ULL) : (uint64_t)INT32_MAX;

        do {
            acumulador = acumulador * 10ULL + (uint64_t)(c - '0');
            if (acumulador > limite) {
                fprintf(stderr, "Erro de leitura: valor excede int32_t no byte %zu.\n", i);
                return false;
            }
            i++;
            if (i >= tamanhoBytes)
                break;
            c = buffer[i];
        } while (c >= '0' && c <= '9');

        if (n >= capacidade) {
            fprintf(stderr, "Erro interno: capacidade do vetor excedida.\n");
            return false;
        }

        destino[n++] = negativo ? (int32_t)(-(int64_t)acumulador) : (int32_t)acumulador;
    }

    *totalLidos = n;
    return true;
}

/*
 * Carrega o arquivo inteiro em memoria com uma unica chamada fread() e
 * converte todos os numeros para um vetor contiguo de int32_t.
 */
static int32_t *lerArquivoNumeros(const char *caminho, size_t * RESTRITO quantidade)
{
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        fprintf(stderr, "Erro: nao foi possivel abrir o arquivo '%s'.\n", caminho);
        return NULL;
    }

    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fprintf(stderr, "Erro: falha ao posicionar no fim do arquivo '%s'.\n", caminho);
        fclose(arquivo);
        return NULL;
    }

    long tamanhoSigned = ftell(arquivo);
    if (tamanhoSigned < 0 || fseek(arquivo, 0, SEEK_SET) != 0) {
        fprintf(stderr, "Erro: falha ao determinar o tamanho do arquivo '%s'.\n", caminho);
        fclose(arquivo);
        return NULL;
    }

    size_t tamanhoBytes = (size_t)tamanhoSigned;
    if (tamanhoBytes == 0) {
        fclose(arquivo);
        *quantidade = 0;
        return (int32_t *)malloc(64 * sizeof(int32_t));
    }

    unsigned char *buffer = (unsigned char *)malloc(tamanhoBytes + 1);
    if (buffer == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para ler '%s' (%zu bytes).\n",
                caminho, tamanhoBytes);
        fclose(arquivo);
        return NULL;
    }

    size_t bytesLidos = fread(buffer, 1, tamanhoBytes, arquivo);
    fclose(arquivo);

    if (bytesLidos != tamanhoBytes) {
        fprintf(stderr, "Erro: leitura incompleta do arquivo '%s'.\n", caminho);
        free(buffer);
        return NULL;
    }
    buffer[tamanhoBytes] = '\n';

    size_t capacidade = (tamanhoBytes / 2) + 2;
    int32_t *vetor = (int32_t *)malloc((capacidade + 64) * sizeof(int32_t));
    if (vetor == NULL) {
        fprintf(stderr, "Erro: memoria insuficiente para alocar o vetor de entrada.\n");
        free(buffer);
        return NULL;
    }

    size_t n = 0;
    if (!converterBufferParaInteiros(buffer, tamanhoBytes, vetor, capacidade, &n)) {
        free(vetor);
        free(buffer);
        return NULL;
    }

    free(buffer);

    /* Ajusta o bloco para o tamanho real (+ folga de 64 elementos para escrita SIMD) */
    if (n > 0) {
        int32_t *vetorAjustado = (int32_t *)realloc(vetor, (n + 64) * sizeof(int32_t));
        if (vetorAjustado != NULL)
            vetor = vetorAjustado;
    }

    *quantidade = n;
    return vetor;
}

/* -------------------------------------------------------------------------- */
/* 2. Identificacao das Caracteristicas e Escolha da Estrategia               */
/* -------------------------------------------------------------------------- */

/*
 * Varredura completa (usada em listas pequenas, quando a amostragem indica
 * faixa ampla ou quando algum elemento escapa da janela amostrada).
 */
static CaracteristicasLista analisarListaCompleta(const int32_t * RESTRITO v, size_t n)
{
    CaracteristicasLista info;
    if (n == 0) {
        info.minimo = 0;
        info.maximo = 0;
        info.faixa = 0;
        info.jaOrdenado = true;
        info.limitesExatos = true;
        return info;
    }

    int32_t mn = v[0];
    int32_t mx = v[0];
    bool ordenado = true;

    for (size_t i = 1; i < n; i++) {
        int32_t x = v[i];
        if (x < v[i - 1])
            ordenado = false;
        if (x < mn)
            mn = x;
        if (x > mx)
            mx = x;
    }

    info.minimo = mn;
    info.maximo = mx;
    info.faixa = (uint64_t)((int64_t)mx - (int64_t)mn) + 1ULL;
    info.jaOrdenado = ordenado;
    info.limitesExatos = true;
    return info;
}

/*
 * Amostragem rapida em O(1) (1000 blocos) para estimar minimo e maximo sem
 * precisar ler os 40 MB duas vezes da RAM quando o Counting Sort em passe
 * unico puder ser aplicado.
 */
static CaracteristicasLista amostrarCaracteristicas(const int32_t * RESTRITO v, size_t n)
{
    if (n <= 4096)
        return analisarListaCompleta(v, n);

    int32_t mn = v[0];
    int32_t mx = v[0];
    size_t passo = n / 1000;
    if (passo == 0)
        passo = 1;

    for (size_t i = 0; i < n; i += passo) {
        int32_t x0 = v[i];
        if (x0 < mn) mn = x0;
        if (x0 > mx) mx = x0;
        if (i > 0) {
            int32_t x1 = v[i - 1];
            if (x1 < mn) mn = x1;
            if (x1 > mx) mx = x1;
        }
    }
    if (v[n - 1] < mn) mn = v[n - 1];
    if (v[n - 1] > mx) mx = v[n - 1];

    CaracteristicasLista info;
    info.minimo = mn;
    info.maximo = mx;
    info.faixa = (uint64_t)((int64_t)mx - (int64_t)mn) + 1ULL;
    info.jaOrdenado = false;
    info.limitesExatos = false;
    return info;
}

/*
 * Decide automaticamente entre Counting Sort e Radix Sort LSD com base na
 * quantidade de elementos, na amplitude (maximo - minimo + 1) e no consumo
 * de memoria da estrutura auxiliar.
 */
static TipoEstrategia escolherEstrategia(size_t n, const CaracteristicasLista *info)
{
    if (n <= 1 || info->jaOrdenado || info->faixa <= 1ULL)
        return ESTRATEGIA_JA_ORDENADO;

    uint64_t bytesContagem = info->faixa * sizeof(uint32_t);
    if (info->faixa <= (uint64_t)n * FATOR_MAX_FAIXA &&
        bytesContagem <= LIMITE_MEMORIA_CONTAGEM) {
        return ESTRATEGIA_COUNTING_SORT;
    }

    return ESTRATEGIA_RADIX_SORT_LSD;
}

/* -------------------------------------------------------------------------- */
/* 3. Algoritmos de Ordenacao (Counting Sort + Radix Sort LSD)                */
/* -------------------------------------------------------------------------- */

static void descarregarIntervalosContagem(uint32_t * RESTRITO c,
                                          IntervaloContiguo * RESTRITO iv,
                                          size_t * RESTRITO niv)
{
    size_t m = *niv;
#if TEM_AVX2
    const __m256i vUns = _mm256_set1_epi32(1);
    for (size_t s = 0; s < m; s++) {
        uint32_t k = iv[s].ini;
        uint32_t fim = iv[s].fim;
        for (; k + 32 <= fim; k += 32) {
            uint32_t *ck = c + k;
            _mm256_storeu_si256((__m256i *)(ck),      _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(ck)),      vUns));
            _mm256_storeu_si256((__m256i *)(ck + 8),  _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(ck + 8)),  vUns));
            _mm256_storeu_si256((__m256i *)(ck + 16), _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(ck + 16)), vUns));
            _mm256_storeu_si256((__m256i *)(ck + 24), _mm256_add_epi32(_mm256_loadu_si256((const __m256i *)(ck + 24)), vUns));
        }
        for (; k < fim; k++)
            c[k]++;
    }
#elif TEM_NEON
    const uint32x4_t vUns = vdupq_n_u32(1);
    for (size_t s = 0; s < m; s++) {
        uint32_t k = iv[s].ini;
        uint32_t fim = iv[s].fim;
        for (; k + 16 <= fim; k += 16) {
            uint32_t *ck = c + k;
            vst1q_u32(ck,      vaddq_u32(vld1q_u32(ck),      vUns));
            vst1q_u32(ck + 4,  vaddq_u32(vld1q_u32(ck + 4),  vUns));
            vst1q_u32(ck + 8,  vaddq_u32(vld1q_u32(ck + 8),  vUns));
            vst1q_u32(ck + 12, vaddq_u32(vld1q_u32(ck + 12), vUns));
        }
        for (; k < fim; k++)
            c[k]++;
    }
#else
    for (size_t s = 0; s < m; s++) {
        for (uint32_t k = iv[s].ini; k < iv[s].fim; k++)
            c[k]++;
    }
#endif
    *niv = 0;
}

/*
 * Preenche rapidamente um segmento [kIni, kFim) onde todas as chaves possuem
 * a mesma frequencia constante 'q' (derivada da sobreposicao de intervalos).
 */
static void preencherSegmentoFrequenciaConstante(int32_t ** RESTRITO pp,
                                                 int32_t *fimVetor,
                                                 uint32_t kIni,
                                                 uint32_t kFim,
                                                 uint32_t q,
                                                 uint32_t base)
{
    if (q == 0 || kIni >= kFim)
        return;

    int32_t *p = *pp;
    uint32_t k = kIni;

#if TEM_AVX2
    if (q == 1) {
        __m256i vVal = _mm256_add_epi32(_mm256_set1_epi32((int32_t)(base + k)),
                                        _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7));
        const __m256i vStep = _mm256_set1_epi32(8);
        for (; k + 8 <= kFim; k += 8) {
            _mm256_storeu_si256((__m256i *)p, vVal);
            vVal = _mm256_add_epi32(vVal, vStep);
            p += 8;
        }
    } else if (q == 2) {
        __m256i vVal = _mm256_add_epi32(_mm256_set1_epi32((int32_t)(base + k)),
                                        _mm256_setr_epi32(0, 0, 1, 1, 2, 2, 3, 3));
        const __m256i vStep = _mm256_set1_epi32(4);
        for (; k + 4 <= kFim; k += 4) {
            _mm256_storeu_si256((__m256i *)p, vVal);
            vVal = _mm256_add_epi32(vVal, vStep);
            p += 8;
        }
    } else if (q == 4) {
        __m256i vVal = _mm256_add_epi32(_mm256_set1_epi32((int32_t)(base + k)),
                                        _mm256_setr_epi32(0, 0, 0, 0, 1, 1, 1, 1));
        const __m256i vStep = _mm256_set1_epi32(2);
        for (; k + 2 <= kFim; k += 2) {
            _mm256_storeu_si256((__m256i *)p, vVal);
            vVal = _mm256_add_epi32(vVal, vStep);
            p += 8;
        }
    } else if (q <= 8) {
        for (; k < kFim && fimVetor - p >= 8; k++) {
            _mm256_storeu_si256((__m256i *)p, _mm256_set1_epi32((int32_t)(base + k)));
            p += q;
        }
    }
#elif TEM_NEON
    if (q == 1) {
        const int32_t r0[4] = {0, 1, 2, 3};
        const int32_t r1[4] = {4, 5, 6, 7};
        int32x4_t v0 = vaddq_s32(vdupq_n_s32((int32_t)(base + k)), vld1q_s32(r0));
        int32x4_t v1 = vaddq_s32(vdupq_n_s32((int32_t)(base + k)), vld1q_s32(r1));
        const int32x4_t vStep = vdupq_n_s32(8);
        for (; k + 8 <= kFim; k += 8) {
            vst1q_s32(p,     v0);
            vst1q_s32(p + 4, v1);
            v0 = vaddq_s32(v0, vStep);
            v1 = vaddq_s32(v1, vStep);
            p += 8;
        }
    } else if (q == 2) {
        const int32_t r0[4] = {0, 0, 1, 1};
        const int32_t r1[4] = {2, 2, 3, 3};
        int32x4_t v0 = vaddq_s32(vdupq_n_s32((int32_t)(base + k)), vld1q_s32(r0));
        int32x4_t v1 = vaddq_s32(vdupq_n_s32((int32_t)(base + k)), vld1q_s32(r1));
        const int32x4_t vStep = vdupq_n_s32(4);
        for (; k + 4 <= kFim; k += 4) {
            vst1q_s32(p,     v0);
            vst1q_s32(p + 4, v1);
            v0 = vaddq_s32(v0, vStep);
            v1 = vaddq_s32(v1, vStep);
            p += 8;
        }
    } else if (q == 4) {
        int32x4_t v0 = vdupq_n_s32((int32_t)(base + k));
        int32x4_t v1 = vdupq_n_s32((int32_t)(base + k + 1));
        const int32x4_t vStep = vdupq_n_s32(2);
        for (; k + 2 <= kFim; k += 2) {
            vst1q_s32(p,     v0);
            vst1q_s32(p + 4, v1);
            v0 = vaddq_s32(v0, vStep);
            v1 = vaddq_s32(v1, vStep);
            p += 8;
        }
    } else if (q <= 8) {
        for (; k < kFim && fimVetor - p >= 8; k++) {
            int32x4_t vv = vdupq_n_s32((int32_t)(base + k));
            vst1q_s32(p,     vv);
            vst1q_s32(p + 4, vv);
            p += q;
        }
    }
#endif

    for (; k < kFim; k++) {
        int32_t val = (int32_t)(base + k);
        for (uint32_t j = 0; j < q; j++)
            *p++ = val;
    }

    *pp = p;
}

/*
 * Counting Sort em passe unico com verificacao de limites SIMD (NEON / AVX2).
 * Se encontrar qualquer elemento fora da faixa estimada [minimo, maximo],
 * retorna false imediatamente para que o controlador execute a analise exata.
 */
static bool countingSortPasseUnico(int32_t * RESTRITO v,
                                   size_t n,
                                   int32_t minimo,
                                   size_t faixa)
{
    const uint32_t base = (uint32_t)minimo;
    size_t cap = 1;
    while (cap < faixa)
        cap <<= 1;

    uint32_t *c = NULL;
    uint32_t curto[4][64];
    memset(curto, 0, sizeof(curto));

    IntervaloContiguo iv[MAX_INTERVALOS];
    size_t niv = 0;
    size_t i = 0;

#if TEM_AVX2
    const __m256i vbase      = _mm256_set1_epi32((int32_t)base);
    const __m256i vmaskFora  = _mm256_set1_epi32((int32_t)~(uint32_t)(cap - 1));
    const __m256i vmask64    = _mm256_set1_epi32((int32_t)~63u);
    const __m256i vAsc0      = _mm256_setr_epi32(0, 1, 2, 3, 4, 5, 6, 7);
    const __m256i vDesc0     = _mm256_setr_epi32(0, -1, -2, -3, -4, -5, -6, -7);
    const __m256i vOito      = _mm256_set1_epi32(8);
    const __m256i vMenosOito = _mm256_set1_epi32(-8);

    for (; i + 16 <= n; i += 16) {
        int32_t diff = (int32_t)((uint32_t)v[i + 15] - (uint32_t)v[i]);
        __m256i vx0 = _mm256_loadu_si256((const __m256i *)(v + i));
        __m256i vx1 = _mm256_loadu_si256((const __m256i *)(v + i + 8));

        if (diff == 15) {
            __m256i d = _mm256_xor_si256(_mm256_sub_epi32(vx0, _mm256_set1_epi32(v[i])), vAsc0);
            d = _mm256_or_si256(d, _mm256_xor_si256(_mm256_sub_epi32(vx1, vx0), vOito));
            if (_mm256_testz_si256(d, d)) {
                uint32_t k = (uint32_t)v[i] - base;
                if (k >= faixa || faixa - k < 16u) {
                    free(c);
                    return false;
                }
                if (niv > 0 && iv[niv - 1].fim == k) {
                    iv[niv - 1].fim = k + 16;
                } else {
                    if (niv == MAX_INTERVALOS) {
                        if (c == NULL) {
                            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
                            if (c == NULL) return false;
                        }
                        descarregarIntervalosContagem(c, iv, &niv);
                    }
                    iv[niv].ini = k;
                    iv[niv].fim = k + 16;
                    niv++;
                }
                continue;
            }
        } else if (diff == -15) {
            __m256i d = _mm256_xor_si256(_mm256_sub_epi32(vx0, _mm256_set1_epi32(v[i])), vDesc0);
            d = _mm256_or_si256(d, _mm256_xor_si256(_mm256_sub_epi32(vx1, vx0), vMenosOito));
            if (_mm256_testz_si256(d, d)) {
                uint32_t k = (uint32_t)v[i + 15] - base;
                if (k >= faixa || faixa - k < 16u) {
                    free(c);
                    return false;
                }
                if (niv > 0 && iv[niv - 1].ini == k + 16) {
                    iv[niv - 1].ini = k;
                } else {
                    if (niv == MAX_INTERVALOS) {
                        if (c == NULL) {
                            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
                            if (c == NULL) return false;
                        }
                        descarregarIntervalosContagem(c, iv, &niv);
                    }
                    iv[niv].ini = k;
                    iv[niv].fim = k + 16;
                    niv++;
                }
                continue;
            }
        }

        __m256i vOr = _mm256_or_si256(_mm256_sub_epi32(vx0, vbase), _mm256_sub_epi32(vx1, vbase));

        if (_mm256_testz_si256(vOr, vmask64)) {
            const int32_t *p = v + i;
            for (size_t j = 0; j < 16; j += 4) {
                curto[0][(uint32_t)p[j]     - base]++;
                curto[1][(uint32_t)p[j + 1] - base]++;
                curto[2][(uint32_t)p[j + 2] - base]++;
                curto[3][(uint32_t)p[j + 3] - base]++;
            }
            continue;
        }

        if (!_mm256_testz_si256(vOr, vmaskFora)) {
            free(c);
            return false;
        }

        if (c == NULL) {
            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
            if (c == NULL) return false;
        }

        const int32_t *p = v + i;
        for (size_t j = 0; j < 16; j += 8) {
            uint32_t k0 = (uint32_t)p[j]     - base, k1 = (uint32_t)p[j + 1] - base;
            uint32_t k2 = (uint32_t)p[j + 2] - base, k3 = (uint32_t)p[j + 3] - base;
            uint32_t k4 = (uint32_t)p[j + 4] - base, k5 = (uint32_t)p[j + 5] - base;
            uint32_t k6 = (uint32_t)p[j + 6] - base, k7 = (uint32_t)p[j + 7] - base;
            if (k0 >= faixa || k1 >= faixa || k2 >= faixa || k3 >= faixa ||
                k4 >= faixa || k5 >= faixa || k6 >= faixa || k7 >= faixa) {
                free(c);
                return false;
            }
            c[k0]++; c[k1]++; c[k2]++; c[k3]++;
            c[k4]++; c[k5]++; c[k6]++; c[k7]++;
        }
    }
#elif TEM_NEON
    const uint32x4_t vbase = vdupq_n_u32(base);
    const int32_t rAsc[4]  = {0, 1, 2, 3};
    const int32_t rDesc[4] = {0, -1, -2, -3};
    const int32x4_t vAsc0  = vld1q_s32(rAsc);
    const int32x4_t vDesc0 = vld1q_s32(rDesc);
    const int32x4_t vQuatro      = vdupq_n_s32(4);
    const int32x4_t vMenosQuatro = vdupq_n_s32(-4);

    for (; i + 16 <= n; i += 16) {
        const int32_t *p = v + i;
        int32_t diff = (int32_t)((uint32_t)p[15] - (uint32_t)p[0]);
        int32x4_t vx0 = vld1q_s32(p);
        int32x4_t vx1 = vld1q_s32(p + 4);
        int32x4_t vx2 = vld1q_s32(p + 8);
        int32x4_t vx3 = vld1q_s32(p + 12);

        if (diff == 15) {
            int32x4_t d = veorq_s32(vsubq_s32(vx0, vdupq_n_s32(p[0])), vAsc0);
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx1, vx0), vQuatro));
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx2, vx1), vQuatro));
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx3, vx2), vQuatro));
            if (vmaxvq_u32(vreinterpretq_u32_s32(d)) == 0u) {
                uint32_t k = (uint32_t)p[0] - base;
                if (k >= faixa || faixa - k < 16u) {
                    free(c);
                    return false;
                }
                if (niv > 0 && iv[niv - 1].fim == k) {
                    iv[niv - 1].fim = k + 16;
                } else {
                    if (niv == MAX_INTERVALOS) {
                        if (c == NULL) {
                            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
                            if (c == NULL) return false;
                        }
                        descarregarIntervalosContagem(c, iv, &niv);
                    }
                    iv[niv].ini = k;
                    iv[niv].fim = k + 16;
                    niv++;
                }
                continue;
            }
        } else if (diff == -15) {
            int32x4_t d = veorq_s32(vsubq_s32(vx0, vdupq_n_s32(p[0])), vDesc0);
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx1, vx0), vMenosQuatro));
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx2, vx1), vMenosQuatro));
            d = vorrq_s32(d, veorq_s32(vsubq_s32(vx3, vx2), vMenosQuatro));
            if (vmaxvq_u32(vreinterpretq_u32_s32(d)) == 0u) {
                uint32_t k = (uint32_t)p[15] - base;
                if (k >= faixa || faixa - k < 16u) {
                    free(c);
                    return false;
                }
                if (niv > 0 && iv[niv - 1].ini == k + 16) {
                    iv[niv - 1].ini = k;
                } else {
                    if (niv == MAX_INTERVALOS) {
                        if (c == NULL) {
                            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
                            if (c == NULL) return false;
                        }
                        descarregarIntervalosContagem(c, iv, &niv);
                    }
                    iv[niv].ini = k;
                    iv[niv].fim = k + 16;
                    niv++;
                }
                continue;
            }
        }

        uint32x4_t vOr0 = vorrq_u32(vsubq_u32(vreinterpretq_u32_s32(vx0), vbase),
                                    vsubq_u32(vreinterpretq_u32_s32(vx1), vbase));
        uint32x4_t vOr1 = vorrq_u32(vsubq_u32(vreinterpretq_u32_s32(vx2), vbase),
                                    vsubq_u32(vreinterpretq_u32_s32(vx3), vbase));
        uint32_t maxOr = vmaxvq_u32(vorrq_u32(vOr0, vOr1));

        if (maxOr < 64u) {
            for (size_t j = 0; j < 16; j += 4) {
                curto[0][(uint32_t)p[j]     - base]++;
                curto[1][(uint32_t)p[j + 1] - base]++;
                curto[2][(uint32_t)p[j + 2] - base]++;
                curto[3][(uint32_t)p[j + 3] - base]++;
            }
            continue;
        }

        if (maxOr >= cap) {
            free(c);
            return false;
        }

        if (c == NULL) {
            c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
            if (c == NULL) return false;
        }

        for (size_t j = 0; j < 16; j += 8) {
            uint32_t k0 = (uint32_t)p[j]     - base, k1 = (uint32_t)p[j + 1] - base;
            uint32_t k2 = (uint32_t)p[j + 2] - base, k3 = (uint32_t)p[j + 3] - base;
            uint32_t k4 = (uint32_t)p[j + 4] - base, k5 = (uint32_t)p[j + 5] - base;
            uint32_t k6 = (uint32_t)p[j + 6] - base, k7 = (uint32_t)p[j + 7] - base;
            if (k0 >= faixa || k1 >= faixa || k2 >= faixa || k3 >= faixa ||
                k4 >= faixa || k5 >= faixa || k6 >= faixa || k7 >= faixa) {
                free(c);
                return false;
            }
            c[k0]++; c[k1]++; c[k2]++; c[k3]++;
            c[k4]++; c[k5]++; c[k6]++; c[k7]++;
        }
    }
#endif

    for (; i < n; i++) {
        uint32_t k = (uint32_t)v[i] - base;
        if (k >= faixa) {
            free(c);
            return false;
        }
        if (k < 64) {
            curto[0][k]++;
        } else {
            if (c == NULL) {
                c = (uint32_t *)calloc(faixa + 64, sizeof(uint32_t));
                if (c == NULL) return false;
            }
            c[k]++;
        }
    }

    if (faixa < 64) {
        for (size_t j = faixa; j < 64; j++) {
            if ((curto[0][j] | curto[1][j] | curto[2][j] | curto[3][j]) != 0u) {
                free(c);
                return false;
            }
        }
    }

    int32_t *fimVetor = v + n;

    /* Caminho rapido em cache L1: quando nao houve chaves dispersas >= 64 fora de intervalos */
    if (c == NULL) {
        uint32_t c64[64];
        for (size_t j = 0; j < 64; j++)
            c64[j] = curto[0][j] + curto[1][j] + curto[2][j] + curto[3][j];

        for (size_t s = 0; s < niv; s++) {
            uint32_t ini = iv[s].ini;
            uint32_t fim = iv[s].fim < 64 ? iv[s].fim : 64;
            for (uint32_t k = ini; k < fim; k++)
                c64[k]++;
        }

        int32_t *p = v;
        size_t limCurto = faixa < 64 ? faixa : 64;
        for (size_t k = 0; k < limCurto; k++) {
            uint32_t q = c64[k];
            int32_t val = (int32_t)(base + (uint32_t)k);
#if TEM_AVX2
            __m256i v256 = _mm256_set1_epi32(val);
            int32_t *w = p;
            int32_t *e = p + q;
            while (e - w >= 32) {
                _mm256_storeu_si256((__m256i *)w, v256);
                _mm256_storeu_si256((__m256i *)(w + 8), v256);
                _mm256_storeu_si256((__m256i *)(w + 16), v256);
                _mm256_storeu_si256((__m256i *)(w + 24), v256);
                w += 32;
            }
            while (e - w >= 8) {
                _mm256_storeu_si256((__m256i *)w, v256);
                w += 8;
            }
            while (w < e)
                *w++ = val;
            p = e;
#elif TEM_NEON
            int32x4_t v128 = vdupq_n_s32(val);
            int32_t *w = p;
            int32_t *e = p + q;
            while (e - w >= 16) {
                vst1q_s32(w,      v128);
                vst1q_s32(w + 4,  v128);
                vst1q_s32(w + 8,  v128);
                vst1q_s32(w + 12, v128);
                w += 16;
            }
            while (e - w >= 4) {
                vst1q_s32(w, v128);
                w += 4;
            }
            while (w < e)
                *w++ = val;
            p = e;
#else
            for (uint32_t j = 0; j < q; j++)
                *p++ = val;
#endif
        }

        if (faixa > 64) {
            EventoIntervalo ev[2 * MAX_INTERVALOS + 2];
            size_t nev = 0;
            for (size_t s = 0; s < niv; s++) {
                uint32_t ini = iv[s].ini < 64 ? 64 : iv[s].ini;
                uint32_t fim = iv[s].fim;
                if (ini < fim) {
                    ev[nev].pos = ini; ev[nev].delta = 1;  nev++;
                    ev[nev].pos = fim; ev[nev].delta = -1; nev++;
                }
            }
            for (size_t a = 1; a < nev; a++) {
                EventoIntervalo atual = ev[a];
                size_t b = a;
                while (b > 0 && ev[b - 1].pos > atual.pos) {
                    ev[b] = ev[b - 1];
                    b--;
                }
                ev[b] = atual;
            }

            int32_t cobertura = 0;
            uint32_t posAtual = 64;
            size_t e = 0;
            while (e < nev) {
                uint32_t proxPos = ev[e].pos;
                if (proxPos > posAtual && cobertura > 0) {
                    preencherSegmentoFrequenciaConstante(&p, fimVetor, posAtual, proxPos,
                                                         (uint32_t)cobertura, base);
                }
                posAtual = proxPos;
                while (e < nev && ev[e].pos == posAtual) {
                    cobertura += ev[e].delta;
                    e++;
                }
            }
        }
        return true;
    }

    /* Caminho geral do Counting Sort com tabela completa */
    if (niv > 0)
        descarregarIntervalosContagem(c, iv, &niv);

    size_t limCurto = faixa < 64 ? faixa : 64;
    for (size_t j = 0; j < limCurto; j++)
        c[j] += curto[0][j] + curto[1][j] + curto[2][j] + curto[3][j];

    int32_t *p = v;
    size_t k = 0;

#if TEM_AVX2
    for (; k + 4 <= faixa && fimVetor - p >= 16; k += 4) {
        uint32_t q0 = c[k], q1 = c[k + 1], q2 = c[k + 2], q3 = c[k + 3];
        int32_t b = (int32_t)(base + (uint32_t)k);
        if ((q0 | q1 | q2 | q3) <= 4u) {
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b));     p += q0;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 1)); p += q1;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 2)); p += q2;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 3)); p += q3;
        } else {
            for (int d = 0; d < 4; d++) {
                uint32_t q = c[k + d];
                __m256i v256 = _mm256_set1_epi32(b + d);
                int32_t *w = p;
                int32_t *e = p + q;
                while (e - w >= 32) {
                    _mm256_storeu_si256((__m256i *)w, v256);
                    _mm256_storeu_si256((__m256i *)(w + 8), v256);
                    _mm256_storeu_si256((__m256i *)(w + 16), v256);
                    _mm256_storeu_si256((__m256i *)(w + 24), v256);
                    w += 32;
                }
                while (e - w >= 8) {
                    _mm256_storeu_si256((__m256i *)w, v256);
                    w += 8;
                }
                while (w < e)
                    *w++ = b + d;
                p = e;
            }
        }
    }
#elif TEM_NEON
    for (; k + 4 <= faixa && fimVetor - p >= 16; k += 4) {
        uint32_t q0 = c[k], q1 = c[k + 1], q2 = c[k + 2], q3 = c[k + 3];
        int32_t b = (int32_t)(base + (uint32_t)k);
        if ((q0 | q1 | q2 | q3) <= 4u) {
            vst1q_s32(p, vdupq_n_s32(b));     p += q0;
            vst1q_s32(p, vdupq_n_s32(b + 1)); p += q1;
            vst1q_s32(p, vdupq_n_s32(b + 2)); p += q2;
            vst1q_s32(p, vdupq_n_s32(b + 3)); p += q3;
        } else {
            for (int d = 0; d < 4; d++) {
                uint32_t q = c[k + d];
                int32x4_t v128 = vdupq_n_s32(b + d);
                int32_t *w = p;
                int32_t *e = p + q;
                while (e - w >= 16) {
                    vst1q_s32(w,      v128);
                    vst1q_s32(w + 4,  v128);
                    vst1q_s32(w + 8,  v128);
                    vst1q_s32(w + 12, v128);
                    w += 16;
                }
                while (e - w >= 4) {
                    vst1q_s32(w, v128);
                    w += 4;
                }
                while (w < e)
                    *w++ = b + d;
                p = e;
            }
        }
    }
#endif

    for (; k < faixa; k++) {
        int32_t val = (int32_t)(base + (uint32_t)k);
        for (uint32_t j = c[k]; j > 0; j--)
            *p++ = val;
    }

    free(c);
    return true;
}

/*
 * Counting Sort direto com limites exatos previamente conhecidos.
 */
static bool countingSortExato(int32_t * RESTRITO v,
                              size_t n,
                              int32_t minimo,
                              size_t faixa)
{
    uint32_t *c = (uint32_t *)calloc(faixa + 16, sizeof(uint32_t));
    if (c == NULL)
        return false;

    const uint32_t base = (uint32_t)minimo;
    size_t i = 0;
    for (; i + 8 <= n; i += 8) {
        uint32_t k0 = (uint32_t)v[i]     - base, k1 = (uint32_t)v[i + 1] - base;
        uint32_t k2 = (uint32_t)v[i + 2] - base, k3 = (uint32_t)v[i + 3] - base;
        uint32_t k4 = (uint32_t)v[i + 4] - base, k5 = (uint32_t)v[i + 5] - base;
        uint32_t k6 = (uint32_t)v[i + 6] - base, k7 = (uint32_t)v[i + 7] - base;
        c[k0]++; c[k1]++; c[k2]++; c[k3]++;
        c[k4]++; c[k5]++; c[k6]++; c[k7]++;
    }
    for (; i < n; i++)
        c[(uint32_t)v[i] - base]++;

    int32_t *p = v;
    int32_t *fimVetor = v + n;
    size_t k = 0;

#if TEM_AVX2
    for (; k + 4 <= faixa && fimVetor - p >= 16; k += 4) {
        uint32_t q0 = c[k], q1 = c[k + 1], q2 = c[k + 2], q3 = c[k + 3];
        int32_t b = (int32_t)(base + (uint32_t)k);
        if ((q0 | q1 | q2 | q3) <= 4u) {
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b));     p += q0;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 1)); p += q1;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 2)); p += q2;
            _mm_storeu_si128((__m128i *)p, _mm_set1_epi32(b + 3)); p += q3;
        } else {
            for (int d = 0; d < 4; d++) {
                uint32_t q = c[k + d];
                __m256i v256 = _mm256_set1_epi32(b + d);
                int32_t *w = p;
                int32_t *e = p + q;
                while (e - w >= 32) {
                    _mm256_storeu_si256((__m256i *)w, v256);
                    _mm256_storeu_si256((__m256i *)(w + 8), v256);
                    _mm256_storeu_si256((__m256i *)(w + 16), v256);
                    _mm256_storeu_si256((__m256i *)(w + 24), v256);
                    w += 32;
                }
                while (e - w >= 8) {
                    _mm256_storeu_si256((__m256i *)w, v256);
                    w += 8;
                }
                while (w < e)
                    *w++ = b + d;
                p = e;
            }
        }
    }
#elif TEM_NEON
    for (; k + 4 <= faixa && fimVetor - p >= 16; k += 4) {
        uint32_t q0 = c[k], q1 = c[k + 1], q2 = c[k + 2], q3 = c[k + 3];
        int32_t b = (int32_t)(base + (uint32_t)k);
        if ((q0 | q1 | q2 | q3) <= 4u) {
            vst1q_s32(p, vdupq_n_s32(b));     p += q0;
            vst1q_s32(p, vdupq_n_s32(b + 1)); p += q1;
            vst1q_s32(p, vdupq_n_s32(b + 2)); p += q2;
            vst1q_s32(p, vdupq_n_s32(b + 3)); p += q3;
        } else {
            for (int d = 0; d < 4; d++) {
                uint32_t q = c[k + d];
                int32x4_t v128 = vdupq_n_s32(b + d);
                int32_t *w = p;
                int32_t *e = p + q;
                while (e - w >= 16) {
                    vst1q_s32(w,      v128);
                    vst1q_s32(w + 4,  v128);
                    vst1q_s32(w + 8,  v128);
                    vst1q_s32(w + 12, v128);
                    w += 16;
                }
                while (e - w >= 4) {
                    vst1q_s32(w, v128);
                    w += 4;
                }
                while (w < e)
                    *w++ = b + d;
                p = e;
            }
        }
    }
#endif

    for (; k < faixa; k++) {
        int32_t val = (int32_t)(base + (uint32_t)k);
        for (uint32_t j = c[k]; j > 0; j--)
            *p++ = val;
    }

    free(c);
    return true;
}

/*
 * Radix Sort LSD otimizado com digitos de 11 bits (2048 baldes).
 * Desloca os valores por 'minimo' (tratando negativos e extremos de int32_t
 * sem overflow) e executa apenas os passes necessarios para cobrir os bits
 * significativos de (maximo - minimo).
 */
static bool radixSortLSD(int32_t * RESTRITO v,
                         size_t n,
                         int32_t minimo,
                         int32_t maximo)
{
    int32_t *auxiliar = (int32_t *)malloc(n * sizeof(int32_t));
    if (auxiliar == NULL)
        return false;

    const uint32_t base = (uint32_t)minimo;
    const uint32_t amplitude = (uint32_t)maximo - base;

    unsigned int bits = 0;
    while (bits < 32u && (amplitude >> bits) != 0u)
        bits++;
    unsigned int numPasses = (bits + RADIX_BITS - 1u) / RADIX_BITS;
    if (numPasses == 0u)
        numPasses = 1u;

    /* Constroi todos os histogramas em uma unica leitura do vetor */
    uint32_t hist[RADIX_MAX_PASSES][RADIX_BALDES];
    memset(hist, 0, sizeof(hist));

    if (numPasses == 1u) {
        for (size_t i = 0; i < n; i++) {
            uint32_t chave = (uint32_t)v[i] - base;
            hist[0][chave & RADIX_MASCARA]++;
        }
    } else if (numPasses == 2u) {
        for (size_t i = 0; i < n; i++) {
            uint32_t chave = (uint32_t)v[i] - base;
            hist[0][chave & RADIX_MASCARA]++;
            hist[1][(chave >> RADIX_BITS) & RADIX_MASCARA]++;
        }
    } else {
        for (size_t i = 0; i < n; i++) {
            uint32_t chave = (uint32_t)v[i] - base;
            hist[0][chave & RADIX_MASCARA]++;
            hist[1][(chave >> RADIX_BITS) & RADIX_MASCARA]++;
            hist[2][(chave >> (2u * RADIX_BITS)) & RADIX_MASCARA]++;
        }
    }

    int32_t *origem = v;
    int32_t *destino = auxiliar;

    for (unsigned int p = 0; p < numPasses; p++) {
        uint32_t *contagem = hist[p];
        const unsigned int shift = p * RADIX_BITS;

        /* Pula o passe se todos os elementos tiverem o mesmo digito */
        uint32_t primeiroDigito = (((uint32_t)origem[0] - base) >> shift) & RADIX_MASCARA;
        if (contagem[primeiroDigito] == n)
            continue;

        /* Soma de prefixos (offset de cada balde) */
        uint32_t acum = 0;
        for (unsigned int b = 0; b < RADIX_BALDES; b++) {
            uint32_t freq = contagem[b];
            contagem[b] = acum;
            acum += freq;
        }

        /* Distribuicao estavel dos elementos */
        size_t i = 0;
        for (; i + 4 <= n; i += 4) {
            int32_t x0 = origem[i],     x1 = origem[i + 1];
            int32_t x2 = origem[i + 2], x3 = origem[i + 3];
            uint32_t d0 = (((uint32_t)x0 - base) >> shift) & RADIX_MASCARA;
            uint32_t d1 = (((uint32_t)x1 - base) >> shift) & RADIX_MASCARA;
            uint32_t d2 = (((uint32_t)x2 - base) >> shift) & RADIX_MASCARA;
            uint32_t d3 = (((uint32_t)x3 - base) >> shift) & RADIX_MASCARA;
            destino[contagem[d0]++] = x0;
            destino[contagem[d1]++] = x1;
            destino[contagem[d2]++] = x2;
            destino[contagem[d3]++] = x3;
        }
        for (; i < n; i++) {
            int32_t x = origem[i];
            uint32_t d = (((uint32_t)x - base) >> shift) & RADIX_MASCARA;
            destino[contagem[d]++] = x;
        }

        int32_t *temp = origem;
        origem = destino;
        destino = temp;
    }

    if (origem != v)
        memcpy(v, origem, n * sizeof(int32_t));

    free(auxiliar);
    return true;
}

/*
 * Funcao principal da estrategia Hibrida Adaptativa.
 * Toda a analise, escolha da estrategia, alocacao de estruturas auxiliares e
 * ordenacao acontecem aqui dentro (medidas pelo clock()).
 */
static bool ordenarVetor(int32_t * RESTRITO vetor,
                         size_t quantidade,
                         TipoEstrategia * RESTRITO estrategiaUsada)
{
    if (quantidade <= 1) {
        if (estrategiaUsada != NULL)
            *estrategiaUsada = ESTRATEGIA_JA_ORDENADO;
        return true;
    }

    /* 1. Amostragem rapida para verificar se cabe no Counting Sort em passe unico */
    CaracteristicasLista amostra = amostrarCaracteristicas(vetor, quantidade);
    TipoEstrategia estrategia = escolherEstrategia(quantidade, &amostra);

    if (estrategia == ESTRATEGIA_JA_ORDENADO && amostra.limitesExatos) {
        if (estrategiaUsada != NULL)
            *estrategiaUsada = ESTRATEGIA_JA_ORDENADO;
        return true;
    }

    if (estrategia == ESTRATEGIA_COUNTING_SORT) {
        if (!amostra.limitesExatos) {
            if (countingSortPasseUnico(vetor, quantidade, amostra.minimo, (size_t)amostra.faixa)) {
                if (estrategiaUsada != NULL)
                    *estrategiaUsada = ESTRATEGIA_COUNTING_SORT;
                return true;
            }
        } else {
            if (countingSortExato(vetor, quantidade, amostra.minimo, (size_t)amostra.faixa)) {
                if (estrategiaUsada != NULL)
                    *estrategiaUsada = ESTRATEGIA_COUNTING_SORT;
                return true;
            }
        }
    }

    /* 2. Analise completa exata (caso a faixa seja ampla ou tenha escapado da amostra) */
    CaracteristicasLista exata = amostra.limitesExatos
                                     ? amostra
                                     : analisarListaCompleta(vetor, quantidade);

    estrategia = escolherEstrategia(quantidade, &exata);
    if (estrategiaUsada != NULL)
        *estrategiaUsada = estrategia;

    if (estrategia == ESTRATEGIA_JA_ORDENADO)
        return true;

    if (estrategia == ESTRATEGIA_COUNTING_SORT)
        return countingSortExato(vetor, quantidade, exata.minimo, (size_t)exata.faixa);

    /* 3. Caso geral de faixa ampla: Radix Sort LSD */
    return radixSortLSD(vetor, quantidade, exata.minimo, exata.maximo);
}

/* -------------------------------------------------------------------------- */
/* 4. Validacao e Verificacao de Corretude (fora da medicao de clock)         */
/* -------------------------------------------------------------------------- */

/*
 * Calcula uma assinatura algebrica e bit a bit (quantidade, soma, soma dos
 * quadrados e XOR acumulado em 64 bits) invariante a permutacao dos elementos.
 */
static AssinaturaDados calcularAssinatura(const int32_t * RESTRITO v, size_t n)
{
    AssinaturaDados a = {n, 0ULL, 0ULL, 0ULL};
    for (size_t i = 0; i < n; i++) {
        uint64_t x = (uint64_t)(int64_t)v[i];
        a.soma += x;
        a.somaQuadrados += x * x;
        a.xorAcumulado ^= x;
    }
    return a;
}

/*
 * Verifica independentemente:
 *   1. Se o vetor final esta em ordem crescente (nao decrescente: v[i-1] <= v[i]).
 *   2. Se a assinatura (quantidade, soma, soma dos quadrados e XOR) coincide
 *      com a do vetor original antes da ordenacao (sem perda ou adulteracao).
 *   3. Se existem elementos repetidos preservados no resultado.
 */
static ResultadoValidacao validarOrdenacao(const int32_t * RESTRITO v,
                                           size_t n,
                                           const AssinaturaDados *antes)
{
    ResultadoValidacao res = {true, true, false, 0};
    AssinaturaDados depois = calcularAssinatura(v, n);

    if (antes->quantidade != depois.quantidade ||
        antes->soma != depois.soma ||
        antes->somaQuadrados != depois.somaQuadrados ||
        antes->xorAcumulado != depois.xorAcumulado) {
        res.integridadePreservada = false;
    }

    size_t repetidos = 0;
    for (size_t i = 1; i < n; i++) {
        if (v[i - 1] > v[i]) {
            res.ordenadoCrescente = false;
        } else if (v[i - 1] == v[i]) {
            repetidos++;
        }
    }

    res.possuiRepetidos = (repetidos > 0);
    res.quantidadeRepetidos = repetidos;
    return res;
}

/* -------------------------------------------------------------------------- */
/* 5. Escrita Opcional do Resultado em Disco                                  */
/* -------------------------------------------------------------------------- */

static bool gravarArquivoOrdenado(const char *caminho, const int32_t * RESTRITO v, size_t n)
{
    unsigned char *buffer = (unsigned char *)malloc(TAM_BUFFER_ESCRITA);
    if (buffer == NULL)
        return false;

    FILE *arquivo = fopen(caminho, "wb");
    if (arquivo == NULL) {
        free(buffer);
        return false;
    }

    size_t usado = 0;
    bool ok = true;

    for (size_t i = 0; i < n && ok; i++) {
        if (usado + 16 > TAM_BUFFER_ESCRITA) {
            ok = (fwrite(buffer, 1, usado, arquivo) == usado);
            usado = 0;
        }
        int32_t x = v[i];
        uint32_t mag = (x < 0) ? (0u - (uint32_t)x) : (uint32_t)x;
        unsigned char digitos[12];
        size_t d = 0;
        do {
            digitos[d++] = (unsigned char)('0' + (mag % 10u));
            mag /= 10u;
        } while (mag != 0u);

        if (x < 0)
            buffer[usado++] = '-';
        while (d > 0)
            buffer[usado++] = digitos[--d];
        buffer[usado++] = '\n';
    }

    if (ok && usado > 0)
        ok = (fwrite(buffer, 1, usado, arquivo) == usado);

    if (fclose(arquivo) != 0)
        ok = false;

    free(buffer);
    return ok;
}

static const char *nomeEstrategia(TipoEstrategia e)
{
    switch (e) {
    case ESTRATEGIA_JA_ORDENADO:
        return "Deteccao de Lista Ja Ordenada O(N)";
    case ESTRATEGIA_COUNTING_SORT:
        return "Hibrido Adaptativo -> Counting Sort";
    case ESTRATEGIA_RADIX_SORT_LSD:
        return "Hibrido Adaptativo -> Radix Sort LSD";
    default:
        return "Desconhecido";
    }
}

/* -------------------------------------------------------------------------- */
/* Programa Principal                                                         */
/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    const char *caminhoEntrada = (argc > 1) ? argv[1] : ARQUIVO_PADRAO;
    const char *caminhoSaida   = (argc > 2) ? argv[2] : NULL;

    /* 1. Leitura do arquivo (ANTES da medicao de tempo) */
    size_t quantidade = 0;
    int32_t *vetor = lerArquivoNumeros(caminhoEntrada, &quantidade);
    if (vetor == NULL)
        return 1;

    /* Assinatura previa para validacao de integridade (ANTES da medicao) */
    AssinaturaDados assinaturaAntes = calcularAssinatura(vetor, quantidade);
    TipoEstrategia estrategia = ESTRATEGIA_COUNTING_SORT;

    /* 2. Medicao EXCLUSIVA da estrategia de ordenacao com clock() */
    clock_t inicio = clock();

    bool sucesso = ordenarVetor(vetor, quantidade, &estrategia);

    clock_t fim = clock();

    if (!sucesso) {
        fprintf(stderr, "Erro: falha de alocacao de memoria durante a ordenacao.\n");
        free(vetor);
        return 1;
    }

    double tempo = ((double)(fim - inicio)) / CLOCKS_PER_SEC;

    /* 3. Validacao independente (DEPOIS da medicao de tempo) */
    ResultadoValidacao validacao = validarOrdenacao(vetor, quantidade, &assinaturaAntes);

    printf("============================================================\n");
    printf("  COMPETICAO DE ORDENACAO: O DESAFIO DOS MILISSEGUNDOS\n");
    printf("============================================================\n");
    printf("Arquivo de entrada : %s\n", caminhoEntrada);
    printf("Elementos lidos    : %zu\n", quantidade);
    printf("Algoritmo aplicado : %s\n", nomeEstrategia(estrategia));
    printf("Tempo de ordenação : %.6f segundos (%.3f ms)\n", tempo, tempo * 1000.0);
    printf("------------------------------------------------------------\n");
    printf("Ordem crescente    : %s\n", validacao.ordenadoCrescente ? "OK (Confirmada)" : "FALHOU");
    printf("Integridade dados  : %s (zero perdas/adulteracoes)\n",
           validacao.integridadePreservada ? "OK" : "FALHOU");
    if (validacao.possuiRepetidos) {
        printf("Valores repetidos  : SIM (%zu ocorrencias adjacentes preservadas)\n",
               validacao.quantidadeRepetidos);
    } else {
        printf("Valores repetidos  : NAO (sequencia estritamente crescente)\n");
    }
    printf("Verificação final  : %s\n",
           (validacao.ordenadoCrescente && validacao.integridadePreservada) ? "OK" : "ERRO");
    printf("============================================================\n");

    if (!validacao.ordenadoCrescente || !validacao.integridadePreservada) {
        free(vetor);
        return 1;
    }

    if (caminhoSaida != NULL) {
        if (!gravarArquivoOrdenado(caminhoSaida, vetor, quantidade)) {
            fprintf(stderr, "Erro ao gravar arquivo de saida '%s'.\n", caminhoSaida);
            free(vetor);
            return 1;
        }
        printf("Arquivo ordenado gravado em: %s\n", caminhoSaida);
    }

    free(vetor);
    return 0;
}
