#include "main.h"

extern "C" {

#define register 
#include "get_slot.cpp"

const char *rtti_strings[] = {"int", "float", "tuple", "str", "code", "func", "class", "bool", "NotImplemented","exception","list","dict","object"};


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

   printf("RTTI: %lx %s\n", v->vtable->rtti, rtti_strings[__builtin_ffsll(v->vtable->rtti)-1]);

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
   dprintf("Import name %p %p %p\n", v1, v2, v3);
   dump(v1);
   dump(v2);
   dump(v3);
   return 0;
}

__attribute__((always_inline)) PyObject_t* unop(PyObject_t *v1, uint32_t slot);
size_t pyobj_hash(pyobj* o){
   PyInt_t *i = (PyInt_t*)unop(o,HASH_SLOT);
   return i->val;
}


__attribute__((always_inline)) PyObject_t* build_map(PyObject_t **v, uint32_t len){ 
   PyDict_t *d = (PyDict_t*)malloc(sizeof(PyDict_t));
   d->vtable = &vtable_dict;
   d->elems = new std::unordered_map<PyObject_t*,PyObject_t*>();
   for(uint32_t i=0; i < len; i++){
      (*d->elems)[v[i*2+1]] = v[i*2];
   }
   return d;
}

__attribute__((always_inline)) PyObject_t* store_subscr(PyObject_t *v1, PyObject_t *v2, PyObject_t *v3){ 
   PyDict_t *d = (PyDict_t*)v2;
   (*d->elems)[v1] = v3;
   return 0;
}


PyObject_t* builtin_getattr(PyObject_t **v1, uint64_t alen, const PyTuple_t **v2){
   dprintf("Load attr %p %p\n", v1[0], v1[1]);
   dump(v1[0]);
   dump(v1[1]);

   PyStr_t* attr = (PyStr_t*)v1[1];
   PyObject_t *obj = (PyObject_t*)v1[0];
   //TODO: assert is string
   const SlotResult *res = in_word_set(attr->str,attr->sz-1);
   if(res){
       dprintf("Slot is %d\n", res->slot_num);
       if(obj->itable && obj->itable->dispatch[res->slot_num]->vtable->rtti != NOIMP_RTTI)
          return obj->itable->dispatch[res->slot_num]; 
       return obj->vtable->dispatch[res->slot_num]; 
   }else{
       if(obj->vtable->rtti == CLASS_RTTI){
          PyClass_t *cls = (PyClass_t*)obj;
          int res = cls->locals_func(attr);
          if(res >= 0 && cls->values->objs[res]->vtable->rtti != NOIMP_RTTI)
             return cls->values->objs[res];
       }
       if(obj->vtable->rtti == OBJECT_RTTI){
          PyBase_t *base = (PyBase_t*)obj;
          auto it = base->attrs->find(attr->str);
          if(it != base->attrs->end())
             return (*it).second; 

          PyClass_t *cls = base->cls;
          int res = cls->locals_func(attr);
          printf("R: %d\n", res);
          if(res >= 0 && cls->values->objs[res]->vtable->rtti != NOIMP_RTTI){
             return cls->values->objs[res];
          }
       }
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

   PyObject_t *obj = (PyObject_t*)v1[0];
   PyStr_t* attr = (PyStr_t*)v1[1];
   //TODO: assert is string
   const SlotResult *res = in_word_set(attr->str,attr->sz-1);
   if(res){
       dprintf("Slot is %d\n", res->slot_num);
       if(!v1[0]->itable){
           printf("Attribute is readonly\n");
           THROW()
       }
       v1[0]->itable->dispatch[res->slot_num] = v1[2];
   }else{
       if(obj->vtable->rtti == CLASS_RTTI){
          PyClass_t *cls = (PyClass_t*)obj;
          int res = cls->locals_func(attr);
          if(res >= 0 && cls->values->objs[res]->vtable->rtti != NOIMP_RTTI){
             cls->values->objs[res] = v1[2];
             return obj;
          }
       }
       if(obj->vtable->rtti == OBJECT_RTTI){
          dprintf("obj setattr\n");
          PyBase_t *base = (PyBase_t*)obj;
          (*base->attrs)[attr->str] = v1[2];
          return obj;
       }
       printf("SA no slot: %s %lu\n", attr->str, attr->sz);
       THROW()       
   }

   return obj;
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
   dprintf("LN\n"); 
   dump(v2);
   if(!v1 || v1->vtable->rtti == NOIMP_RTTI){
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
    PyObject_t *tgt = (PyObject_t*)v1[0];
    if(tgt->vtable->rtti == FUNC_RTTI){
       PyFunc_t *func = (PyFunc_t*)tgt;
       dprintf("Call via direct\n");
       return func->code->func(v1+1, alen-1, 0);
    }
    if(tgt->vtable->dispatch[CALL_SLOT] && tgt->vtable->dispatch[CALL_SLOT]->vtable->rtti != NOIMP_RTTI){
       dprintf("Can call via vtable\n");
    }

    if(tgt->itable->dispatch[CALL_SLOT] && tgt->itable->dispatch[CALL_SLOT]->vtable->rtti != NOIMP_RTTI){
       dprintf("Can call via itable\n");
       assert(tgt->itable->dispatch[CALL_SLOT]->vtable->rtti == FUNC_RTTI);
       //Instance attributes get self
       PyObject_t *ret = ((PyFunc_t*)tgt->itable->dispatch[CALL_SLOT])->code->func(v1,alen,0);
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
   printf("unop %p %d\n", v1, slot);
   dump(v1);
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

__attribute__((always_inline)) uint64_t hash_fnv(uint64_t d, PyStr_t *v){
    if(d == 0)
       d = 0x01000193;

    // Use the FNV algorithm from http://isthe.com/chongo/tech/comp/fnv/ 
    for(uint32_t i=0; i < v->sz-1; i++)
        d = ( (d ^ v->str[i]) * 0x01000193);

    return d;
}

__attribute__((always_inline)) int32_t local_lookup(PyStr_t* str, PyTuple_t *t, int32_t *g, int32_t *v, uint32_t len){
    dprintf("LF\n");
    //dump(str);

    int d = g[hash_fnv(0,str) % len];
/*
    printf("d: %d %d %d %d\n", d, hash_fnv(0,str), len, hash_fnv(0,str) % len);
    for(int i=0; i < len; i++){
       printf("%d ", g[i]);
    }
    printf("\n");

    for(int i=0; i < len; i++){
       printf("%d ", v[i]);
    }
    printf("\n");
*/
    int slot=0;
    if(d < 0){
      slot = v[-d-1];
    }else{
      slot = v[hash_fnv(d,str) % len];
    }
    
    //for(int i=0; i < t->sz; i++)
    //  dump(t->objs[i]);
    PyStr_t *c=(PyStr_t*)t->objs[slot];
    if(strcmp(c->str,str->str)==0)
       return slot;
    dump(c);
    return -1;
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

PyObject_t* str_hash(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    printf("Str hash\n");
    PyStr_t *t = (PyStr_t*)v1[0];

    PyInt_t *ret = (PyInt_t*)malloc(sizeof(PyInt_t));
    ret->val = hash_fnv(0,t);
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

__attribute__((always_inline)) PyObject_t* dict_getitem(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
    PyDict_t *dict = (PyDict_t*)v1[0];
    PyObject_t *key = v1[1];

    auto it = dict->elems->find(key);
    if(it == dict->elems->end()){
        THROW();
    }
    return (*it).second;
}

PyObject_t* builtin_new(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   dprintf("new %lu, %p\n", alen, v1[0]);
   dump(v1[0]);

   PyClass_t *cls = (PyClass_t*)v1[0];

   PyBase_t *obj = (PyBase_t*)malloc(sizeof(PyBase_t));
   obj->vtable = &vtable_object;
   obj->itable = cls->itable;
   obj->cls = cls;
   obj->attrs = new std::unordered_map<std::string,PyObject_t*>();

   dump(obj->itable->dispatch[INIT_SLOT]);

   PyObject_t *args[alen];
   args[0] = obj;
   for(int i=1; i < alen; i++)
     args[i] = v1[i];

   ((PyFunc_t*)obj->itable->dispatch[INIT_SLOT])->code->func(args,alen,0);

   return obj;
/*
   PyFunc_t *bound_func = (PyFunc_t*)malloc(sizeof(PyFunc_t));
   *bound_func = *(PyFunc_t*)(v1[2]);
*/
}

PyObject_t* builtin_buildclass(PyObject_t **v1, uint64_t alen, PyTuple_t **v2){
   dprintf("Buildclass\n");
   dprintf("%p %p %p\n", v1[0], v1[1], v1[2]);
   dump(v1[0]);
   dump(v1[1]);
   dump(v1[2]);

   PyFunc_t *constructor = (PyFunc_t*)(v1[0]);
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

   dprintf("Locals: %p\n", values);
   for(int i=0; i < values->sz; i++){
      dprintf("L: %p %p\n", values->objs[i], values->objs[i]->vtable);
      //dump(values->objs[i]);

      PyStr_t *str = (PyStr_t*)constructor->code->locals->objs[i];
      dump(str);
      const SlotResult *res = in_word_set(str->str,str->sz-1);
      if(res){
         dprintf("L: %d\n", res->slot_num);
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
