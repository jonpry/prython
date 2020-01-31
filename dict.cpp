#include "main.h"

extern "C" {

static PyObject_t* dict_items_int(PyObject_t **v1, int type){
   printf("Items\n");
   PyDict_View_t *ret = (PyDict_View_t*)malloc(sizeof(PyDict_View_t));
   ret->dict = (PyDict_t*)v1[0];
   ret->vtable = &vtable_dict_view;
   ret->itable = 0;
   ret->type = type;
   return ret;
}

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

PyObject_t* dict_len(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   PyDict_t *dict = (PyDict_t*)v1[0];
   return make_int(dict->elems->size());
}


PyObject_t* dict_items(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   return dict_items_int(v1,DICTVIEW_ITEMS);
}

PyObject_t* dict_keys(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   return dict_items_int(v1,DICTVIEW_KEYS);
}

PyObject_t* dict_values(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   return dict_items_int(v1,DICTVIEW_VALUES);
}


PyObject_t* dict_view_iter(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   PyDict_Iterator_t *ret = (PyDict_Iterator_t*)malloc(sizeof(PyDict_Iterator_t));
   ret->view = (PyDict_View_t*)v1[0];
   ret->it = ret->view->dict->elems->begin();
   ret->vtable = &vtable_dict_iter;
   ret->itable = 0;
   ret->cls=0;
   return ret;
}

PyObject_t* dict_iter_next(PyObject_t **v1, uint64_t alen, const PyCtx_t *v2){
   PyDict_Iterator_t *iter = (PyDict_Iterator_t*)v1[0];
   if(iter->it == iter->view->dict->elems->end()){
      THROW();
   } 
   PyObject_t *ret = 0;
   if(iter->view->type == DICTVIEW_KEYS){
      ret = (*(iter->it)).first;
   }else if(iter->view->type == DICTVIEW_VALUES){
      ret = (*(iter->it)).second;
   }else{ //Items
      PyObject_t *tmp[2] = {(*(iter->it)).first,(*(iter->it)).second};
      ret = make_tuple(tmp,2);
   }
   iter->it++;
   return ret;
}

} //Extern "C"
