#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  uint64_t rtti;
} vtable_t;

typedef struct {
  vtable_t *vtable;
} PyObject_t;

typedef struct {
  PyObject_t obj;
  int64_t val;
} PyInt_t;

typedef struct {
  PyObject_t obj;
  uint64_t sz;
  char str[];
} PyStr_t;

PyObject_t* code_blob_5(PyObject_t *, PyObject_t*);

const char *rtti_strings[] = {"int", "float", "str", "code", "tuple", "func"};


void dump(PyObject_t *v){
   if(!v){
      printf("None\n");
      return;
   }

   printf("RTTI: %lx %s\n", v->vtable->rtti, rtti_strings[v->vtable->rtti]);

   switch(v->vtable->rtti){
      case 0: {
          printf("Int: %ld\n", ((PyInt_t*)v)->val); 
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

PyObject_t* builtin_buildclass(PyObject_t *v1, PyObject_t *v2){
   printf("Buildclass %p %p\n", v1, v2);
   dump(v1);
   dump(v2);
   return 0;
}

int main(){
   code_blob_5(0,0);
   return 0;
}
