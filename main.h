#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <exception>
#include <unordered_map>

extern "C" {
class pyobj;

size_t pyobj_hash(pyobj*);
}

// custom specialization of std::hash can be injected in namespace std
namespace std
{
    template<> struct hash<pyobj*>
    {
      std::size_t operator()(pyobj* o) const noexcept {
          return pyobj_hash(o);
      }
    };
}       

extern "C" {

#include "slots.h"

#define DEBUG

#ifdef DEBUG
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif

class pyobj;
class pyfunc;
class pystr;
class pyclass;
class pytuple;
struct pyctx;
typedef pyobj* (*fnty)(struct pyobj **v1, uint64_t alen, pyctx *v2);
typedef int32_t (*lfnty )(pystr *);


typedef struct {
  uint64_t rtti;
  pyobj *dispatch[100]; //TODO: this has to be right, hard to sync with dump.py
} vtable_t;

extern const vtable_t vtable_int, vtable_float, vtable_str, vtable_code, vtable_tuple, vtable_func, vtable_class, vtable_bool, vtable_NotImplemented, vtable_object, vtable_dict, vtable_list_iter, vtable_dict_view, vtable_dict_iter, vtable_slice;

class pyclass;

typedef class pyobj {
public:
  const vtable_t *vtable;
  vtable_t *itable;
  class pyclass *cls;
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
  PyTuple_t *closures;
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
  uint64_t nbases;
  class pyclass *bases[];
} PyClass_t;

typedef class pybase : public pyobj {
public:
   std::unordered_map<std::string,PyObject_t*> *attrs;
} PyBase_t;

typedef class pydict : public pyobj {
public:
   std::unordered_map<PyObject_t*,PyObject_t*> *elems;
} PyDict_t;

typedef class pylist_it : public pyobj {
public:
   PyList_t *obj;
   uint64_t pos;
} PyList_Iterator_t;

typedef class pydict_view : public pyobj {
public:
   PyDict_t *dict;
   int type;
} PyDict_View_t;

typedef class pydict_iter : public pyobj {
public:
   PyDict_View_t *view;
   std::unordered_map<PyObject_t*,PyObject_t*>::iterator it;
} PyDict_Iterator_t;

typedef class pyslice : public pyobj {
public:
   PyInt_t *start,*stop,*step;
} PySlice_t;

typedef struct pyctx {
   PyTuple_t *locals;
   PyTuple_t *closures;
} PyCtx_t;

extern PyNoImp_t global_noimp;
extern PyBool_t global_false, global_true;
extern PyFunc_t pyfunc_builtin_print_wrap, pyfunc_builtin_str, 
                pyfunc_builtin_repr, pyfunc_builtin_getattr, 
                pyfunc_builtin_setattr, pyfunc_builtin_buildclass,
                pyfunc_builtin_new, pyfunc_builtin_len,
                pyfunc_builtin_hash, pyfunc_builtin_exit, pyfunc_builtin_super;

extern PyClass_t pyclass_dict;

#define BINARY_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){ \
   if(v1[1]->vtable->rtti != v.rtti) \
     return &global_noimp; \
   t *ret = (t*)malloc(sizeof(t)); \
   int64_t aval = ((t*)(v1[0]))->val; \
   int64_t val = ((t*)v1[1])->val; \
   ret->val =  op; \
   ret->vtable = &v; \
   ret->itable = 0; \
   ret->cls = 0; \
   return ret; \
}

#define BOOL_DECL(f,t,v,op) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){ \
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
   ret->cls = 0; \
   return ret; \
}

#define BINARY_DECL_INT_TO_FLOAT(f,t,v,...) \
__attribute__((always_inline)) PyObject_t* f(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){ \
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
   ret->cls = 0; \
   return ret; \
}

typedef PyObject_t* (*bin_func_t)(PyObject_t*, uint64_t alen, PyCtx_t*);

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

__attribute__((always_inline)) PyObject_t* unop(PyObject_t *v1, uint32_t slot);

enum {DICTVIEW_ITEMS,DICTVIEW_KEYS,DICTVIEW_VALUES};

PyInt_t *make_int(int64_t v);

} //Extern "C"
