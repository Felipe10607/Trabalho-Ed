from fastapi import FastAPI, Query, HTTPException
from kdtree_wrapper import lib, Tarv, TReg, TRegArray, EMBEDDING_DIM, MAX_PERSON_ID_LEN
from ctypes import POINTER, c_char, c_float
from pydantic import BaseModel, Field
from typing import List

app = FastAPI()

# Pydantic model for input data (insertion)
class PontoEntrada(BaseModel):
    lat: float
    lon: float
    embedding: List[float] = Field(..., min_length=EMBEDDING_DIM, max_length=EMBEDDING_DIM)
    person_id: str = Field(..., max_length=MAX_PERSON_ID_LEN - 1)

# Pydantic model for search results
class PontoResultado(BaseModel):
    lat: float
    lon: float
    person_id: str
    embedding: List[float]

# Helper to check if C library is loaded
def _check_lib_loaded():
    if lib is None:
        raise HTTPException(status_code=500, detail="C library (libkdtree.so) not loaded. Check server logs.")

@app.post("/construir-arvore")
def constroi_arvore():
    _check_lib_loaded()
    lib.kdtree_construir()
    return {"message": "KD-Tree initialized."}

@app.post("/inserir")
def inserir(ponto: PontoEntrada):
    _check_lib_loaded()

    c_embedding = (c_float * EMBEDDING_DIM)(*ponto.embedding)
    
    person_id_bytes = ponto.person_id.encode('utf-8')
    c_person_id_array = (c_char * MAX_PERSON_ID_LEN)()
    # Copy bytes safely, ensuring null-termination
    for i, byte in enumerate(person_id_bytes):
        if i < MAX_PERSON_ID_LEN - 1:
            c_person_id_array[i] = byte
        else:
            break
    c_person_id_array[min(len(person_id_bytes), MAX_PERSON_ID_LEN - 1)] = b'\0'

    lib.inserir_ponto(ponto.lat, ponto.lon, c_embedding, c_person_id_array)
    return {"message": f"Point '{ponto.person_id}' inserted."}

@app.get("/buscar-n-vizinhos", response_model=List[PontoResultado])
def buscar_n_vizinhos(lat: float = Query(...), lon: float = Query(...), n: int = Query(1, ge=1)):
    _check_lib_loaded()
    
    # Query point
    query_reg = TReg(lat=lat, lon=lon,
                     embedding=(c_float * EMBEDDING_DIM)(*[0.0]*EMBEDDING_DIM),
                     person_id="query".encode('utf-8'))

    arv = lib.get_tree()
    if not arv:
        raise HTTPException(status_code=500, detail="KD-Tree not initialized. Use /construir-arvore first.")

    c_results_array = lib.buscar_n_mais_proximos(arv, query_reg, n)

    results = []
    try:
        for i in range(c_results_array.size):
            reg = c_results_array.elements[i]
            results.append(PontoResultado(
                lat=reg.lat,
                lon=reg.lon,
                person_id=reg.person_id.decode('utf-8'),
                embedding=list(reg.embedding)
            ))
    finally:
        lib.free_treg_array(c_results_array) # Release C-allocated memory

    return results
