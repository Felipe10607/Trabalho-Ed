#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
#include <assert.h>

#define EMBEDDING_DIM 128
#define MAX_PERSON_ID_LEN 100

// Representa um ponto no espaço KD com dados associados
typedef struct _reg {
    double lat;
    double lon;
    float embedding[EMBEDDING_DIM];
    char person_id[MAX_PERSON_ID_LEN];
} treg;

// Aloca e inicializa uma nova estrutura treg
void *aloca_reg(double lat, double lon, float embedding[], const char person_id[]) {
    treg *reg = malloc(sizeof(treg));
    if (!reg) {
        perror("Memory allocation failed for treg");
        exit(EXIT_FAILURE);
    }
    reg->lat = lat;
    reg->lon = lon;
    memcpy(reg->embedding, embedding, EMBEDDING_DIM * sizeof(float));
    strncpy(reg->person_id, person_id, MAX_PERSON_ID_LEN - 1);
    reg->person_id[MAX_PERSON_ID_LEN - 1] = '\0';
    return reg;
}

// Comparador para os eixos da KD-Tre
int comparador(void *a, void *b, int pos) {
    double diff;
    if (pos == 0) diff = ((treg *)a)->lat - ((treg *)b)->lat;
    else if (pos == 1) diff = ((treg *)a)->lon - ((treg *)b)->lon;
    else return 0; // Invalid axis

    return (diff > 0) ? 1 : (diff < 0) ? -1 : 0;
}

// Distância euclidiana quadrada para coordenadas
double distancia_kdtree_coord(void *a, void *b) {
    double d_lat = ((treg *)a)->lat - ((treg *)b)->lat;
    double d_lon = ((treg *)a)->lon - ((treg *)b)->lon;
    return d_lat * d_lat + d_lon * d_lon;
}

// Distância euclidiana
double distancia_embedding(float *emb1, float *emb2) {
    double sum_sq_diff = 0.0;
    for (int i = 0; i < EMBEDDING_DIM; ++i) {
        double diff = emb1[i] - emb2[i];
        sum_sq_diff += diff * diff;
    }
    return sum_sq_diff;
}

typedef struct _node {
    void *key;
    struct _node *esq;
    struct _node *dir;
} tnode;

// Estrutura da KD-Tree
typedef struct _arv {
    tnode *raiz;
    int (*cmp)(void *, void *, int); // Comparador de eixo
    double (*dist)(void *, void *);  // Função de distância para busca
    int k; // Dimensões da árvore (2 para lat/lon)
} tarv;

typedef struct _heap_element {
    double distance;
    treg *data;
} heap_element;

// Estrutura do Max-Heap para N vizinhos mais próximos
typedef struct _max_heap {
    heap_element *elements;
    int capacity;
    int size;
} max_heap;

max_heap *create_max_heap(int capacity) {
    max_heap *heap = malloc(sizeof(max_heap));
    if (!heap) { perror("Heap alloc failed"); exit(EXIT_FAILURE); }
    heap->elements = malloc(sizeof(heap_element) * capacity);
    if (!heap->elements) { perror("Heap elements alloc failed"); free(heap); exit(EXIT_FAILURE); }
    heap->capacity = capacity;
    heap->size = 0;
    return heap;
}

void destroy_max_heap(max_heap *heap) {
    if (heap) {
        free(heap->elements);
        free(heap);
    }
}

void swap_heap_elements(heap_element *a, heap_element *b) {
    heap_element temp = *a;
    *a = *b;
    *b = temp;
}

void heapify_up(max_heap *heap, int index) {
    int parent = (index - 1) / 2;
    while (index > 0 && heap->elements[index].distance > heap->elements[parent].distance) {
        swap_heap_elements(&heap->elements[index], &heap->elements[parent]);
        index = parent;
        parent = (index - 1) / 2;
    }
}

void heapify_down(max_heap *heap, int index) {
    int largest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;

    if (left < heap->size && heap->elements[left].distance > heap->elements[largest].distance) largest = left;
    if (right < heap->size && heap->elements[right].distance > heap->elements[largest].distance) largest = right;

    if (largest != index) {
        swap_heap_elements(&heap->elements[index], &heap->elements[largest]);
        heapify_down(heap, largest);
    }
}

// Insere elemento no max-heap
void insert_into_max_heap(max_heap *heap, double distance, treg *data) {
    if (heap->size < heap->capacity) {
        heap->elements[heap->size].distance = distance;
        heap->elements[heap->size].data = data;
        heapify_up(heap, heap->size);
        heap->size++;
    } else if (distance < heap->elements[0].distance) {
        heap->elements[0].distance = distance;
        heap->elements[0].data = data;
        heapify_down(heap, 0);
    }
}

void kdtree_constroi(tarv *arv, int (*cmp)(void *a, void *b, int), double (*dist)(void *, void *), int k) {
    arv->raiz = NULL;
    arv->cmp = cmp;
    arv->dist = dist;
    arv->k = k;
}

void _kdtree_insere(tnode **raiz, void *key, int (*cmp)(void *a, void *b, int), int profund, int k) {
    if (!*raiz) {
        *raiz = malloc(sizeof(tnode));
        if (!*raiz) { perror("Node alloc failed"); exit(EXIT_FAILURE); }
        (*raiz)->key = key;
        (*raiz)->esq = NULL;
        (*raiz)->dir = NULL;
    } else {
        int pos = profund % k;
        if (cmp((*raiz)->key, key, pos) < 0) _kdtree_insere(&((*raiz)->dir), key, cmp, profund + 1, k);
        else _kdtree_insere(&((*raiz)->esq), key, cmp, profund + 1, k);
    }
}

// Insere um ponto na KD-Tree
void kdtree_insere(tarv *arv, void *key) {
    _kdtree_insere(&(arv->raiz), key, arv->cmp, 0, arv->k);
}

void _kdtree_destroi(tnode *node) {
    if (node) {
        _kdtree_destroi(node->esq);
        _kdtree_destroi(node->dir);
        free(node->key);
        free(node);
    }
}

void kdtree_destroi(tarv *arv) {
    _kdtree_destroi(arv->raiz);
}

// Busca recursiva por N vizinhos mais próximos, utilizando um max-heap para manter os resultados
void _kdtree_busca_n_nearest(tarv *arv, tnode *atual, void *key_query, int profund, max_heap *heap_results, int N) {
    if (!atual) return;

    double dist_atual = arv->dist(atual->key, key_query);
    if (heap_results->size < N || dist_atual < heap_results->elements[0].distance) {
        insert_into_max_heap(heap_results, dist_atual, (treg *)atual->key);
    }

    int pos = profund % arv->k;
    int comp = arv->cmp(key_query, atual->key, pos);

    tnode *lado_principal = (comp < 0) ? atual->esq : atual->dir;
    tnode *lado_oposto = (comp < 0) ? atual->dir : atual->esq;

    _kdtree_busca_n_nearest(arv, lado_principal, key_query, profund + 1, heap_results, N);

    double dist_to_hyperplane_sq;
    if (pos == 0) dist_to_hyperplane_sq = (((treg *)key_query)->lat - ((treg *)atual->key)->lat);
    else dist_to_hyperplane_sq = (((treg *)key_query)->lon - ((treg *)atual->key)->lon);
    dist_to_hyperplane_sq *= dist_to_hyperplane_sq;

    if (heap_results->size < N || dist_to_hyperplane_sq < heap_results->elements[0].distance) {
        _kdtree_busca_n_nearest(arv, lado_oposto, key_query, profund + 1, heap_results, N);
    }
}

typedef struct _treg_array {
    treg *elements;
    int size;
} treg_array;

// Função pública para buscar N vizinhos mais próximos
treg_array buscar_n_mais_proximos(tarv *arv, treg query, int n_neighbors) {
    max_heap *results_heap = create_max_heap(n_neighbors);
    
    _kdtree_busca_n_nearest(arv, arv->raiz, &query, 0, results_heap, n_neighbors);

    treg_array final_results;
    final_results.size = results_heap->size;
    final_results.elements = malloc(sizeof(treg) * final_results.size);
    if (!final_results.elements) {
        perror("Results array alloc failed");
        destroy_max_heap(results_heap);
        final_results.size = 0;
        return final_results;
    }

    for (int i = 0; i < final_results.size; ++i) {
        final_results.elements[i] = *(results_heap->elements[i].data);
    }
    
    destroy_max_heap(results_heap);
    return final_results;
}

// Libera memória alocada para o array de resultados
void free_treg_array(treg_array arr) {
    if (arr.elements) free(arr.elements);
}

// Árvore global
tarv arvore_global;

tarv* get_tree() {
    return &arvore_global;
}

void inserir_ponto(double lat, double lon, float embedding[EMBEDDING_DIM], const char person_id[MAX_PERSON_ID_LEN]) {
    treg *novo_reg = aloca_reg(lat, lon, embedding, person_id);
    kdtree_insere(&arvore_global, novo_reg);
}

void kdtree_construir() {
    arvore_global.k = 2;
    arvore_global.dist = distancia_kdtree_coord;
    arvore_global.cmp = comparador;
    arvore_global.raiz = NULL;
}

/* Testes */
void test_constroi(){
    tarv arv;
    float dummy_emb[EMBEDDING_DIM];
    for(int i=0; i<EMBEDDING_DIM; ++i) dummy_emb[i] = (float)i/100.0;

    char nome1[MAX_PERSON_ID_LEN] = "Dourados";
    char nome2[MAX_PERSON_ID_LEN] = "Campo Grande";

    treg *node1_key = aloca_reg(2.0, 3.0, dummy_emb, nome1);
    treg *node2_key = aloca_reg(1.0, 1.0, dummy_emb, nome2);

    kdtree_constroi(&arv,comparador,distancia_kdtree_coord,2);
    
    assert(arv.raiz == NULL);
    assert(arv.k == 2);
    assert(arv.cmp(node1_key,node2_key,0) == 1);
    assert(arv.cmp(node1_key,node2_key,1) == 1);
    assert(strcmp(node1_key->person_id, "Dourados") == 0);
    assert(strcmp(node2_key->person_id, "Campo Grande") == 0);
    free(node1_key);
    free(node2_key);
}

void test_busca_n_nearest(){
    tarv arv;
    kdtree_constroi(&arv,comparador,distancia_kdtree_coord,2);

    float dummy_emb[EMBEDDING_DIM];
    for(int i=0; i<EMBEDDING_DIM; ++i) dummy_emb[i] = (float)i/100.0;

    char id_a[MAX_PERSON_ID_LEN] = "a";
    char id_b[MAX_PERSON_ID_LEN] = "b";
    char id_c[MAX_PERSON_ID_LEN] = "c";
    char id_d[MAX_PERSON_ID_LEN] = "d";
    char id_e[MAX_PERSON_ID_LEN] = "e";
    char id_f[MAX_PERSON_ID_LEN] = "f";

    inserir_ponto(10.0,10.0, dummy_emb,id_a);
    inserir_ponto(20.0,20.0, dummy_emb,id_b);
    inserir_ponto(1.0,10.0, dummy_emb,id_c);
    inserir_ponto(3.0,5.0, dummy_emb,id_d);
    inserir_ponto(7.0,15.0, dummy_emb,id_e);
    inserir_ponto(4.0,11.0, dummy_emb,id_f);

    printf("\n--- N-Nearest Search Test ---\n");
    treg query_point = {
        .lat = 7.0,
        .lon = 14.0,
        .embedding = {0.0},
        .person_id = "query"
    };

    int n_neighbors = 3;
    treg_array results = buscar_n_mais_proximos(&arv, query_point, n_neighbors);

    printf("Neighbors found for (7,14):\n");
    for (int i = 0; i < results.size; ++i) {
        printf("  ID: %s, Lat: %.1f, Lon: %.1f, Dist Sq: %.3f\n",
               results.elements[i].person_id, results.elements[i].lat,
               results.elements[i].lon, distancia_kdtree_coord(&results.elements[i], &query_point));
    }
    
    assert(results.size <= n_neighbors);

    int found_e = 0, found_f = 0, found_a = 0;
    for (int i = 0; i < results.size; ++i) {
        if (strcmp(results.elements[i].person_id, "e") == 0) found_e = 1;
        if (strcmp(results.elements[i].person_id, "f") == 0) found_f = 1;
        if (strcmp(results.elements[i].person_id, "a") == 0) found_a = 1;
    }
    assert(found_e == 1);
    assert(found_f == 1);
    assert(found_a == 1);

    free_treg_array(results);

    kdtree_destroi(&arvore_global);
}

int main(void){
    test_constroi();
    test_busca_n_nearest();
    printf("All tests passed successfully!\n");
    return EXIT_SUCCESS;
}
