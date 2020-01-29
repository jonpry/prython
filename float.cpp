#include "main.h"

extern "C" {

BINARY_DECL_INT_TO_FLOAT(float_add,PyFloat_t,vtable_float,aval+val)
BINARY_DECL_INT_TO_FLOAT(float_radd,PyFloat_t,vtable_float,aval+val)
BINARY_DECL_INT_TO_FLOAT(float_mul,PyFloat_t,vtable_float,aval*val)
BINARY_DECL_INT_TO_FLOAT(float_rmul,PyFloat_t,vtable_float,aval*val)
BINARY_DECL_INT_TO_FLOAT(float_sub,PyFloat_t,vtable_float,aval-val)
BINARY_DECL_INT_TO_FLOAT(float_rsub,PyFloat_t,vtable_float,aval-val)
BINARY_DECL_INT_TO_FLOAT(float_mod,PyFloat_t,vtable_float,__builtin_fmod(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_rmod,PyFloat_t,vtable_float,__builtin_fmod(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_truediv,PyFloat_t,vtable_float,aval/val)
BINARY_DECL_INT_TO_FLOAT(float_rtruediv,PyFloat_t,vtable_float,aval/val)
BINARY_DECL_INT_TO_FLOAT(float_floordiv,PyFloat_t,vtable_float,floor(aval/val))
BINARY_DECL_INT_TO_FLOAT(float_rfloordiv,PyFloat_t,vtable_float,floor(aval/val))
BINARY_DECL_INT_TO_FLOAT(float_pow,PyFloat_t,vtable_float,pow(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_rpow,PyFloat_t,vtable_float,pow(aval,val))

PyObject_t* float_str(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){
    char buf[32];
    sprintf(buf,"%lf", ((PyFloat_t*)*v1)->val);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    strcpy(ret->str,buf);
    return ret;
}

} //Extern "C"

