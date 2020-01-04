#include "main.h"

extern "C" {

BINARY_DECL(int_add,PyInt_t,vtable_int,aval + val)
BINARY_DECL(int_radd,PyInt_t,vtable_int,aval + val)
BINARY_DECL(int_mul,PyInt_t,vtable_int,aval - val)
BINARY_DECL(int_sub,PyInt_t,vtable_int,aval - val)
BINARY_DECL(int_and,PyInt_t,vtable_int,aval & val)
BINARY_DECL(int_or,PyInt_t,vtable_int,aval | val)
BINARY_DECL(int_xor,PyInt_t,vtable_int,aval ^ val)
BINARY_DECL(int_lshift,PyInt_t,vtable_int,aval << val)
BINARY_DECL(int_rshift,PyInt_t,vtable_int,aval >> val)
BINARY_DECL(int_floordiv,PyInt_t,vtable_int,aval / val)
BINARY_DECL(int_mod,PyInt_t,vtable_int,aval % val)

BOOL_DECL(int_gt,PyInt_t,vtable_int,aval > val) 
BOOL_DECL(int_lt,PyInt_t,vtable_int,aval < val)
BOOL_DECL(int_ge,PyInt_t,vtable_int,aval >= val) 
BOOL_DECL(int_le,PyInt_t,vtable_int,aval <= val) 
BOOL_DECL(int_ne,PyInt_t,vtable_int,aval != val) 
BOOL_DECL(int_eq,PyInt_t,vtable_int,aval == val) 
UNARY_DECL(int_neg,PyInt_t,vtable_int,-val)

PyObject_t* int_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    char buf[32];
    sprintf(buf,"%ld", ((PyInt_t*)*v1)->val);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    strcpy(ret->str,buf);
    return ret;
}

__attribute__((always_inline)) PyObject_t* int_float(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   PyFloat_t *ret = (PyFloat_t*)malloc(sizeof(PyFloat_t)); 
   ret->val = ((PyInt_t*)(v1[0]))->val; 
   ret->vtable = &vtable_float; 
   ret->itable = 0;
   return ret; 
}

} //Extern "C"

