#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <exception>

extern "C" {

#define register 
#include "get_slot.cpp"
#include "slots.h"

class pyobj;
class pyfunc;
class pystr;
class pyclass;
class pytuple;
typedef pyobj* (*fnty)(struct pyobj **v1, uint64_t alen, pytuple **v2);
typedef int32_t (*lfnty )(pystr *);


typedef struct {
  uint64_t rtti;
  pyobj *dispatch[100]; //TODO: this has to be right, hard to sync with dump.py
} vtable_t;

extern const vtable_t vtable_int, vtable_float, vtable_str, vtable_code, vtable_tuple, vtable_func, vtable_class, vtable_bool, vtable_NotImplemented;

typedef class pyobj {
public:
  const vtable_t *vtable;
  vtable_t *itable;
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

typedef class pylist : public pyobj {
public:
  uint64_t sz, capacity;
  pyobj **objs;
} PyList_t;

typedef struct pycode : public pyobj {
public:
  fnty func;
  lfnty locals_func;
  PyTuple_t *locals;
} PyCode_t;

typedef class pyfunc : public pyobj {
public:
  PyCode_t *code;
  PyStr_t *str;
  PyTuple_t *dargs;
  pyclass *cls;
} PyFunc_t;

typedef class pybool : public pyobj {
public:
  int64_t val;
} PyBool_t;

typedef class pynoimp : public pyobj {
} PyNoImp_t;

typedef class pyclass : public pyobj {
public:
  PyStr_t* name;
  PyFunc_t* constructor;
  lfnty  locals_func;
  PyTuple_t *locals;
  PyTuple_t *values;
} PyClass_t;

extern PyNoImp_t global_noimp;
extern PyBool_t global_false, global_true;
extern PyFunc_t pyfunc_builtin_print_wrap, pyfunc_builtin_str, 
                pyfunc_builtin_repr, pyfunc_builtin_getattr, 
                pyfunc_builtin_setattr, pyfunc_builtin_buildclass,
                pyfunc_builtin_new;

const char *rtti_strings[] = {"int", "float", "tuple", "str", "code", "func", "class", "bool", "NotImplemented"};

#undef malloc
void* malloc(size_t) __attribute__((returns_nonnull));
void* __cxa_allocate_exception(size_t thrown_size);
void __cxa_throw(void* thrown_exception,
                 struct type_info *tinfo,
                 void (*dest)(void*));

//      void* exc = __cxa_allocate_exception(16); 
#define THROW() \
    { \
      __cxa_throw((void*)13,0,0); \
    }

__attribute__((noinline)) void dump(const PyObject_t *v){
   if(!v){
      printf("None\n");
      return;
   }

   printf("RTTI: %lx %s\n", v->vtable->rtti, rtti_strings[__builtin_ffsll(v->vtable->rtti)]);

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

PyObject_t* builtin_getattr(PyObject_t **v1, uint64_t alen, const PyTuple_t **v2){
   printf("Load attr %p %p\n", v1[0], v1[1]);
   dump(v1[0]);
   dump(v1[1]);

   PyStr_t* attr = (PyStr_t*)v1[0];
   //TODO: assert is string
   const SlotResult *res = in_word_set(attr->str,attr->sz-1);
   if(res){
       printf("Slot is %d\n", res->slot_num);
       if(v1[1]->itable && v1[1]->itable->dispatch[res->slot_num]->vtable->rtti != NOIMP_RTTI)
          return v1[1]->itable->dispatch[res->slot_num]; 
       return v1[1]->vtable->dispatch[res->slot_num]; 
   }else{
       printf("No slot: %s %lu\n", attr->str, attr->sz);
       THROW()       
   }

   return v1[0];
}

PyObject_t* builtin_setattr(PyObject_t **v1, uint64_t alen, const PyTuple_t **v2){
   printf("Set attr %p %p %p\n", v1[0], v1[1], v1[2]);
   dump(v1[0]);
   dump(v1[1]);
   dump(v1[2]);

   PyStr_t* attr = (PyStr_t*)v1[1];
   //TODO: assert is string
   const SlotResult *res = in_word_set(attr->str,attr->sz-1);
   if(res){
       printf("Slot is %d\n", res->slot_num);
       if(!v1[0]->itable){
           printf("Attribute is readonly\n");
           THROW()
       }
       v1[0]->itable->dispatch[res->slot_num] = v1[2];
   }else{
       printf("No slot: %s %lu\n", attr->str, attr->sz);
       THROW()       
   }

   return v1[0];
}

__attribute__((noinline)) PyObject_t* builtin_print(PyObject_t ** pv1, 
                                                    uint64_t alen,
                                                    const PyTuple_t **v2){
   //printf("Print entry %p %p %p %lu\n", pv1, *pv1, (*pv1)->vtable, (*pv1)->vtable->rtti);
   PyObject_t* v1 = *pv1;
   if(v1->vtable->rtti != STR_RTTI && v1->vtable->rtti != NOIMP_RTTI){
      v1 = ((PyFunc_t*)v1->vtable->dispatch[STR_SLOT])->code->func(pv1,1,0);
   }
   printf("Print %p %p\n", v1, v2);
   dump(v1);
   dump(pv1[1]);
   return 0;
}

__attribute__((always_inline)) PyObject_t* load_name(PyObject_t *v1, PyObject_t* v2){
   printf("LN\n"); 
   dump(v2);
   if(v1->vtable->rtti == NOIMP_RTTI){
      PyStr_t *str=(PyStr_t*)v2;
      //TODO: use mph
      if(strcmp(str->str,"print") == 0)
         return &pyfunc_builtin_print_wrap;
      if(strcmp(str->str,"str") == 0)
         return &pyfunc_builtin_str;
      if(strcmp(str->str,"getattr") == 0)
         return &pyfunc_builtin_getattr;
      if(strcmp(str->str,"setattr") == 0)
         return &pyfunc_builtin_setattr;
      if(strcmp(str->str,"repr") == 0)
         return &pyfunc_builtin_repr;
      if(strcmp(str->str,"buildclass") == 0)
         return &pyfunc_builtin_buildclass;


      //These things are actually globals
      if(strcmp(str->str,"object") == 0)
         return &global_noimp; //TODO:
      if(strcmp(str->str,"__name__") == 0)
         return &global_noimp; //TODO:
      THROW();
   }
   return v1;
}


__attribute__((always_inline)) PyObject_t* call_function(PyObject_t **v1, uint64_t alen, PyTuple_t** v2){
    PyObject_t *tgt = (PyObject_t*)v1[alen-1];
    if(tgt->vtable->rtti == FUNC_RTTI){
       PyFunc_t *func = (PyFunc_t*)tgt;
       return func->code->func(v1, alen-1, 0);
    }
    if(tgt->vtable->dispatch[CALL_SLOT] && tgt->vtable->dispatch[CALL_SLOT]->vtable->rtti != NOIMP_RTTI){
       printf("Can call via vtable\n");
    }

    if(tgt->itable->dispatch[CALL_SLOT] && tgt->itable->dispatch[CALL_SLOT]->vtable->rtti != NOIMP_RTTI){
       printf("Can call via itable\n");
       assert(tgt->itable->dispatch[CALL_SLOT]->vtable->rtti == FUNC_RTTI);
       PyTuple_t *locals = 0;
       PyObject_t *ret = ((PyFunc_t*)tgt->itable->dispatch[CALL_SLOT])->code->func(v1,alen-1,&locals);
       printf("Locals: %p\n", locals);
       for(int i=0; i < 5; i++){
          printf("L: %p %p\n", locals->objs[i], locals->objs[i]->vtable);
       }
       return ret;
    }

    THROW()
    return 0;
}


__attribute__((always_inline)) PyObject_t* binop(PyObject_t *v1, PyObject_t *v2, uint32_t slot1, uint32_t slot2){
   //printf("binop %p %p %d %d\n", v1, v2, slot1, slot2);
   //dump(v1);
   //dump(v2);
   PyObject_t *ret=0;
   PyObject_t *vs[2] = {v1,v2};
   if(v1->vtable->dispatch[slot1]->vtable->rtti != NOIMP_RTTI){
      ret = ((PyFunc_t*)v1->vtable->dispatch[slot1])->code->func(vs,2,0);
      if(!ret || ret->vtable->rtti != NOIMP_RTTI)
         return ret;
   }
   if(v2->vtable->dispatch[slot2]->vtable->rtti != NOIMP_RTTI){
      ret = ((PyFunc_t*)v2->vtable->dispatch[slot2])->code->func(vs,2,0);
      if(!ret || ret->vtable->rtti != NOIMP_RTTI)
         return ret;
   }
   printf("Could not perform operator\n");

   THROW();

   return (PyObject_t*)&global_noimp;
}

__attribute__((always_inline)) PyObject_t* unop(PyObject_t *v1, uint32_t slot){
   //printf("binop %p %p %d %d\n", v1, v2, slot1, slot2);
   //dump(v1);
   //dump(v2);
   PyObject_t *ret=0;
   if(v1->vtable->dispatch[slot]){
      ret = ((PyFunc_t*)v1->vtable->dispatch[slot])->code->func(&v1,2,0);
      if(!ret || ret->vtable->rtti != NOIMP_RTTI)
         return ret;
   }
   printf("Could not perform operator\n");

   THROW();

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

__attribute__((always_inline)) uint32_t hash_fnv(uint32_t d, PyStr_t *v){
    if(d == 0)
       d = 0x01000193;

    // Use the FNV algorithm from http://isthe.com/chongo/tech/comp/fnv/ 
    for(uint32_t i=0; i < v->sz; i++)
        d = ( (d * 0x01000193) ^ v->str[i]) & 0xffffffff;

    return d;
}

__attribute__((always_inline)) int32_t local_lookup(PyTuple_t *t, uint32_t *g, uint32_t *v){
    //TODO:
    return 0;
}


#define BINARY_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){ \
   if(v1[1]->vtable->rtti != v.rtti) \
     return &global_noimp; \
   t *ret = (t*)malloc(sizeof(t)); \
   int64_t aval = ((t*)(v1[0]))->val; \
   int64_t val = ((t*)v1[1])->val; \
   ret->val =  op; \
   ret->vtable = &v; \
   ret->itable = 0; \
   return ret; \
}

#define BOOL_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){ \
   if(v1[1]->vtable->rtti != v.rtti) \
     return &global_noimp; \
   int64_t aval = ((t*)(v1[0]))->val; \
   int64_t val = ((t*)v1[1])->val; \
   bool res =  op; \
   return res?&global_true:&global_false; \
}

#define UNARY_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){ \
   int64_t val = ((t*)(v1[0]))->val; \
   t *ret = (t*)malloc(sizeof(t)); \
   ret->val =  op; \
   ret->vtable = &v; \
   ret->itable = 0; \
   return ret; \
}

#define BINARY_DECL_INT_TO_FLOAT(f,t,v,...) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){ \
   double val=0; \
   if(v1[1]->vtable->rtti != v.rtti) {\
      if(v1[1]->vtable->dispatch[FLOAT_SLOT]){ \
         PyFloat_t *temp = (PyFloat_t*)((PyFunc_t*)v1[1]->vtable->dispatch[FLOAT_SLOT])->code->func(&v1[1],alen-1,0); \
         val = temp->val; \
      }else \
         return &global_noimp; \
   } else \
      val = ((PyFloat_t*)v1[1])->val; \
   t *ret = (t*)malloc(sizeof(t)); \
   double aval=((t*)(v1[0]))->val; \
   ret->val = __VA_ARGS__ ;\
   ret->vtable = &v; \
   ret->itable = 0; \
   return ret; \
}

typedef PyObject_t* (*bin_func_t)(PyObject_t*, uint64_t alen, PyTuple_t**);
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

BOOL_DECL(int_gt,PyInt_t,vtable_int,aval > val) 
BOOL_DECL(int_lt,PyInt_t,vtable_int,aval < val)
BOOL_DECL(int_ge,PyInt_t,vtable_int,aval >= val) 
BOOL_DECL(int_le,PyInt_t,vtable_int,aval <= val) 
BOOL_DECL(int_ne,PyInt_t,vtable_int,aval != val) 
BOOL_DECL(int_eq,PyInt_t,vtable_int,aval == val) 
UNARY_DECL(int_neg,PyInt_t,vtable_int,-val)


PyObject_t* float_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    char buf[32];
    sprintf(buf,"%lf", ((PyFloat_t*)*v1)->val);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    strcpy(ret->str,buf);
    return ret;
}

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

PyObject_t* bool_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    char buf[32];
    sprintf(buf,"%s", ((PyInt_t*)*v1)->val?"True":"False");
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + strlen(buf)+1);
    ret->sz = strlen(buf);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    strcpy(ret->str,buf);
    return ret;
}

PyObject_t* str_getitem(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    PyStr_t *t = (PyStr_t*)v1[0];
    PyInt_t *i = (PyInt_t*)v1[1];
    char c = t->str[i->val];   

    PyInt_t *ret = (PyInt_t*)malloc(sizeof(PyInt_t));
    ret->val = c;
    ret->vtable = &vtable_int;
    ret->itable = 0;
    return ret;
}

PyObject_t* tuple_getitem(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    PyTuple_t *t = (PyTuple_t*)v1[0];
    PyInt_t *i = (PyInt_t*)v1[1];
    return t->objs[i->val];   
}

PyObject_t* join(PyObject_t *v1, PyObject_t *v2, char left, char right){
    PyTuple_t *t = (PyTuple_t*)v1;
    PyStr_t *strs[t->sz];
    size_t total_sz=0;
    for(uint64_t i=0; i < t->sz; i++){
       strs[i] = (PyStr_t*)((PyFunc_t*)t->objs[i]->vtable->dispatch[STR_SLOT])->code->func(&t->objs[i],1,0);
       total_sz += strs[i]->sz;
    }
    uint64_t str_sz = total_sz + 2 + (t->sz - 1)*2;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + str_sz + 1);
    ret->sz = str_sz;
    ret->vtable = &vtable_str;
    ret->itable = 0;

    uint64_t pos=1;
    ret->str[0] = left;
    for(uint64_t i=0; i < t->sz; i++){
       memcpy(ret->str+pos,strs[i]->str,strs[i]->sz);
       pos += strs[i]->sz;
       if(i!=t->sz-1){
          ret->str[pos++] = ',';      
          ret->str[pos++] = ' ';      
       }
    }
    ret->str[pos++] = right;      
    ret->str[pos++] = 0;      

    return ret;
}

PyObject_t* list_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    char left = '[';
    char right = ']';
    PyList_t *t = (PyList_t*)(v1[0]);
    PyStr_t *strs[t->sz];
    size_t total_sz=0;
    for(uint64_t i=0; i < t->sz; i++){
       strs[i] = (PyStr_t*)((PyFunc_t*)t->objs[i]->vtable->dispatch[STR_SLOT])->code->func(&t->objs[i],1,0);
       total_sz += strs[i]->sz;
    }
    uint64_t str_sz = total_sz + 2 + (t->sz - 1)*2;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + str_sz + 1);
    ret->sz = str_sz;
    ret->vtable = &vtable_str;
    ret->itable = 0;

    uint64_t pos=1;
    ret->str[0] = left;
    for(uint64_t i=0; i < t->sz; i++){
       memcpy(ret->str+pos,strs[i]->str,strs[i]->sz);
       pos += strs[i]->sz;
       if(i!=t->sz-1){
          ret->str[pos++] = ',';      
          ret->str[pos++] = ' ';      
       }
    }
    ret->str[pos++] = right;      
    ret->str[pos++] = 0;      

    return ret;

}

PyObject_t* tuple_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    //assert(false);
    return join(v1[0],v1[1],'(',')');
}

PyObject_t* func_str(PyObject_t **v1, uint64_t alen, PyObject_t **v2){
    //printf("func_str %p\n", v1, *v1);
    PyFunc_t* func = (PyFunc_t*)(v1[0]);
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + func->str->sz + strlen("<function  >") + 1);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    sprintf(ret->str,"<function %s>", func->str->str);
    ret->sz = strlen(ret->str);
    return ret;
}

__attribute__((always_inline)) PyObject_t* str_str(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    return v1[0];
}

__attribute__((always_inline)) PyObject_t* int_float(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   PyFloat_t *ret = (PyFloat_t*)malloc(sizeof(PyFloat_t)); 
   ret->val = ((PyInt_t*)(v1[0]))->val; 
   ret->vtable = &vtable_float; 
   ret->itable = 0;
   return ret; 
}

__attribute__((always_inline)) PyObject_t* str_add(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    PyStr_t *s1 = (PyStr_t*)v1[0];
    PyStr_t *s2 = (PyStr_t*)v1[1];

    if(s2->vtable->rtti != STR_RTTI) \
      return &global_noimp;

    size_t newsz = s1->sz + s2->sz;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + newsz + 1);
    ret->vtable = &vtable_str;
    ret->itable = 0;
    ret->sz = newsz;
    memcpy(ret->str,s1->str,s1->sz);
    memcpy(ret->str + s1->sz, s2->str, s2->sz);
    ret->str[newsz] = 0;
    return ret;
}

PyObject_t* builtin_new(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   printf("new\n");
   exit(0);

/*
   PyFunc_t *bound_func = (PyFunc_t*)malloc(sizeof(PyFunc_t));
   *bound_func = *(PyFunc_t*)(v1[2]);
*/
}

PyObject_t* builtin_buildclass(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   printf("Buildclass\n");
   printf("%p %p %p\n", v1[0], v1[1], v1[2]);
   dump(v1[1]);
   dump(v1[2]);

   PyFunc_t *constructor = (PyFunc_t*)(v1[2]);
   PyTuple_t *values=0;
   constructor->code->func(0,0,&values);

   PyClass_t *cls = (PyClass_t*)malloc(sizeof(PyClass_t));
   cls->vtable = &vtable_class;
   cls->itable = (vtable_t*)malloc(sizeof(vtable_t));
   cls->itable->rtti = 0;
   cls->values = values;
   for(int i=0; i < 100; i++){
      cls->itable->dispatch[i] = &global_noimp;
   }

   printf("Locals: %p\n", values);
   for(int i=0; i < values->sz; i++){
      printf("L: %p %p\n", values->objs[i], values->objs[i]->vtable);
      dump(values->objs[i]);

      PyStr_t *str = (PyStr_t*)constructor->code->locals->objs[i];
      dump(str);
      const SlotResult *res = in_word_set(str->str,str->sz-1);
      if(res){
         printf("L: %d\n", res->slot_num);
         cls->itable->dispatch[res->slot_num] = values->objs[i];
      }
   }


   cls->itable->dispatch[CALL_SLOT] = &pyfunc_builtin_new;


   cls->name = (PyStr_t*)v1[1];
   cls->constructor = constructor;
   cls->locals_func = constructor->code->locals_func;
   cls->locals = constructor->code->locals;

   assert(cls->locals_func);
   assert(cls->locals);
   return cls;
}

PyObject_t* code_blob_0(PyObject_t **, uint64_t, PyObject_t*);

int main(){
   code_blob_0(0,0,0);
   return 0;
}

} //Extern "C"
