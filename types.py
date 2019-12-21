#! /usr/bin/python3
from llvmlite import ir
from llvmlite import binding as llvm

############## Simple type decl stuff
int1 = ir.IntType(1)
int8 = ir.IntType(8)
int32 = ir.IntType(32)
int64 = ir.IntType(64)
char = ir.IntType(8)
pchar = char.as_pointer()
dbl = ir.DoubleType()

############## Strings of different length require different types for static initializers
str_types = {}
def make_str_type(l,name=None):
   global int64,char,str_types,pyobj_type
   if l in str_types:
      return str_types[l]
   t = ir.global_context.get_identified_type(name if name else ("PyStr" + str(l)))
   t.set_body(pyobj_type, int64, ir.ArrayType(char,l))
   p = t.as_pointer()
   str_types[l] = (t,p)
   return t,p

############## Tuples of different length require different types for static initializers
tuple_types = {}
def make_tuple_type(l,name=None):
   global int64,char,tuple_types,pyobj_type,ppyobj_type
   if l in tuple_types:
      return tuple_types[l]
   t = ir.global_context.get_identified_type(name if name else ("PyTuple" + str(l)))
   t.set_body(pyobj_type, int64, ir.ArrayType(ppyobj_type,l))
   p = t.as_pointer()
   tuple_types[l] = (t,p)
   return t,p

############## Types for integral python types
vtable_type = ir.global_context.get_identified_type("struct.vtable_t")
pvtable_type = vtable_type.as_pointer() 

pyobj_type = ir.global_context.get_identified_type("class.pyobj")
pyobj_type.set_body(pvtable_type,pvtable_type)
ppyobj_type = pyobj_type.as_pointer()
pppyobj_type = ppyobj_type.as_pointer()

pyfunc_type = ir.global_context.get_identified_type("class.pyfunc")
ppyfunc_type = pyfunc_type.as_pointer()

fnty = ir.FunctionType(ppyobj_type, (pppyobj_type, int64, ppyobj_type))
vlist = [ir.IntType(64),ir.ArrayType(ppyobj_type,len(magic_methods))]
vtable_type.set_body(*vlist)

pyint_type = ir.global_context.get_identified_type("PyInt")
pyint_type.set_body(pyobj_type, int64)
ppyint_type = pyint_type.as_pointer()

pyfloat_type = ir.global_context.get_identified_type("PyFloat")
pyfloat_type.set_body(pyobj_type, dbl)
ppyfloat_type = pyfloat_type.as_pointer()

pycode_type = ir.global_context.get_identified_type("class.pycode")
pycode_type.set_body(pyobj_type, fnty.as_pointer())
ppycode_type = pycode_type.as_pointer()

pystr_type, ppystr_type = make_str_type(0,"class.pystr")

pyfunc_type.set_body(pyobj_type, ppycode_type, ppystr_type, make_tuple_type(0,"class.pytuple")[1])

pyclass_type = ir.global_context.get_identified_type("PyClass")
pyclass_type.set_body(pyfunc_type)
ppyclass_type = pyclass_type.as_pointer()

pybool_type = ir.global_context.get_identified_type("PyBool")
pybool_type.set_body(pyobj_type, int64)
ppybool_type = pybool_type.as_pointer()

pyexc_type = ir.global_context.get_identified_type("PyExc")
pyexc_type.set_body(pyobj_type)
ppyexc_type = pyexc_type.as_pointer()

pynoimp_type = ir.global_context.get_identified_type("PyNotImplemented")
pynoimp_type.set_body(pyobj_type)
ppynoimp_type = pynoimp_type.as_pointer()

pylist_type = ir.global_context.get_identified_type("PyList")
pylist_type.set_body(pyobj_type,int64,int64,pppyobj_type)
ppylist_type = pylist_type.as_pointer()

noimp = ir.GlobalVariable(module,pynoimp_type,"global_noimp")
const_map[(type(noimp),noimp) ] = noimp

tp_pers = ir.FunctionType(int32, (), var_arg=True)
pers = ir.Function(module, tp_pers, '__gxx_personality_v0')

