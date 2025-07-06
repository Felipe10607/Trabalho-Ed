#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <time.h>


#define SEED 0x12345678
#define DEFAULT_LOAD_FACTOR_THRESHOLD 0.7 // 70% de ocupação para redimensionamento

//Aluno : Felipe Eduardo F.P.Lupoli
//RGA : 202319040630



// --- Tipos de Sondagem ---
typedef enum {
    LINEAR_PROBING,
    DOUBLE_HASHING
} ProbingType;

// --- Estrutura da Tabela Hash ---
typedef struct {
    uintptr_t *table;
    int size; 
    int max; 
    uintptr_t deleted;
    char *(*get_key)(void *);
    ProbingType probing_type; // Tipo de sondagem (linear ou dupla)
    float load_factor_threshold; 
} thash;

// --- Estrutura para Dados de CEP ---
typedef struct {
    char cep_prefix[6]; // Primeiros 5 dígitos do CEP + '\0'
    char cidade[50];    // Nome da cidade
    char estado[3];     // Sigla do estado (ex: "SP", "RJ")
} tcep_data;

// --- Funções de Hash ---
// Função de hash principal (Murmur hash)
uint32_t hashf(const char *str, uint32_t h) {
    for (; *str; ++str) {
        h ^= *str;
        h *= 0x5bd1e995;
        h ^= h >> 15;
    }
    return h;
}
// Função de hash secundária para Double Hashing
uint32_t hashf2(const char *str, int max_buckets) {
    uint32_t h1_val = hashf(str, SEED);
    if ((max_buckets - 1) <= 1) {
        return 1;
    }
    return 1 + (h1_val % (max_buckets - 1));
}


// --- Funções Auxiliares para CEPs ---
// Função para obter a chave (prefixo do CEP) de um dado tcep_data
char *get_cep_key(void *reg) {
    return ((tcep_data *)reg)->cep_prefix;
}

// Função para alocar e inicializar um novo tcep_data
tcep_data *aloca_cep_data(const char *cep_prefix, const char *cidade, const char *estado) {
    tcep_data *data = malloc(sizeof(tcep_data));
    if (!data) {
        perror("Erro ao alocar tcep_data");
        exit(EXIT_FAILURE);
    }
    strncpy(data->cep_prefix, cep_prefix, 5);
    data->cep_prefix[5] = '\0'; 
    strncpy(data->cidade, cidade, sizeof(data->cidade) - 1);
    data->cidade[sizeof(data->cidade) - 1] = '\0';
    strncpy(data->estado, estado, sizeof(data->estado) - 1);
    data->estado[sizeof(data->estado) - 1] = '\0';
    return data;
}

// --- Função de Redimensionamento  ---
int hash_resize(thash *h) {
    int old_max = h->max;
    uintptr_t *old_table = h->table;
    int new_max = (old_max - 1) * 2 + 1;
    h->table = calloc(sizeof(void *), new_max);
    if (!h->table) {
        perror("Erro ao redimensionar a tabela hash");
        h->table = old_table;
        return EXIT_FAILURE;
    }
    
    h->max = new_max;
    h->size = 0; 

    // Re-inserir todos os elementos da tabela antiga na nova tabela
    for (int i = 0; i < old_max; i++) {
        if (old_table[i] != 0 && old_table[i] != h->deleted) {
            uint32_t hash = hashf(h->get_key((void *)old_table[i]), SEED);
            int pos = hash % (h->max);
            int step = 1;
            if (h->probing_type == DOUBLE_HASHING) {
                step = hashf2(h->get_key((void *)old_table[i]), h->max);
                if (step == 0) step = 1;
            }

            while (h->table[pos] != 0) {
                pos = (pos + step) % h->max;
            }
            h->table[pos] = old_table[i];
            h->size++; 
        }
    }
    free(old_table);
    printf("Tabela redimensionada de %d para %d buckets. Elementos re-inseridos: %d. Nova ocupacao: %.2f%%\n", old_max -1, h->max -1, h->size, (float)h->size / (h->max -1) * 100);
    return EXIT_SUCCESS;
}

// --- Funções da Tabela Hash Modificadas  ---
int hash_insere(thash *h, void *bucket) {
    // Verifica a taxa de ocupação antes de inserir e redimensiona se necessário
    if ((float)(h->size + 1) / (h->max - 1) >= h->load_factor_threshold) {
        if (hash_resize(h) != EXIT_SUCCESS) {
            free(bucket); // Se redimensionamento falhar, libera o bucket original
            return EXIT_FAILURE;
        }
    }

    uint32_t hash = hashf(h->get_key(bucket), SEED);
    int pos = hash % (h->max);
    int step = 1; // Padrão para Linear Probing

    if (h->probing_type == DOUBLE_HASHING) {
        step = hashf2(h->get_key(bucket), h->max);
        if (step == 0) step = 1; // Garante que o passo nunca seja zero
    }

    // Busca a próxima posição disponível (slot vazio ou 'deleted')
    while ((h->table[pos]) != 0 && (h->table[pos]) != h->deleted) {
        pos = (pos + step) % h->max;
    }

    h->table[pos] = (uintptr_t)bucket;
    h->size += 1;
    return EXIT_SUCCESS;
}

int hash_constroi(thash *h, int nbuckets, char *(*get_key)(void *), ProbingType p_type, float load_factor_threshold) {
    h->table = calloc(sizeof(void *), nbuckets + 1);
    if (!h->table) return EXIT_FAILURE;
    h->max = nbuckets + 1;
    h->size = 0;
    h->deleted = (uintptr_t)&(h->size);
    h->get_key = get_key;
    h->probing_type = p_type; // Define o tipo de sondagem
    h->load_factor_threshold = load_factor_threshold; // Define o limiar de ocupação
    return EXIT_SUCCESS;
}

void *hash_busca(thash h, const char *key) {
    uint32_t hash = hashf(key, SEED);
    int pos = hash % h.max;
    int step = 1; 

    if (h.probing_type == DOUBLE_HASHING) { 
        step = hashf2(key, h.max);
        if (step == 0) step = 1; 
    }

    while (h.table[pos] != 0) { 
        if (h.table[pos] != h.deleted && strcmp(h.get_key((void *)h.table[pos]), key) == 0) {
            return (void *)h.table[pos]; 
        }
        pos = (pos + step) % h.max; 
    }
    return NULL;
}
int hash_remove(thash *h, const char *key) {
    uint32_t hash = hashf(key, SEED);
    int pos = hash % h->max;
    int step = 1;
    if (h->probing_type == DOUBLE_HASHING) {
        step = hashf2(key, h->max);
        if (step == 0) step = 1;
    }

    while (h->table[pos] != 0) {
        if (h->table[pos] != h->deleted && strcmp(h->get_key((void *)h->table[pos]), key) == 0) {
            free((void *)h->table[pos]);
            h->table[pos] = h->deleted;
            h->size--;
            return EXIT_SUCCESS;
        }
        pos = (pos + step) % h->max;
    }
    return EXIT_FAILURE;
}

// --- Apaga ---
void hash_apaga(thash *h) {
    if (h == NULL) {
        return;
    }
    if (h->table == NULL) {
        h->size = 0;
        h->max = 0;
        return;
    }

    for (int i = 0; i < h->max; i++) {
        if (h->table[i] != 0 && h->table[i] != h->deleted) {
            free((void *)h->table[i]); // Libera cada bucket que contém dados reais
        }
    }
    free(h->table); // Libera o array da tabela em si
    h->table = NULL; // Zera o ponteiro para evitar double free futuros
    h->max = 0;      //o tamanho máximo
    h->size = 0;     //o contador de elementos
}

// --- Funções para leitura do arquivo CSV  ---
#define MAX_LINE_LENGTH 256
#define MAX_FIELD_LENGTH 100

// Função para carregar dados de CEP
int load_ceps_from_csv(thash *h, const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Erro ao abrir o arquivo CSV de CEPs");
        return EXIT_FAILURE;
    }

    char line[MAX_LINE_LENGTH];
    if (fgets(line, sizeof(line), file) == NULL) {
        fclose(file);
        return EXIT_FAILURE; // Arquivo vazio ou erro de leitura
    }

    int count = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token;
        char *rest = line;
        // 1. Pega "Estado"
        token = strtok(rest, ";"); 
        if (!token) continue;
        char uf_temp[3];
        strncpy(uf_temp, token, sizeof(uf_temp) - 1);
        uf_temp[sizeof(uf_temp) - 1] = '\0';

        // 2. Pega "Localidade"
        token = strtok(NULL, ";");
        if (!token) continue;
        char cidade_temp[MAX_FIELD_LENGTH]; // Variável temporária para Cidade
        strncpy(cidade_temp, token, sizeof(cidade_temp) - 1);
        cidade_temp[sizeof(cidade_temp) - 1] = '\0';
        // 3. Pega "Faixa de CEP"
        token = strtok(NULL, ";");
        if (!token) continue;
        char cep_prefix_temp[6]; // Variável temporária para o prefixo do CEP
        strncpy(cep_prefix_temp, token, 5); 
        cep_prefix_temp[5] = '\0'; // Garante terminação nula
        // Aloca e insere os dados na tabela hash
        tcep_data *data = aloca_cep_data(cep_prefix_temp, cidade_temp, uf_temp);
        if (hash_insere(h, data) != EXIT_SUCCESS) {
            fprintf(stderr, "Falha ao inserir CEP %s. Tabela cheia ou erro de redimensionamento.\n", cep_prefix_temp);
        } else {
            count++;
        }
    }

    fclose(file);
    return count;
}

// --- Funções de Comparativos  ---

// Função para popular a tabela até uma certa taxa de ocupação para testes de busca
char** populate_for_search_test(thash *h, int total_buckets, float occupation_rate, const char *prefix, ProbingType p_type) {
    int num_elements_to_insert = (int)(total_buckets * occupation_rate);
    if (num_elements_to_insert == 0) return NULL;
    hash_apaga(h);
    // Load factor alto para evitar resize durante o teste de populacao
    hash_constroi(h, total_buckets, get_cep_key, p_type, 2.0); 

    char** inserted_keys = malloc(sizeof(char*) * num_elements_to_insert);
    if (!inserted_keys) {
        perror("Erro ao alocar array de chaves para teste de busca");
        hash_apaga(h); // Libera a tabela hash recém-construída
        return NULL;
    }

    printf("Populando tabela para busca (%s Probing) com %d elementos (%.2f%% de ocupacao)...\n", 
           (p_type == LINEAR_PROBING ? "Linear" : "Double"), num_elements_to_insert, occupation_rate * 100);
    
    char temp_cep[6];
    for (int i = 0; i < num_elements_to_insert; ++i) {
        // Gera CEPs fictícios para o teste, garantindo unicidade para o prefixo dado
        snprintf(temp_cep, sizeof(temp_cep), "%s%03d", prefix, i);
        tcep_data *data = aloca_cep_data(temp_cep, "Cidade Teste", "TS");
        if (hash_insere(h, data) != EXIT_SUCCESS) {
            fprintf(stderr, "Falha na insercao de dados ficticios para teste de busca na iteracao %d.\n", i);
            free(data); 
            for(int j=0; j<i; ++j) free(inserted_keys[j]);
            free(inserted_keys);
            hash_apaga(h);
            return NULL;
        } else {
            inserted_keys[i] = strdup(temp_cep); // Armazena a chave para buscar depois
            if (inserted_keys[i] == NULL) { // Verificar strdup
                fprintf(stderr, "Erro ao duplicar string para inserted_keys na iteracao %d.\n", i);
                for(int j=0; j<i; ++j) free(inserted_keys[j]);
                free(inserted_keys);
                hash_apaga(h);
                return NULL;
            }
        }
    }
    printf("Populacao para busca completa. Tamanho da hash: %d/%d\n", h->size, h->max -1);
    return inserted_keys;
}


// Função de busca que será perfilada pelo gprof
void perform_search_test(thash *h, char** keys_to_search, int num_keys_to_search, int occupation_percent, char* probing_name) {
    if (num_keys_to_search == 0 || keys_to_search == NULL) {
        printf("Nenhuma chave para buscar em %s Hash com %d%% de ocupacao.\n", probing_name, occupation_percent);
        return;
    }
    printf("Iniciando busca em %s Hash com %d%% de ocupacao (buscando %d chaves)...\n", probing_name, occupation_percent, num_keys_to_search);
    volatile tcep_data *found_data;
    for (int i = 0; i < num_keys_to_search; ++i) {
        if (keys_to_search[i]) {
            found_data = hash_busca(*h, keys_to_search[i]);
        }
    }
    printf("Busca em %s Hash com %d%% de ocupacao concluida.\n", probing_name, occupation_percent);
}

// Função para os testes de inserção
void perform_insertion_test(int initial_buckets, ProbingType p_type, float load_factor_threshold, const char* filename) {
    printf("Iniciando insercao de CEPs com %d buckets iniciais (%s Probing, threshold %.2f)...\n", 
           initial_buckets, (p_type == LINEAR_PROBING ? "Linear" : "Double"), load_factor_threshold);
    thash h;
    h.table = NULL; 
    hash_constroi(&h, initial_buckets, get_cep_key, p_type, load_factor_threshold);
    int inserted_count = load_ceps_from_csv(&h, filename);
    printf("Insercao de %d CEPs concluida. Tamanho final da hash: %d/%d\n", inserted_count, h.size, h.max -1);
    hash_apaga(&h);
}


// --- Função Principal (Main) para Execução e Testes ---
int main(int argc, char *argv[]) {
    srand(time(NULL)); // Inicializa o gerador de números aleatórios para keys fictícias

    printf("--- Testes Basicos e Funcionais ---\n");
    thash h_basic_test;
    h_basic_test.table = NULL; 
    hash_constroi(&h_basic_test, 5, get_cep_key, LINEAR_PROBING, 0.5); // Redimensiona com 50% de ocupação
    assert(hash_insere(&h_basic_test, aloca_cep_data("12345", "Cidade A", "AA")) == EXIT_SUCCESS); // Size 1/5 = 20%
    assert(hash_insere(&h_basic_test, aloca_cep_data("12346", "Cidade B", "BB")) == EXIT_SUCCESS); // Size 2/5 = 40%
    printf("Inserindo terceiro elemento, deve redimensionar...\n");
    assert(hash_insere(&h_basic_test, aloca_cep_data("12347", "Cidade C", "CC")) == EXIT_SUCCESS); // Size 3/5 = 60%
    printf("Size: %d, Max: %d\n", h_basic_test.size, h_basic_test.max - 1);
    tcep_data *found_data = hash_busca(h_basic_test, "12345");
    assert(found_data != NULL && strcmp(found_data->cidade, "Cidade A") == 0);
    found_data = hash_busca(h_basic_test, "12347");
    assert(found_data != NULL && strcmp(found_data->cidade, "Cidade C") == 0);
    assert(hash_remove(&h_basic_test, "12346") == EXIT_SUCCESS);
    found_data = hash_busca(h_basic_test, "12346");
    assert(found_data == NULL);
    hash_apaga(&h_basic_test);
    printf("Testes basicos concluidos com sucesso.\n\n");

    // --- Comparativo de Tempo de Busca por Taxa de Ocupação ---
    printf("--- Comparativo de Tempo de Busca por Taxa de Ocupacao ---\n");
    int total_buckets_search_test = 6100;
    float occupation_rates[] = {0.10, 0.20, 0.30, 0.40, 0.50, 0.60, 0.70, 0.80, 0.90, 0.99};
    int num_rates = sizeof(occupation_rates) / sizeof(occupation_rates[0]);

    // Loop para Hash Simples (Linear Probing)
    printf("\n>>> Testes de Busca com HASH SIMPLES (Linear Probing) <<<\n");
    thash h_linear_search;
    h_linear_search.table = NULL; 
    for (int i = 0; i < num_rates; ++i) {
        char** inserted_keys = populate_for_search_test(&h_linear_search, total_buckets_search_test, occupation_rates[i], "L%02d", LINEAR_PROBING);
        if (inserted_keys) {
            perform_search_test(&h_linear_search, inserted_keys, h_linear_search.size, (int)(occupation_rates[i] * 100), "Linear");
            for(int j=0; j < h_linear_search.size; ++j) { // Apenas libera as chaves que foram de fato inseridas
                free(inserted_keys[j]);
            }
            free(inserted_keys);
        }
        hash_apaga(&h_linear_search);
    }

    // Loop para Hash Duplo (Double Hashing)
    printf("\n>>> Testes de Busca com HASH DUPLO (Double Hashing) <<<\n");
    thash h_double_search;
    h_double_search.table = NULL; 
    for (int i = 0; i < num_rates; ++i) {
        char** inserted_keys = populate_for_search_test(&h_double_search, total_buckets_search_test, occupation_rates[i], "D%02d", DOUBLE_HASHING);
        if (inserted_keys) {
            perform_search_test(&h_double_search, inserted_keys, h_double_search.size, (int)(occupation_rates[i] * 100), "Double");
            for(int j=0; j < h_double_search.size; ++j) {
                free(inserted_keys[j]);
            }
            free(inserted_keys);
        }
        hash_apaga(&h_double_search);
    }
    printf("\n--- Fim ---\n\n");


    // ---Comparativo de Tempo de Inserção com Redimensionamento ---
    printf("--- Comparativo de Tempo de Insercao com Redimensionamento ---\n");
    const char *cep_filename = "ceps.csv";
    printf("\n>>> Teste de Insercao com 6100 Buckets Iniciais (Linear Probing) <<<\n");
    perform_insertion_test(6100, LINEAR_PROBING, DEFAULT_LOAD_FACTOR_THRESHOLD, cep_filename);

    printf("\n>>> Teste de Insercao com 6100 Buckets Iniciais (Double Hashing) <<<\n");
    perform_insertion_test(6100, DOUBLE_HASHING, DEFAULT_LOAD_FACTOR_THRESHOLD, cep_filename);

    printf("\n>>> Teste de Insercao com 1000 Buckets Iniciais (Linear Probing) <<<\n");
    perform_insertion_test(1000, LINEAR_PROBING, DEFAULT_LOAD_FACTOR_THRESHOLD, cep_filename);

    printf("\n>>> Teste de Insercao com 1000 Buckets Iniciais (Double Hashing) <<<\n");
    perform_insertion_test(1000, DOUBLE_HASHING, DEFAULT_LOAD_FACTOR_THRESHOLD, cep_filename);
    printf("\n--- FIM Comparativo de Tempo de Insercao com Redimensionamento ---\n");

    return 0;
}
