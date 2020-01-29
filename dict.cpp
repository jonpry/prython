#include "main.h"

extern "C" {

__attribute__((always_inline)) PyObject_t* dict_getitem(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){
    PyDict_t *dict = (PyDict_t*)v1[0];
    PyObject_t *key = v1[1];

    auto it = dict->elems->find(key);
    if(it == dict->elems->end()){
        THROW();
    }
    return (*it).second;
}

PyObject_t* dict_str(PyObject_t **v1, uint64_t alen, PyCtx_t *v2){
    char left = '{';
    char right = '}';
    PyDict_t *t = (PyDict_t*)(v1[0]);
    PyStr_t *strs[t->elems->size()*2];
    size_t total_sz=0;
    int i=0;
    for(auto it=t->elems->begin(); it != t->elems->end(); it++){
       strs[i*2] = (PyStr_t*)unop((*it).first,STR_SLOT);
       strs[i*2+1] = (PyStr_t*)unop((*it).second,STR_SLOT);
       total_sz += strs[i*2]->sz;
       total_sz += strs[i++*2+1]->sz;
    }
    uint64_t str_sz = total_sz + 2 + (t->elems->size() - 1)*2 + t->elems->size()*3;
    PyStr_t *ret = (PyStr_t*)malloc(sizeof(PyStr_t) + str_sz + 1);
    ret->sz = str_sz;
    ret->vtable = &vtable_str;
    ret->itable = 0;

    uint64_t pos=1;
    ret->str[0] = left;
    for(uint64_t i=0; i < t->elems->size(); i++){
       memcpy(ret->str+pos,strs[i*2]->str,strs[i*2]->sz);
       pos += strs[i*2]->sz;
       ret->str[pos++] = ' ';      
       ret->str[pos++] = ':';      
       ret->str[pos++] = ' ';      

       memcpy(ret->str+pos,strs[i*2+1]->str,strs[i*2+1]->sz);
       pos += strs[i*2+1]->sz;

       if(i!=t->elems->size()-1){
          ret->str[pos++] = ',';      
          ret->str[pos++] = ' ';      
       }
    }
    ret->str[pos++] = right;      
    ret->str[pos++] = 0;      

    return ret;

}


} //Extern "C"
