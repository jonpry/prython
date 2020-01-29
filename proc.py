#! /usr/bin/python3
import dis, marshal, struct, sys, time, types, warnings, bisect
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

from llvmlite import ir
from llvmlite import binding as llvm

from dump import show_file, codes
import emph

USAGE = "  usage: %s <PYC FILENAME>" % sys.argv[0]

if len(sys.argv) == 1:
    sys.exit("Error: Too few arguments\n%s" % USAGE)
if len(sys.argv) > 2:
    warnings.warn("Ignoring extra arguments: %s" % (sys.argv[2:],))

if sys.argv[1] == "-h":
    print(USAGE)
else:
    show_file(sys.argv[1])

debug_prints = True

############## End Stuff to print out pyc file

module = ir.Module(name=__file__)
module.triple = "x86_64-pc-linux-gnu"
td = llvm.create_target_data("e-m:e-i64:64-f80:128-n8:16:32:64-S128")

from irtypes import *
from magic import *

const_map = {}

noimp = ir.GlobalVariable(module,pynoimp_type,"global_noimp")
const_map[(type(noimp),noimp) ] = noimp

tp_pers = ir.FunctionType(int32, (), var_arg=True)
pers = ir.Function(module, tp_pers, '__gxx_personality_v0')

i=0

def get_int_array(a):
   const_idx = len(const_map)
   t = ir.ArrayType(int32,len(a))
   g = ir.GlobalVariable(module,t,"global_" + str(const_idx))
   const_map["global_" + str(const_idx)] = g
   g.initializer = t([int32(v) for v in a])
   return g

class clz:
   def __init__(self,name,ary,funcs):
      self.name = name
      self.ary = ary
      self.funcs = funcs

def obj_con(s):
  return [vtable_map[s],pvtable_type(None),ppyclass_type(None)]

############## This function creates a global variables decleration for any compile time constants
def get_constant(con,name=""):
   const_idx = len(const_map)
   tup = (type(con),con)
   if tup in const_map:
      return const_map[tup]

   if con is None:
      const_map[tup] = None
      return None
        
   if type(con) == int:
      g = ir.GlobalVariable(module,pyint_type,"global_" + str(const_idx))
      const_map[tup] = g
      g.initializer = pyint_type([obj_con('int'),ir.Constant(int64,con)])
   elif type(con) == bool:
      g = ir.GlobalVariable(module,pybool_type,"global_" + str(con).lower())
      const_map[tup] = g
      g.initializer = pybool_type([obj_con('bool'),ir.Constant(int64,int(con))])
   elif type(con) == float:
      g = ir.GlobalVariable(module,pyfloat_type,"global_" + str(const_idx))
      const_map[tup] = g
      g.initializer = pyfloat_type([obj_con('float'),ir.Constant(dbl,con)])
   elif isinstance(con,str):
      t,p = make_str_type(len(con)+1)
      g = ir.GlobalVariable(module,t,"global_" + str(const_idx) + name)
      const_map[tup] = g
      g.initializer = t([obj_con('str'),int64(len(con)),ir.Constant(ir.ArrayType(char,len(con)+1),bytearray(con + "\0",'utf8'))])
   elif isinstance(con, types.CodeType):
      g = ir.GlobalVariable(module,pycode_type,"global_" + str(const_idx))
      const_map[tup] = g
      g.initializer = pycode_type([obj_con('code'), func_map[con], table_map[con], get_constant(con.co_names).bitcast(make_tuple_type(0)[1])])
   elif isinstance(con, tuple):
      t,p = make_tuple_type(len(con))
      g = ir.GlobalVariable(module,t,"global_" + str(const_idx))
      const_map[tup] = g
      cons = []
      for c in con:
        c = get_constant(c)
        cons.append(c.bitcast(ppyobj_type) if c else None)
      g.initializer = t([obj_con('tuple'), int64(len(con)), ir.ArrayType(ppyobj_type,len(con))(cons)])    
   elif isinstance(con, ir.Function):
      c = ir.GlobalVariable(module,pycode_type,"global_" + str(const_idx))      
      const_map[c] = c
      c.global_constant = True
      c.initializer = pycode_type([obj_con('code'),con,lfnty.as_pointer()(None), make_tuple_type(0)[1](None)])

      g = ir.GlobalVariable(module,pyfunc_type,"pyfunc_" + con.name)      
      const_map[tup] = g
      g.initializer = pyfunc_type([obj_con('func'),c,get_constant(con.name).bitcast(ppystr_type),make_tuple_type(0)[1](None), make_tuple_type(0)[1](None), ppyclass_type(None)])
   elif isinstance(con,clz):
      lookup = make_lookup(con.name + "_lookup",con.ary)
      name = get_constant(con.name).bitcast(ppystr_type)
      locs = get_constant(con.ary).bitcast(ppytuple_type)
      tgts = get_constant(con.funcs).bitcast(ppytuple_type)
      g = ir.GlobalVariable(module,pyclass_type,"pyclass_" + con.name)      
      const_map[tup] = g
      g.initializer = pyclass_type([obj_con(con.name),name,ppyfunc_type(None),lookup,locs,tgts,int64(0),ir.ArrayType(ppyclass_type,0)(None)])
   elif isinstance(con,ir.values.Constant):
      return con
   else:
      print(type(con))
      assert(False)   

   g.global_constant = True
   return g


#Add all the members to the various vtables
integrals = {"int" : { "mul" , "add", "xor", "or", "and", "radd", "mod", "floordiv", "float", "str", "gt", "lt", "le", "ge", "ne", "eq", "neg" }, 
             "float" : { "mul", "add", "sub", "pow", "radd", "rmul", "rsub", "rpow", "mod", "truediv", "rmod", "rtruediv",
                         "floordiv", "rfloordiv", "str"},
             "tuple" : { "str", "getitem", "len" }, 
             "str" : { "add", "str", "getitem", "hash"}, 
             "code" : {}, 
             "func" : { "str"}, 
             "class" : {}, 
             "bool" : { "str" }, 
             "NotImplemented" : {},
             "exception" : {},
             "list" : { "str", "iter", "getitem", "en"}, 
             "dict" : { "getitem", "str", "len"}, 
             "object" : {},
             "list_iter" : {"next"},
             "dict_view" : { "iter" },
             "dict_iter" : { "next" },
             "slice" : {}, 
             "cell" : {} }

vtable_map = {}
table_map = {}

for t in integrals.keys():
   g = ir.GlobalVariable(module,vtable_type,"vtable_" + t)
   vtable_map[t] = g

for t in integrals.keys():
   g = vtable_map[t]
   vinit = [int64(1<<i),[]]
   for m in magic_methods:
      if m in integrals[t]:
        vinit[1].append(get_constant(ir.Function(module, fnty, name=t + "_" + m)).bitcast(ppyobj_type))
      else:
        vinit[1].append(noimp.bitcast(ppyobj_type))
   g.initializer = vtable_type(vinit)
   g.global_constant = True
   i+=1

noimp.initializer = pynoimp_type([obj_con('NotImplemented')])

############## These functions are implemented in C
malloc_type = ir.FunctionType(int8.as_pointer(), (int64,))
malloc = ir.Function(module, malloc_type, name="malloc")
malloc.return_value.add_attribute("nonnull")

stacksave_type = ir.FunctionType(int8.as_pointer(), [])
stacksave = ir.Function(module, stacksave_type, name="llvm.stacksave")

stackrestore_type = ir.FunctionType(ir.VoidType(), (int8.as_pointer(),))
stackrestore = ir.Function(module, stackrestore_type, name="llvm.stackrestore")

printf_type = ir.FunctionType(int8.as_pointer(), (int8.as_pointer(),), var_arg=True)
printf = ir.Function(module, printf_type, name="printf")

import_name_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type, ppyobj_type))
import_name = ir.Function(module, import_name_type, name="import_name")

load_name_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type))
load_name = ir.Function(module, load_name_type, name="load_name")

unpack_sequence_type = ir.FunctionType(ppyobj_type, (pppyobj_type, int64, ppyobj_type))
unpack_sequence = ir.Function(module, unpack_sequence_type, name="unpack_sequence")

list_append_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type))
list_append = ir.Function(module, list_append_type, name="list_append")

store_subscr_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type, ppyobj_type))
store_subscr = ir.Function(module, store_subscr_type, name="store_subscr")

local_lookup_type = ir.FunctionType(int32, (make_str_type(0)[1], make_tuple_type(0)[1], ir.ArrayType(int32,0).as_pointer(),ir.ArrayType(int32,0).as_pointer(), int32))
local_lookup = ir.Function(module, local_lookup_type, name="local_lookup")

std_builtins = ["print","buildclass","repr","str","slice","new","len","hash","getattr","setattr","exit","super"]
for builtin in std_builtins:
    globals()['builtin_' + builtin] = ir.Function(module, fnty, name="builtin_" + builtin)
    globals()['builtin_' + builtin].attributes.add("uwtable")

build_map_type = ir.FunctionType(ppyobj_type,(pppyobj_type,int32))
build_map = ir.Function(module, build_map_type, name="build_map")

build_const_key_map_type = ir.FunctionType(ppyobj_type,(pppyobj_type,int32, ppyobj_type))
build_const_key_map = ir.Function(module, build_const_key_map_type, name="build_const_key_map")


dict_items = ir.Function(module, fnty, name="dict_items")
dict_items.attributes.add("uwtable")

dict_values = ir.Function(module, fnty, name="dict_values")
dict_values.attributes.add("uwtable")

dict_keys = ir.Function(module, fnty, name="dict_keys")
dict_keys.attributes.add("uwtable")


begin_catch_type = ir.FunctionType(ir.VoidType(), [])
begin_catch = ir.Function(module, begin_catch_type, "__cxa_begin_catch")
end_catch = ir.Function(module, begin_catch_type, "__cxa_end_catch")
resume_type = ir.FunctionType(ir.VoidType(), [int8.as_pointer()])
resume = ir.Function(module, resume_type, "_Unwind_Resume")

#TODO: these functions are no good and should ideally use something like unop. need to check itable and potential class members 
name_to_slots = ["repr","str"]
for name in name_to_slots:
   func = locals()["builtin_" + name]
   block = func.append_basic_block(name="entry")
   builder = ir.IRBuilder(block)
   print(func.args[0])
   v = builder.load(builder.gep(func.args[0],(int32(0),)))
   print(v)
   v = builder.load(builder.gep(v,(int32(0),int32(0))))
   print(v)
   p = builder.load(builder.gep(v,(int32(0),int32(1),int32(magic_methods.index(name)))))
   print(p)
   p = builder.bitcast(p,ppyfunc_type)
   print(p)
   c = builder.load(builder.gep(p,(int32(0),int32(1))))
   print(c)
   c = builder.load(builder.gep(c,(int32(0),int32(1))))
   print(c)
   builder.ret(builder.call(c,func.args))

builtin_print_wrap = ir.Function(module, fnty, name="builtin_print_wrap")
#builtin_print_wrap.attributes.add("noalias")
builtin_print_wrap.attributes.add("noinline")
builtin_print_wrap.attributes.add("uwtable")
super(ir.values.AttributeSet,builtin_print_wrap.args[0].attributes).add("readonly")
super(ir.values.AttributeSet,builtin_print_wrap.args[0].attributes).add("nocapture")
super(ir.values.AttributeSet,builtin_print_wrap.args[2].attributes).add("readonly")
super(ir.values.AttributeSet,builtin_print_wrap.args[2].attributes).add("nocapture")

block = builtin_print_wrap.append_basic_block(name="entry")
builder = ir.IRBuilder(block)
builder.ret(builder.call(builtin_print,builtin_print_wrap.args))

binop_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type, int32, int32))
binop = ir.Function(module, binop_type, name="binop")
binop.attributes.add("uwtable")


unop_type = ir.FunctionType(ppyobj_type, (ppyobj_type, int32))
unop = ir.Function(module, unop_type, name="unop")
unop.attributes.add("uwtable")

truth_type = ir.FunctionType(int1, (ppyobj_type,))
truth = ir.Function(module, truth_type, name="truth")
truth.attributes.add("uwtable")

call_function = ir.Function(module,fnty, name="call_function")
call_function.attributes.add("uwtable")

di_file = module.add_debug_info("DIFile", {
    "filename":        "test.py",
    "directory":       "",
})

di_compileunit = module.add_debug_info("DICompileUnit", {
    "language":        ir.DIToken("DW_LANG_Python"),
    "file":            di_file,
    "producer":        "Prython",
    "runtimeVersion":  0,
    "isOptimized":     True,
  }, is_distinct=True)

module.add_named_metadata("llvm.dbg.cu").add(di_compileunit)
module.add_named_metadata("llvm.ident").add(module.add_metadata(["Prython"]))
flags = module.add_named_metadata("llvm.module.flags")
flags.add(module.add_metadata([int32(2), "Dwarf Version", int32(4)]))
flags.add(module.add_metadata([int32(2), "Debug Info Version", int32(3)]))
flags.add(module.add_metadata([int32(1), "wchar_size", int32(4)]))

############## Helper code

def debug(builder, s, *args):
  if not debug_prints:
     return
  alist = [builder.gep(get_constant(s + "\n","debug_" + s),(int32(0),int32(2),int32(0)))]
  builder.call(printf,alist)

def pyslot(builder, o, slot):
  vtable = builder.load(builder.gep(o,(int32(0),int32(0))))
  f = builder.gep(vtable,(int32(0),int32(1),int32(magic_methods.index(slot))))
  return builder.load(f)

def binary_op(func,builder,stack_ptr,op,reverse=True):
  v1 = builder.load(stack[stack_ptr-1])
  stack_ptr-=1
  v2 = builder.load(stack[stack_ptr-1])
  stack_ptr-=1

  v = stack[stack_ptr]
  args = (v2,v1,int32(magic_methods.index(op)),int32(magic_methods.index("r" + op) if reverse else -1))
  if len(except_stack):
     newblock,builder,rval = invoke(func,builder,binop,args)
     replace_block(ins,block_idx,newblock,builder)
  else:
     rval=builder.call(binop,args)
  builder.store(rval,v)

  stack_ptr+=1
  return (stack_ptr,builder)

def unary_op(func,builder,stack_ptr,op,pop=True):
  v1 = builder.load(stack[stack_ptr-1])
  if pop:
     stack_ptr-=1

  v = stack[stack_ptr]
  args = (v1,int32(magic_methods.index(op)))
  if len(except_stack):
     newblock,builder,rval = invoke(func,builder,unop,args)
     replace_block(ins,block_idx,newblock,builder)
     builder.store(rval,stack[stack_ptr])
  else:
     builder.store(builder.call(unop,args),v)
  stack_ptr+=1
  return (stack_ptr,builder)

pad_map = {}
def get_lpad(func):
   global pad_map, block_num
   if except_stack[-1] in pad_map:
     return pad_map[except_stack[-1]]
   lpad = func.append_basic_block(name="lpad" + str(block_num+1))
   block_num +=1 
   lbuilder = ir.IRBuilder(lpad) 
   lp = lbuilder.landingpad(ir.LiteralStructType([int8.as_pointer(),int64]), 'lp')
   lp.add_clause(ir.CatchClause(int8.as_pointer()(None)))

   lbuilder.call(begin_catch,[])
   lbuilder.call(end_catch,[])
   
   rpad = func.append_basic_block(name="lpad" + str(block_num+1) + "_tgt")
   lbuilder.branch(rpad)
   rbuilder = ir.IRBuilder(rpad)
   

   #lbuilder.call(resume,[lbuilder.extract_value(lp, 0)])
   #lbuilder.resume(lp)
   pad_map[except_stack[-1]] = (lpad,lbuilder,rpad,rbuilder)
   return pad_map[except_stack[-1]]

def invoke(func,builder,target,args):
   global block_num

   lpad = get_lpad(func)[0]
   newblock = func.append_basic_block(name="block" + str(block_num+1))
   print(newblock.name)
   #blocks_by_ofs[ins.offset] = newblock
   block_num += 1
   rval = builder.invoke(target, args, newblock, lpad)
   builder = ir.IRBuilder(newblock)
   return newblock,builder,rval

def make_lookup(name,ary):
   #build tables
   func = ir.Function(module, lfnty, name=name)
   func.attributes.personality = pers
   func.attributes.add("alwaysinline")
   func.attributes.add("uwtable")
   block = func.append_basic_block(name="entry")
   builder = ir.IRBuilder(block)

   g,v = emph.CreateMinimalPerfectHash({ary[i]:i for i in range(len(ary))})
   l = len(g)
   g = builder.bitcast(get_int_array(g),ir.ArrayType(int32,0).as_pointer())
   v = builder.bitcast(get_int_array(v),ir.ArrayType(int32,0).as_pointer())
   strs = builder.bitcast(get_constant(ary),make_tuple_type(0)[1])
   
   builder.ret(builder.call(local_lookup,(func.args[0],strs,g,v,int32(l))))
   return func

get_constant(clz("dict",("items","keys","values"),(dict_items,dict_keys,dict_values)))

def pop_and_call(builder,func,stack_ptr, tgt, stack_vals, constants):
   savestack = builder.call(stacksave,[])

   args = builder.alloca(ppyobj_type,len(stack_vals) + len(constants))

   for de in range(len(stack_vals)):
      v1 = builder.load(stack[stack_ptr-1-stack_vals[de]])
      builder.store(v1,builder.gep(args,(int32(de),)))
  
   stack_ptr-=len(stack_vals)

   for de in range(len(constants)):
      builder.store(constants[de],builder.gep(args,(int32(len(stack_vals)+de),)))

   v = stack[stack_ptr]

   if len(except_stack):
      newblock,builder,rval = invoke(func,builder,tgt,(args,int64(len(stack_vals)+len(constants)),ppyobj_type(None)))
      replace_block(ins,block_idx,newblock,builder)
      builder.store(rval,stack[stack_ptr])
   else:
      rval = builder.call(tgt,(args,int64(len(stack_vals)+len(constants)),ppyctx_type(None)))

   builder.store(rval,stack[stack_ptr])
   stack_ptr+=1
   builder.call(stackrestore,[savestack])
   return stack_ptr

############## Enumerate all function definitions
i=0
codes.reverse()
func_map = {}
for c in codes:
   print("*************")
   func = ir.Function(module, fnty, name="code_blob_" + str(i))
   func.attributes.personality = pers
   func.attributes.add("alwaysinline")
   func.attributes.add("uwtable")

   #personality i8* bitcast (i32 (...)* @__gxx_personality_v0 to i8*)
   func_map[c] = func

   #build tables
   table_map[c] = make_lookup("code_blob_" + str(i) + "_locals", c.co_names)
   i+=1

get_constant(False)
get_constant(True)

#Apply get_constant to constant table contents
i=0
for c in codes:
   for con in c.co_consts:
      get_constant(con)
   for con in c.co_cellvars:
      get_constant(con)

i=0

di_func_type = module.add_debug_info("DISubroutineType", {
        # None as `null`
        "types":           module.add_metadata([None]),
     })


def replace_block(ins,block_idx,newblock,builder):
   #blocks_by_ofs[ins.offset] = newblock
   blocks[block_idx][2] = newblock
   blocks[block_idx][3] = builder                

builtin_names = ["buildclass", "str", "repr", "getattr", "setattr", "print_wrap", "new", "len", "hash", "exit", "super"]
for n in builtin_names:
   get_constant(locals()['builtin_' + n])

############## Emit llvm for each code section
for c in codes:
   print("*************")
   func = func_map[c]

   di_func = module.add_debug_info("DISubprogram", {
            "name":            "my_func",
            "file":            di_file,
            "line":            c.co_firstlineno,
            "type":            di_func_type,
            "isLocal":         False,
            "unit":            di_compileunit,
            }, is_distinct=True)


   block = func.append_basic_block(name="entry")
   builder = ir.IRBuilder(block) 

   stack = []
   for s in range(c.co_stacksize):
      s = builder.alloca(ppyobj_type,1)
      builder.store(ir.Constant(ppyobj_type,None),s)
      stack.append(s)
   local = []
   if c.co_argcount:
       args = func.args[0]
   nargs = func.args[1]  #TODO:
   for s in range(c.co_nlocals):
      l = builder.alloca(ppyobj_type,1)
      if s < c.co_argcount:
         builder.store(builder.load(builder.gep(args,(int32(s),))),l)
      else:
         builder.store(noimp.bitcast(ppyobj_type),l)
      local.append(l)

   name = []
   for s in range(len(c.co_names)):
      l = builder.alloca(ppyobj_type,1)
      builder.store(noimp.bitcast(ppyobj_type),l)
      name.append(l)

   cell = []
   for s in range(len(c.co_cellvars) + len(c.co_freevars)):
      l = builder.alloca(ppyobj_type,1)
      builder.store(noimp.bitcast(ppyobj_type),l)
      cell.append(l)


   blocks = [[0,0,block,builder]]
   blocks_by_ofs = {0: block}
   block_num=0
   ins_idx=0
   nxt=False
   for ins in dis.get_instructions(c):
      if ins.is_jump_target or nxt:
        b = func.append_basic_block(name="block" + str(block_num+1))
        blocks.append([ins_idx,block_num+1,b,ir.IRBuilder(b)])
        blocks_by_ofs[ins.offset] = b
        block_num += 1
        nxt=False
      if ins.opname=='POP_JUMP_IF_FALSE' or ins.opname=='POP_JUMP_IF_TRUE' or ins.opname=='SETUP_EXCEPT' or ins.opname=='JUMP_FORWARD' or ins.opname=='JUMP_ABSOLUTE' or ins.opname=='SETUP_LOOP' or ins.opname=='BREAK_LOOP':
        nxt=True
      ins_idx +=1

   ins_idxs = [r[0] for r in blocks] 
   stack_ptr = 0
   ins_idx=0
   branch_stack = {}
   except_stack = []
   finally_stack = []
   loop_stack = []
   loop_tgt = {}
   for_stack = {}
   unreachable = False

   for ins in dis.get_instructions(c):
       print(ins)
       a,block_idx,block,builder = blocks[bisect.bisect_right(ins_idxs,ins_idx)-1]

       print(block.name)


       if unreachable and (not ins.is_jump_target):
          ins_idx+=1
          func.blocks.remove(block)
          print("Unreachable")
          continue
       unreachable=False

       did_jmp=False  
       if ins.starts_line:
          builder.debug_metadata = builder.module.add_debug_info("DILocation", {
            "line":  ins.starts_line,
            "scope": di_func,
            })
       if ins.offset in branch_stack:
          stack_ptr = branch_stack[ins.offset]
          print("Restoring stack_ptr: " + str(stack_ptr))

       if ins.offset in except_stack: #This is a catch block
          assert(ins.offset == except_stack[-1]) #assumption not sure if true
          func.blocks.remove(block)
          newblock,builder = get_lpad(except_stack[-1])[2:4]
          replace_block(ins,block_idx,newblock,builder)
          if not ins.offset in for_stack:
             stack_ptr += 3
          print("$$$$$ " + str(ins))

       if ins.offset in loop_stack:
          except_stack = except_stack[:-1]

       debug(builder,"ins " + str(ins.offset))

       oldstuff = None
       if 42 in branch_stack:
          oldstuff = branch_stack[42]

       save_stack_ptr = stack_ptr
       if ins.opname=='LOAD_CONST':
         print(stack_ptr)
         v = stack[stack_ptr]
         tbl = ins.argval
         tup = (type(tbl),tbl)
         if const_map[tup] is None:
            builder.store(ir.Constant(ppyobj_type,None),v)
         else:
            builder.store(builder.bitcast(const_map[tup],ppyobj_type),v)
         stack_ptr+=1
       elif ins.opname=='LOAD_FAST' or ins.opname=='LOAD_NAME': #TODO: this is all wrong, name first searches locals then globals then builtin. name[ins.arg] may not even be a thing
         print("stack_ptr: " + str(stack_ptr))       
         v = stack[stack_ptr]
         #TODO: use invoke
         tbl = builder.call(load_name, (builder.load((local if ins.opname=="LOAD_FAST" else name)[ins.arg]),builder.bitcast(get_constant(ins.argval),ppyobj_type)))
         builder.store(tbl,v)
         stack_ptr+=1
       elif ins.opname=='LOAD_GLOBAL': #TODO
         v = stack[stack_ptr]
         tbl = builder.call(load_name, (ppyobj_type(None),builder.bitcast(get_constant(ins.argval),ppyobj_type)))
         builder.store(tbl,v)
         stack_ptr+=1
       elif ins.opname=='LOAD_DEREF':  #TODO: perform get on the pycell
         v = stack[stack_ptr]
         builder.store(builder.load(cell[ins.arg]),v)
         stack_ptr+=1
       elif ins.opname=='LOAD_CLOSURE': 
         v = stack[stack_ptr]
         builder.store(builder.load(cell[ins.arg]),v)
         stack_ptr+=1
       elif ins.opname=='STORE_FAST' or ins.opname=="STORE_NAME":
         v = builder.load(stack[stack_ptr-1])
         tbl = (local if ins.opname=="STORE_FAST" else name)[ins.arg]
         builder.store(v,tbl)
         stack_ptr-=1
       elif ins.opname=='IMPORT_NAME': #TODO
         v1 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v2 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v3 = builder.bitcast(get_constant(ins.argval),ppyobj_type)
         v = stack[stack_ptr]
         builder.store(builder.call(import_name,(v1,v2,v3)),v)
         stack_ptr+=1
       elif ins.opname=='POP_TOP':
         stack_ptr-=1
       elif ins.opname=='BUILD_TUPLE':
         t,p = make_tuple_type(ins.arg)
         obj = builder.bitcast(builder.call(malloc,[int64(p.pointee.get_abi_size(td))]),p)
         builder.store(vtable_map["tuple"],builder.gep(obj,(int32(0),int32(0),int32(0))))
         builder.store(pvtable_type(None),builder.gep(obj,(int32(0),int32(0),int32(1))))
         builder.store(ppyclass_type(None),builder.gep(obj,(int32(0),int32(0),int32(2))))
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(1))))
         for te in range(ins.arg):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(obj,(int32(0),int32(2),int32(te))))
            stack_ptr-=1
         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='BUILD_LIST':
         obj = builder.bitcast(builder.call(malloc,[int64(pylist_type.get_abi_size(td))]),ppylist_type)
         builder.store(vtable_map["list"],builder.gep(obj,(int32(0),int32(0),int32(0))))
         builder.store(pvtable_type(None),builder.gep(obj,(int32(0),int32(0),int32(1))))
         builder.store(ppyclass_type(None),builder.gep(obj,(int32(0),int32(0),int32(2))))
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(1))))
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(2))))
         data = builder.bitcast(builder.call(malloc,[int64(pppyobj_type.get_abi_size(td))]),pppyobj_type)
         builder.store(data,builder.gep(obj,(int32(0),int32(3))))

         for te in range(ins.arg):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(data,(int32(ins.arg-te-1),)))
            stack_ptr-=1
         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='BUILD_MAP':
        
         savestack = builder.call(stacksave,[])
         args = builder.alloca(ppyobj_type,ins.arg*2)

         for de in range(ins.arg*2):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(args,(int32(de),)))
            stack_ptr-=1

         obj = builder.call(build_map,(args,int32(ins.arg)))

         builder.store(obj,stack[stack_ptr])
         stack_ptr+=1
         builder.call(stackrestore,[savestack])
       elif ins.opname=='BUILD_CONST_KEY_MAP':
         keys = builder.load(stack[stack_ptr-1])
         stack_ptr-=1

         savestack = builder.call(stacksave,[])
         args = builder.alloca(ppyobj_type,ins.arg)

         for de in range(ins.arg):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(args,(int32(ins.arg-de-1),)))
            stack_ptr-=1

         obj = builder.call(build_const_key_map,(args,int32(ins.arg),keys))

         builder.store(obj,stack[stack_ptr])
         stack_ptr+=1
         builder.call(stackrestore,[savestack])

       elif ins.opname=='CALL_FUNCTION' or ins.opname=='CALL_METHOD': 
         savestack = builder.call(stacksave,[])
         args = builder.alloca(ppyobj_type,ins.arg+1)
         cnt = ins.arg+ (2 if ins.opname=='CALL_METHOD' else 1)
         for i in range(cnt): 
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(args,(int32(cnt - i - 1),))) #TODO: i think args are reversed
            stack_ptr-=1
         #debug(builder,"post store " + str(ins.offset))

         if len(except_stack):
            newblock,builder,rval = invoke(func,builder,call_function,(args,int64(ins.arg+1),ppyctx_type(None)))
            replace_block(ins,block_idx,newblock,builder)
            builder.store(rval,stack[stack_ptr])
         else:
            builder.store(builder.call(call_function,(args,int64(ins.arg+1),ppyctx_type(None))),stack[stack_ptr])

         stack_ptr+=1
         builder.call(stackrestore,[savestack])

       elif ins.opname=='MAKE_FUNCTION':
         func_name = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         code = builder.load(stack[stack_ptr-1])
         stack_ptr-=1

         args=None
         if ins.arg == 0:         
            pass
         elif ins.arg == 1:         
            args = builder.load(stack[stack_ptr-1])
            stack_ptr-=1
         elif ins.arg == 8: #TODO: closure args        
            args = builder.load(stack[stack_ptr-1])
            stack_ptr-=1
         else:
            assert(False)

         obj = builder.bitcast(builder.call(malloc,[int64(pyfunc_type.get_abi_size(td))]),ppyfunc_type)
         builder.store(vtable_map['func'],builder.gep(obj,(int32(0),int32(0),int32(0))))
         builder.store(pvtable_type(None),builder.gep(obj,(int32(0),int32(0),int32(1))))
         builder.store(ppyclass_type(None),builder.gep(obj,(int32(0),int32(0),int32(2))))

         builder.store(builder.bitcast(code,ppycode_type),builder.gep(obj,(int32(0),int32(1))))
         builder.store(builder.bitcast(func_name,ppystr_type),builder.gep(obj,(int32(0),int32(2))))
         if args:
            builder.store(builder.bitcast(args,make_tuple_type(0)[1]),builder.gep(obj,(int32(0),int32(3))))
         else:
            builder.store(make_tuple_type(0)[1](None),builder.gep(obj,(int32(0),int32(3))))
         builder.store(make_tuple_type(0)[1](None),builder.gep(obj,(int32(0),int32(4))))
         builder.store(ppyclass_type(None),builder.gep(obj,(int32(0),int32(5))))

         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='RETURN_VALUE':
         debug(builder,"@ret")
         cond = builder.icmp_unsigned("!=",int64(None),builder.ptrtoint(func.args[2],int64))
         tblock = func.append_basic_block(name="block" + str(block_num+1))
         block_num += 1
         fblock = func.append_basic_block(name="block" + str(block_num+1))
         block_num += 1
         tbuild = ir.IRBuilder(tblock)
         fbuild = ir.IRBuilder(fblock)

 
         print(tblock.name)
         print(fblock.name)

         builder.cbranch(cond,tblock,fblock)

         debug(tbuild,"true")
         mem = tbuild.call(malloc, [int64(make_tuple_type(len(c.co_names))[0].get_abi_size(td))])
         mem = tbuild.bitcast(mem,make_tuple_type(len(c.co_names))[1])
         tbuild.store(vtable_map['tuple'], tbuild.gep(mem,[int32(0),int32(0),int32(0)]))        
         tbuild.store(int64(len(c.co_names)), tbuild.gep(mem,[int32(0),int32(1)]))        

         for i in range(len(c.co_names)):
            tbuild.store(tbuild.load(name[i]), tbuild.gep(mem,[int32(0),int32(2),int32(i)]))        
         tbuild.store(tbuild.bitcast(mem,ppytuple_type),tbuild.gep(func.args[2],[int32(0),int32(1)]))
         
         tbuild.ret(tbuild.load(stack[stack_ptr-1]))


         fbuild.ret(fbuild.load(stack[stack_ptr-1]))
         stack_ptr-=1
       elif ins.opname=='LOAD_ATTR': 
         stack_ptr = pop_and_call(builder,func,stack_ptr,builtin_getattr, [0], [builder.bitcast(get_constant(ins.argval),ppyobj_type)])

       elif ins.opname=='STORE_ATTR': 
         v1 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v2 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v3 = builder.bitcast(get_constant(ins.argval),ppyobj_type)

         args = builder.alloca(ppyobj_type,3)
         builder.store(v1,builder.gep(args,(int32(0),)))
         builder.store(v2,builder.gep(args,(int32(2),)))
         builder.store(v3,builder.gep(args,(int32(1),)))

         if len(except_stack):
            newblock,builder,rval = invoke(func,builder,builtin_setattr,(args,int64(3),ppyctx_type(None)))
            replace_block(ins,block_idx,newblock,builder)
         else:
            builder.call(builtin_setattr,(args,int64(3),ppyctx_type(None)))
       elif ins.opname=='STORE_SUBSCR': 
         v1 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v2 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v3 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1

         if len(except_stack):
            newblock,builder,rval = invoke(func,builder,store_subscr,(v1,v2,v3))
            replace_block(ins,block_idx,newblock,builder)
         else:
            rval = builder.call(store_subscr,(v1,v2,v3))

       elif ins.opname=='LOAD_BUILD_CLASS':
         v = stack[stack_ptr]
         builder.store(builder.bitcast(get_constant(builtin_buildclass),ppyobj_type),v)
         stack_ptr+=1
       elif ins.opname=='POP_JUMP_IF_FALSE':
         v = builder.load(stack[stack_ptr-1])
         call = builder.call(truth,(v,))
         builder.cbranch(builder.not_(call),blocks_by_ofs[ins.arg],blocks[block_idx+1][2])
         stack_ptr-=1
         branch_stack[ins.arg] = stack_ptr
         did_jmp = True
       elif ins.opname=='JUMP_FORWARD' or ins.opname=='JUMP_ABSOLUTE':
         builder.branch(blocks_by_ofs[ins.argval])
         branch_stack[ins.argval] = stack_ptr
         did_jmp=True
         unreachable=True

       elif ins.opname=='LIST_APPEND': 
         v = builder.load(stack[stack_ptr-1])
         stack_ptr-=1         
         l = builder.load(stack[stack_ptr-ins.argval])
         builder.call(list_append,(l,v))


       elif ins.opname=='LOAD_METHOD': #TODO
         v1 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v2 = builder.bitcast(get_constant(ins.argval),ppyobj_type)
         v = stack[stack_ptr]

         args = builder.alloca(ppyobj_type,2)
         builder.store(v1,builder.gep(args,(int32(0),)))
         builder.store(v2,builder.gep(args,(int32(1),)))

         if len(except_stack):
            newblock,builder,rval = invoke(func,builder,builtin_getattr,(args,int64(2),ppyobj_type(None)))
            replace_block(ins,block_idx,newblock,builder)
            builder.store(rval,stack[stack_ptr])
         else:
            rval = builder.call(builtin_getattr,(args,int64(2),ppyctx_type(None)))

         builder.store(rval,stack[stack_ptr])
         stack_ptr+=1      
         builder.store(v1,stack[stack_ptr])
         stack_ptr+=1      
       elif ins.opname=='BINARY_MULTIPLY':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"mul")
       elif ins.opname=='BINARY_ADD':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"add")
       elif ins.opname=='BINARY_SUBTRACT':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"sub")
       elif ins.opname=='BINARY_OR':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"or")
       elif ins.opname=='BINARY_AND':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"and")
       elif ins.opname=='BINARY_XOR':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"xor")
       elif ins.opname=='BINARY_LSHIFT':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"lshift")
       elif ins.opname=='BINARY_RSHIFT':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"rshift")
       elif ins.opname=='BINARY_POWER':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"pow")
       elif ins.opname=='BINARY_MODULO':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"mod")
       elif ins.opname=='BINARY_TRUE_DIVIDE':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"truediv")
       elif ins.opname=='BINARY_FLOOR_DIVIDE':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"floordiv")
       elif ins.opname=='BINARY_SUBSCR':
         stack_ptr,builder = binary_op(func,builder,stack_ptr,"getitem",False)
       elif ins.opname=='UNARY_NEGATIVE':
         stack_ptr,builder = unary_op(func,builder,stack_ptr,"neg")
       elif ins.opname=='UNARY_POSITIVE':
         stack_ptr,builder = unary_op(func,builder,stack_ptr,"pos")
       elif ins.opname=='UNARY_INVERT':
         stack_ptr,builder = unary_op(func,builder,stack_ptr,"invert")
       elif ins.opname=="NOP":
            pass
       elif ins.opname=="EXTENDED_ARG":
            pass
       elif ins.opname=="DUP_TOP":
           v1 = builder.load(stack[stack_ptr-1])
           builder.store(v1,stack[stack_ptr])
           stack_ptr+=1
       elif ins.opname=='COMPARE_OP':
         if ins.argval == "exception match": #TODO:
           v1 = builder.load(stack[stack_ptr-1])
           stack_ptr-=1
         elif ins.argval == "is":
           v1 = builder.load(stack[stack_ptr-1])
           stack_ptr-=1
           v2 = builder.load(stack[stack_ptr-1])
           stack_ptr-=1
           p = builder.icmp_unsigned("==",v1,v2)
           builder.store(builder.bitcast(builder.select(p,get_constant(True),get_constant(False)),ppyobj_type),stack[stack_ptr])
           stack_ptr+=1
         else:
           opmap = { '>' : 'gt', '<' : 'lt', '>=' : 'ge', "<=" : 'le' }
           stack_ptr,builder = binary_op(func,builder,stack_ptr,opmap[ins.argval],False)
       elif ins.opname=='SETUP_FINALLY':
           finally_stack.append(ins.argval)       
       elif ins.opname=='END_FINALLY':
           finally_stack = finally_stack[:-1]
       elif ins.opname=='SETUP_EXCEPT':
           except_stack.append(ins.argval)
           branch_stack[ins.argval] = stack_ptr
       elif ins.opname=='SETUP_LOOP':
           branch_stack[ins.argval] = stack_ptr
           loop_stack.append(ins.argval)
           loop_tgt[ins.argval] = ins.offset
       elif ins.opname=='POP_BLOCK':
           pass
       elif ins.opname=='POP_EXCEPT':
           except_stack = except_stack[:-1]
       elif ins.opname=='GET_ITER':
           stack_ptr,builder = unary_op(func,builder,stack_ptr,"iter",True)
       elif ins.opname=='FOR_ITER':
           branch_stack[ins.argval] = stack_ptr-1
           for_stack[ins.argval] = True
           except_stack.append(ins.argval)
           stack_ptr,builder = unary_op(func,builder,stack_ptr,"next",False)  
       elif ins.opname=="BREAK_LOOP":
           tgt = loop_stack[-1]
           builder.branch(blocks_by_ofs[tgt])
           branch_stack[tgt] = stack_ptr-1
           did_jmp=True
           unreachable=True
       elif ins.opname=="UNPACK_SEQUENCE": 
           v1 = builder.load(stack[stack_ptr-1])
           stack_ptr-=1

           savestack = builder.call(stacksave,[])
           args = builder.alloca(ppyobj_type,ins.arg)

           builder.call(unpack_sequence, [args, int64(ins.arg), v1])

           for de in range(ins.arg):
              builder.store(builder.load(builder.gep(args,(int32(de),))),stack[stack_ptr])
              stack_ptr+=1

           builder.call(stackrestore,[savestack])
       elif ins.opname=='BUILD_SLICE': 
           stack_ptr = pop_and_call(builder,func,stack_ptr,builtin_slice, range(ins.arg), [])
       else:
           assert(False)

       if 42 in branch_stack and branch_stack[42] != oldstuff:
          print("br stack: " + str(branch_stack[42]) + ", " + str(oldstuff))

       print(block.name)

       if (   ins.opname!='SETUP_FINALLY' and 
              ins.opname!='SETUP_EXCEPT' and
              ins.opname!='POP_EXCEPT' and
              ins.opname!='END_FINALLY' and
              ins.opname!='EXTENDED_ARG' and
              ins.opname!='SETUP_LOOP' and
              not dis.stack_effect(ins.opcode,ins.arg) == stack_ptr - save_stack_ptr):
          print(dis.stack_effect(ins.opcode,ins.arg), stack_ptr - save_stack_ptr)
          assert(False)
       if did_jmp == False and block_idx+1 < len(blocks) and ins_idx+1==blocks[block_idx+1][0]:
         builder.branch(blocks[block_idx+1][2])
       ins_idx+=1
   i+=1

open("foo.ll", "w+").write(str(module))

