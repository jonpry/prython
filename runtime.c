#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static uint8_t arena[256][1024];
static uint32_t malloc_pos=0;
__attribute__((always_inline)) void* my_malloc(size_t sz){
   return arena[malloc_pos++];
}


struct pyobj;
typedef struct pyobj* (*fnty)(struct pyobj *v1, struct pyobj *v2);

typedef struct {
  uint64_t rtti;
  fnty dispatch[];
} vtable_t;

extern const vtable_t vtable_int, vtable_float, vtable_str, vtable_code, vtable_tuple, vtable_func, vtable_class, vtable_bool, vtable_NotImplemented;

typedef struct pyobj {
  const vtable_t *vtable;
} PyObject_t;

typedef struct {
  PyObject_t obj;
  int64_t val;
} PyInt_t;

typedef struct {
  PyObject_t obj;
  double val;
} PyFloat_t;

typedef struct {
  PyObject_t obj;
  uint64_t sz;
  char str[];
} PyStr_t;

typedef struct {
  PyObject_t obj;
  PyObject_t *(*func)(PyObject_t* obj, PyObject_t *obj2);
} PyCode_t;

typedef struct {
  PyObject_t obj;
  PyCode_t *code;
  PyStr_t *str;
} PyFunc_t;

typedef struct {
  PyObject_t obj;
} PyNoImp_t;

extern PyNoImp_t global_noimp;

const char *rtti_strings[] = {"int", "float", "str", "code", "tuple", "func", "class", "bool", "NotImplemented"};
#define NOIMP_RTTI 8

#undef malloc
void* malloc(size_t) __attribute__((returns_nonnull));


__attribute__((noinline)) void dump(PyObject_t *v){
   if(!v){
      printf("None\n");
      return;
   }

   printf("RTTI: %lx %s\n", v->vtable->rtti, rtti_strings[v->vtable->rtti]);

   switch(v->vtable->rtti){
      case 0: {
          printf("Int: %ld\n", ((PyInt_t*)v)->val); 
       }break;
      case 1: {
          printf("Float: %lf\n", ((PyFloat_t*)v)->val); 
       }break;
      case 2: {
          printf("Str: %s\n", ((PyStr_t*)v)->str); 
       }break;
   }
}

PyObject_t* import_name(PyObject_t *v1, PyObject_t *v2, PyObject_t *v3){
   printf("Import name %p %p %p\n", v1, v2, v3);
   dump(v1);
   dump(v2);
   dump(v3);
   return 0;
}

PyObject_t* load_attr(PyObject_t *v1, PyObject_t *v2){
   printf("Load attr %p %p\n", v1, v2);
   dump(v1);
   dump(v2);
   return 0;
}

PyObject_t* builtin_print(PyObject_t *v1, PyObject_t *v2){
   printf("Print %p %p\n", v1, v2);
   dump(v1);
   dump(v2);
   return 0;
}

PyObject_t* builtin_new(PyObject_t *v1, PyObject_t *v2){
   printf("new\n");
   return 0;
}

__attribute__((always_inline)) PyObject_t* binop(PyObject_t *v1, PyObject_t *v2, uint32_t slot1, uint32_t slot2){
   //printf("binop %d %d\n", slot1, slot2);
   //dump(v1);
   //dump(v2);
   PyObject_t *ret=0;
   if(v1->vtable->dispatch[slot1]){
      ret = v1->vtable->dispatch[slot1](v1,v2);
      if(!ret || ret->vtable->rtti != NOIMP_RTTI)
         return ret;
   }
   if(v2->vtable->dispatch[slot2]){
      ret = v2->vtable->dispatch[slot2](v2,v1);
      if(!ret || ret->vtable->rtti != NOIMP_RTTI)
         return ret;
   }
   printf("Could not perform operator\n");
   return (PyObject_t*)&global_noimp;
}


#define BINARY_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t *v1, PyObject_t *v2){ \
   if(v2->vtable->rtti != v.rtti) \
     return &global_noimp.obj; \
   t *ret = (t*)malloc(sizeof(t)); \
   int64_t aval = ((t*)v1)->val; \
   int64_t val = ((t*)v2)->val; \
   ret->val =  op; \
   ret->obj.vtable = &v; \
   return (PyObject_t*)ret; \
}

#define BINARY_DECL_INT_TO_FLOAT(f,t,v,...) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t *v1, PyObject_t *v2){ \
   double val=0; \
   if(v2->vtable->rtti == vtable_int.rtti){ \
      val = ((PyInt_t*)v2)->val; \
   } else if(v2->vtable->rtti != v.rtti) {\
      return &global_noimp.obj; \
   } else \
      val = ((PyFloat_t*)v2)->val; \
   t *ret = (t*)malloc(sizeof(t)); \
   double aval=((t*)v1)->val; \
   ret->val = __VA_ARGS__ ;\
   ret->obj.vtable = &v; \
   return (PyObject_t*)ret; \
}

BINARY_DECL_INT_TO_FLOAT(float_add,PyFloat_t,vtable_float,aval+val)
BINARY_DECL_INT_TO_FLOAT(float_radd,PyFloat_t,vtable_float,aval+val)
BINARY_DECL_INT_TO_FLOAT(float_mul,PyFloat_t,vtable_float,aval*val)
BINARY_DECL_INT_TO_FLOAT(float_rmul,PyFloat_t,vtable_float,aval*val)
BINARY_DECL_INT_TO_FLOAT(float_sub,PyFloat_t,vtable_float,aval-val)
BINARY_DECL_INT_TO_FLOAT(float_rsub,PyFloat_t,vtable_float,aval-val)
BINARY_DECL_INT_TO_FLOAT(float_mod,PyFloat_t,vtable_float,remainder(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_rmod,PyFloat_t,vtable_float,remainder(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_truediv,PyFloat_t,vtable_float,aval/val)
BINARY_DECL_INT_TO_FLOAT(float_rtruediv,PyFloat_t,vtable_float,aval/val)
BINARY_DECL_INT_TO_FLOAT(float_floordiv,PyFloat_t,vtable_float,floor(aval/val))
BINARY_DECL_INT_TO_FLOAT(float_rfloordiv,PyFloat_t,vtable_float,floor(aval/val))
BINARY_DECL_INT_TO_FLOAT(float_pow,PyFloat_t,vtable_float,pow(aval,val))
BINARY_DECL_INT_TO_FLOAT(float_rpow,PyFloat_t,vtable_float,pow(aval,val))

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


PyObject_t* builtin_buildclass(PyObject_t *v1, PyObject_t *v2){
   printf("Buildclass %p %p\n", v1, v2);
   dump(v1);
   dump(v2);

   PyCode_t *code = (PyCode_t*)malloc(sizeof(PyCode_t));
   code->func = builtin_new;
   code->obj.vtable = &vtable_code;

   PyStr_t *str = (PyStr_t*)malloc(sizeof(PyStr_t) + 4);
   strcpy(str->str, "new");
   str->sz = 4;
   str->obj.vtable = &vtable_str;

   PyFunc_t *func = (PyFunc_t*)malloc(sizeof(PyFunc_t));
   func->code = code;
   func->str = str;
   func->obj.vtable = &vtable_func;

   return (PyObject_t*)func;
}

PyObject_t* code_blob_0(PyObject_t *, PyObject_t*);

int main(){
   malloc_pos=0;
   code_blob_0(0,0);
   return 0;
}
