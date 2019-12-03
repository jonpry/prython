#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>

extern "C" {

static uint8_t arena[256][1024];
static uint32_t malloc_pos=0;
__attribute__((always_inline)) void* my_malloc(size_t sz){
   return arena[malloc_pos++];
}


class pyobj;
typedef struct pyobj* (*fnty)(struct pyobj *v1, struct pyobj *v2);

typedef struct {
  uint64_t rtti;
  fnty dispatch[101]; //TODO: this has to be right, hard to sync with dump.py
} vtable_t;

extern const vtable_t vtable_int, vtable_float, vtable_str, vtable_code, vtable_tuple, vtable_func, vtable_class, vtable_bool, vtable_NotImplemented;

typedef class pyobj {
public:
  const vtable_t *vtable;
} PyObject_t;

typedef class pyint : public pyobj {
public:
  int64_t val;
} PyInt_t;

typedef class pyfloat : public pyobj {
public:
  double val;
} PyFloat_t;

typedef class pystr : public pyobj {
public:
  uint64_t sz;
  char str[];
} PyStr_t;

typedef class pytuple : public pyobj {
public:
  uint64_t sz;
  pyobj *objs[];
} PyTuple_t;

typedef struct pycode : public pyobj {
public:
  PyObject_t *(*func)(PyObject_t* obj, PyObject_t *obj2);
} PyCode_t;

typedef class pyfunc : public pyobj {
public:
  PyCode_t *code;
  PyStr_t *str;
} PyFunc_t;

typedef class pynoimp : public pyobj {
} PyNoImp_t;

extern PyNoImp_t global_noimp;

const char *rtti_strings[] = {"int", "float", "tuple", "str", "code", "func", "class", "bool", "NotImplemented"};
#define NOIMP_RTTI 8
#define INT_RTTI 0
#define FLOAT_RTTI 1
#define TUPLE_RTTI 2
#define STR_RTTI 3

#define FLOAT_SLOT 0
#define STR_SLOT 1

#undef malloc
void* malloc(size_t) __attribute__((returns_nonnull));


__attribute__((noinline)) void dump(const PyObject_t *v){
   if(!v){
      printf("None\n");
      return;
   }

   printf("RTTI: %lx %s\n", v->vtable->rtti, rtti_strings[v->vtable->rtti]);

   switch(v->vtable->rtti){
      case INT_RTTI: {
          printf("Int: %ld\n", ((PyInt_t*)v)->val); 
       }break;
      case FLOAT_RTTI: {
          printf("Float: %lf\n", ((PyFloat_t*)v)->val); 
       }break;
      case STR_RTTI: {
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

__attribute__((noinline)) PyObject_t* builtin_print(const PyObject_t * v1, 
                                                    const PyObject_t * v2){
   if(v1->vtable->rtti != STR_RTTI){
      v1 = v1->vtable->dispatch[STR_SLOT]((PyObject_t*)v1,0);
   }
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

__attribute__((always_inline)) bool truth(PyObject_t *v1){
   //printf("truth\n");
   //dump(v1);
   if(!v1)
     return false;
   if(v1->vtable->rtti == INT_RTTI && ((PyInt_t*)v1)->val == 0)
     return false;
   if(v1->vtable->rtti == FLOAT_RTTI && ((PyFloat_t*)v1)->val == 0)
     return false;
   //printf("Was true\n");
   return true;
}

#define BINARY_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t *v1, PyObject_t *v2){ \
   if(v2->vtable->rtti != v.rtti) \
     return &global_noimp; \
   t *ret = (t*)malloc(sizeof(t)); \
   int64_t aval = ((t*)v1)->val; \
   int64_t val = ((t*)v2)->val; \
   ret->val =  op; \
   ret->vtable = &v; \
   return ret; \
}

#define BINARY_DECL_INT_TO_FLOAT(f,t,v,...) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t *v1, PyObject_t *v2){ \
   double val=0; \
   if(v2->vtable->rtti != v.rtti) {\
      if(v2->vtable->dispatch[FLOAT_SLOT]){ \
         PyFloat_t *temp = (PyFloat_t*)v2->vtable->dispatch[FLOAT_SLOT](v2,0); \
         val = temp->val; \
      }else \
         return &global_noimp; \
   } else \
      val = ((PyFloat_t*)v2)->val; \
   t *ret = (t*)malloc(sizeof(t)); \
   double aval=((t*)v1)->val; \
   ret->val = __VA_ARGS__ ;\
   ret->vtable = &v; \
   return ret; \
}

typedef PyObject_t* (*bin_func_t)(PyObject_t*, PyObject_t*);
//bin_func_t my_func = [](PyObject_t*, PyObject_t*){return 0;}

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

PyObject_t* float_str(PyObject_t *v1, PyObject_t *v2){
    char buf[32];
    sprintf(buf,"%lf", ((PyFloat_t*)v1)->val);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    strcpy(ret->str,buf);
    return ret;
}

PyObject_t* int_str(PyObject_t *v1, PyObject_t *v2){
    char buf[32];
    sprintf(buf,"%ld", ((PyInt_t*)v1)->val);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    strcpy(ret->str,buf);
    return ret;
}

PyObject_t* bool_str(PyObject_t *v1, PyObject_t *v2){
    char buf[32];
    sprintf(buf,"%s", ((PyInt_t*)v1)->val?"True":"False");
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    strcpy(ret->str,buf);
    return ret;
}

PyObject_t* tuple_str(PyObject_t *v1, PyObject_t *v2){
    PyTuple_t *t = (PyTuple_t*)v1;
    PyStr_t *strs[t->sz];
    size_t total_sz=0;
    for(uint64_t i=0; i < t->sz; i++){
       strs[i] = (PyStr_t*)t->objs[i]->vtable->dispatch[STR_SLOT](t->objs[i],0);
       total_sz += strs[i]->sz;
    }
    uint64_t str_sz = total_sz + 2 + t->sz - 1;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + str_sz + 1);
    ret->sz = str_sz;
    ret->vtable = &vtable_str;

    uint64_t pos=1;
    ret->str[0] = '(';
    for(uint64_t i=0; i < t->sz; i++){
       memcpy(ret->str+pos,strs[i]->str,strs[i]->sz);
       pos += strs[i]->sz;
       if(i!=t->sz-1)
          ret->str[pos++] = ',';      
    }
    ret->str[pos++] = ')';      
    ret->str[pos++] = 0;      

    return ret;
}

PyObject_t* str_str(PyObject_t *v1, PyObject_t *v2){
    return v1;
}

__attribute__((always_inline)) PyObject_t* int_float(PyObject_t *v1, PyObject_t *v2){
   PyFloat_t *ret = (PyFloat_t*)malloc(sizeof(PyFloat_t)); 
   ret->val = ((PyInt_t*)v1)->val; 
   ret->vtable = &vtable_float; 
   return ret; 
}

__attribute__((always_inline)) PyObject_t* str_add(PyObject_t *v1, PyObject_t *v2){
    PyStr_t *s1 = (PyStr_t*)v1;
    PyStr_t *s2 = (PyStr_t*)v2;

    if(s2->vtable->rtti != STR_RTTI) \
      return &global_noimp;

    size_t newsz = s1->sz + s2->sz;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + newsz + 1);
    ret->vtable = &vtable_str;
    ret->sz = newsz;
    memcpy(ret->str,s1->str,s1->sz);
    memcpy(ret->str + s1->sz, s2->str, s2->sz);
    ret->str[newsz] = 0;
    return ret;
}

PyObject_t* builtin_buildclass(PyObject_t *v1, PyObject_t *v2){
   printf("Buildclass %p %p\n", v1, v2);
   dump(v1);
   dump(v2);

   PyCode_t *code = (PyCode_t*)malloc(sizeof(PyCode_t));
   code->func = builtin_new;
   code->vtable = &vtable_code;

   PyStr_t *str = (PyStr_t*)malloc(sizeof(PyStr_t) + 4);
   strcpy(str->str, "new");
   str->sz = 4;
   str->vtable = &vtable_str;

   PyFunc_t *func = (PyFunc_t*)malloc(sizeof(PyFunc_t));
   func->code = code;
   func->str = str;
   func->vtable = &vtable_func;

   return func;
}

PyObject_t* code_blob_0(PyObject_t *, PyObject_t*);

int main(){
   malloc_pos=0;
   code_blob_0(0,0);
   return 0;
}

} //Extern "C"