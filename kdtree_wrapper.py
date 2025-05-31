import ctypes
from ctypes import Structure, POINTER, c_double, c_int, c_char, c_float, CFUNCTYPE

EMBEDDING_DIM = 128
MAX_PERSON_ID_LEN = 100

# C-compatible structure for a point (register)
class TReg(Structure):
    _fields_ = [("lat", c_double),
                ("lon", c_double),
                ("embedding", c_float * EMBEDDING_DIM),
                ("person_id", c_char * MAX_PERSON_ID_LEN)]

# C-compatible structure for a KD-Tree node
class TNode(Structure):
    pass # Defined later to handle self-reference

TNode._fields_ = [("key", ctypes.c_void_p),
                  ("esq", POINTER(TNode)),
                  ("dir", POINTER(TNode))]

# C-compatible structure for the KD-Tree
class Tarv(Structure):
    _fields_ = [("k", c_int),
                ("dist", CFUNCTYPE(c_double, ctypes.c_void_p, ctypes.c_void_p)),
                ("cmp", CFUNCTYPE(c_int, ctypes.c_void_p, ctypes.c_void_p, c_int)),
                ("raiz", POINTER(TNode))]

# C-compatible structure for returning an array of TReg from C
class TRegArray(Structure):
    _fields_ = [("elements", POINTER(TReg)),
                ("size", c_int)]

# Load the C shared library
try:
    lib = ctypes.CDLL("./libkdtree.so")
except OSError as e:
    print(f"Error loading libkdtree.so: {e}")
    # This pass allows the Python app to start, but C-dependent calls will fail
    lib = None 

# Define C function signatures
if lib:
    lib.inserir_ponto.argtypes = [c_double, c_double, c_float * EMBEDDING_DIM, c_char * MAX_PERSON_ID_LEN]
    lib.inserir_ponto.restype = None

    lib.buscar_n_mais_proximos.argtypes = [POINTER(Tarv), TReg, c_int]
    lib.buscar_n_mais_proximos.restype = TRegArray

    lib.get_tree.restype = POINTER(Tarv)
    lib.kdtree_construir.argtypes = []
    lib.kdtree_construir.restype = None

    lib.free_treg_array.argtypes = [TRegArray]
    lib.free_treg_array.restype = None
