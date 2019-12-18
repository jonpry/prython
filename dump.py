#! /usr/bin/python3
import dis, marshal, struct, sys, time, types, warnings, bisect
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO

from llvmlite import ir
from llvmlite import binding as llvm

############## Stuff to print out pyc file
INDENT = " " * 3
MAX_HEX_LEN = 16
NAME_OFFSET = 20

def to_hexstr(bytes_value, level=0, wrap=False):
    indent = INDENT*level
    line = " ".join(("%02x",) * MAX_HEX_LEN)
    last = " ".join(("%02x",) * (len(bytes_value) % MAX_HEX_LEN))
    lines = (line,) * (len(bytes_value) // MAX_HEX_LEN)
    if last:
        lines += (last,)
    if wrap:
        template = indent + ("\n"+indent).join(lines)
    else:
        template = " ".join(lines)
    try:
        return template % tuple(bytes_value)
    except TypeError:
        return template % tuple(ord(char) for char in bytes_value)

def unpack_pyc(filename):
    f = open(filename, "rb")
    magic = f.read(4)
    unixtime = struct.unpack("I", f.read(4))[0]
    timestamp = time.asctime(time.localtime(unixtime))
    size = struct.unpack("I", f.read(4))[0]
    size = struct.unpack("I", f.read(4))[0]
    code = marshal.load(f)
    f.close()
    return filename, magic, unixtime, timestamp, code

def show_consts(consts, level=0):
    indent = INDENT*level
    i = 0
    for obj in consts:
        if isinstance(obj, types.CodeType):
            print(indent+"%s (code object)" % i)
            show_code(obj, level=level+1)
        else:
            print(indent+"%s %r" % (i, obj))
        i += 1

codes = []
def show_bytecode(code, level=0):
    indent = INDENT*level
    print(to_hexstr(code.co_code, level, wrap=True))
    print(indent+"disassembled:")
    buffer = StringIO()
    sys.stdout = buffer
    codes.append(code)
    dis.disassemble(code)
    sys.stdout = sys.__stdout__
    print(indent + buffer.getvalue().replace("\n", "\n"+indent))

def show_code(code, level=0):
    indent = INDENT*level

    for name in dir(code):
        if not name.startswith("co_"):
            continue
        if name in ("co_code", "co_consts"):
            continue
        value = getattr(code, name)
        if isinstance(value, str):
            value = repr(value)
        elif name == "co_flags":
            value = "0x%05x" % value
        elif name == "co_lnotab":
            value = "0x(%s)" % to_hexstr(value)
        print("%s%s%s" % (indent, (name+":").ljust(NAME_OFFSET), value))
    print("%sco_consts" % indent)
    show_consts(code.co_consts, level=level+1)
    print("%sco_code" % indent)
    show_bytecode(code, level=level+1)

def show_file(filename):
    filename, magic, unixtime, timestamp, code = unpack_pyc(filename)
    magic = "0x(%s)" % to_hexstr(magic)

    print("  ## inspecting pyc file ##")
    print("filename:     %s" % filename)
    print("magic number: %s" % magic)
    print("timestamp:    %s (%s)" % (unixtime, timestamp))
    print("code")
    show_code(code, level=1)
    print("  ## done inspecting pyc file ##")


USAGE = "  usage: %s <PYC FILENAME>" % sys.argv[0]

if len(sys.argv) == 1:
    sys.exit("Error: Too few arguments\n%s" % USAGE)
if len(sys.argv) > 2:
    warnings.warn("Ignoring extra arguments: %s" % (sys.argv[2:],))

if sys.argv[1] == "-h":
    print(USAGE)
else:
    show_file(sys.argv[1])

############## End Stuff to print out pyc file

############## Simple type decl stuff
module = ir.Module(name=__file__)
module.triple = "x86_64-pc-linux-gnu"
td = llvm.create_target_data("e-m:e-i64:64-f80:128-n8:16:32:64-S128")
int1 = ir.IntType(1)
int8 = ir.IntType(8)
int32 = ir.IntType(32)
int64 = ir.IntType(64)
char = ir.IntType(8)
pchar = char.as_pointer()
dbl = ir.DoubleType()

############## Strings of different length require different types for static initializers
str_types = {}
def make_str_type(l):
   global int64,char,str_types,pyobj_type
   if l in str_types:
      return str_types[l]
   t = ir.global_context.get_identified_type("PyStr" + str(l))
   t.set_body(pyobj_type, int64, ir.ArrayType(char,l))
   p = t.as_pointer()
   str_types[l] = (t,p)
   return t,p

############## Tuples of different length require different types for static initializers
tuple_types = {}
def make_tuple_type(l):
   global int64,char,tuple_types,pyobj_type,ppyobj_type
   if l in tuple_types:
      return tuple_types[l]
   t = ir.global_context.get_identified_type("PyTuple" + str(l))
   t.set_body(pyobj_type, int64, ir.ArrayType(ppyobj_type,l))
   p = t.as_pointer()
   tuple_types[l] = (t,p)
   return t,p


magic_methods = ["float","str",
                 "add","sub","mul","floordiv","truediv","mod","pow","lshift","rshift","and","xor","or",
                 "radd","rsub","rmul","rfloordiv","rtruediv","rmod","rpow","rlshift","rrshift","rand","rxor","ror",
                 "iadd","isub","imul","itruediv","ifloordiv","imod","ipow","ilshift","irshift","iand","ixor","ior",
                 "neg","pos","abs","invert","complex","int","long","oct","hex","complex",
                 "index","round","trunc","floor","ceil",
                 "enter","exit","iter","next",
                 "lt","le","eq","ne","ge","gt",
                 "new", "init", "del", "repr", "bytes", "format", "hash", "bool",
                 "getattr","getattribute","setattr","delattr","dir",
                 "get","set","delete","set_name","slots",
                 "init_subclass","prepare","class","instancecheck","subclasscheck","class_getitem","call",
                 "len","length_hint","getitem","setitem","delitem","missing","reverse","contains",
                 "await","aiter","anext","aenter","aexit",
                 ];

const_map = {}

############## Types for integral python types
vtable_type = ir.global_context.get_identified_type("struct.vtable_t")
pvtable_type = vtable_type.as_pointer()

pyobj_type = ir.global_context.get_identified_type("class.pyobj")
pyobj_type.set_body(pvtable_type)
ppyobj_type = pyobj_type.as_pointer()
pppyobj_type = ppyobj_type.as_pointer()

pyfunc_type = ir.global_context.get_identified_type("PyFunc")
ppyfunc_type = pyfunc_type.as_pointer()

fnty = ir.FunctionType(ppyobj_type, (pppyobj_type, int64, ppyobj_type))
vlist = [ir.IntType(64),ir.ArrayType(ppyfunc_type,len(magic_methods))]
vtable_type.set_body(*vlist)

pyint_type = ir.global_context.get_identified_type("PyInt")
pyint_type.set_body(pyobj_type, int64)
ppyint_type = pyint_type.as_pointer()

pyfloat_type = ir.global_context.get_identified_type("PyFloat")
pyfloat_type.set_body(pyobj_type, dbl)
ppyfloat_type = pyfloat_type.as_pointer()

pycode_type = ir.global_context.get_identified_type("PyCode")
pycode_type.set_body(pyobj_type, fnty.as_pointer())
ppycode_type = pycode_type.as_pointer()

pystr_type, ppystr_type = make_str_type(0)

pyfunc_type.set_body(pyobj_type, ppycode_type, ppystr_type, make_tuple_type(0)[1])

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

i=0

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
      g.initializer = pyint_type([[vtable_map['int']],ir.Constant(int64,con)])
   elif type(con) == bool:
      g = ir.GlobalVariable(module,pybool_type,"global_" + str(con).lower())
      const_map[tup] = g
      g.initializer = pybool_type([[vtable_map['bool']],ir.Constant(int64,int(con))])
   elif type(con) == float:
      g = ir.GlobalVariable(module,pyfloat_type,"global_" + str(const_idx))
      const_map[tup] = g
      g.initializer = pyfloat_type([[vtable_map['float']],ir.Constant(dbl,con)])
   elif isinstance(con,str):
      t,p = make_str_type(len(con)+1)
      g = ir.GlobalVariable(module,t,"global_" + str(const_idx) + name)
      const_map[tup] = g
      g.initializer = t([[vtable_map['str']],int64(len(con)+1),ir.Constant(ir.ArrayType(char,len(con)+1),bytearray(con + "\0",'utf8'))])
   elif isinstance(con, types.CodeType):
      g = ir.GlobalVariable(module,pycode_type,"global_" + str(const_idx))
      const_map[tup] = g
      g.initializer = pycode_type([[vtable_map['code']], func_map[con]])
   elif isinstance(con, tuple):
      t,p = make_tuple_type(len(con))
      g = ir.GlobalVariable(module,t,"global_" + str(const_idx))
      const_map[tup] = g
      cons = []
      for c in con:
        c = get_constant(c)
        cons.append(c.bitcast(ppyobj_type) if c else None)
      g.initializer = t([[vtable_map['tuple']], int64(len(con)), ir.ArrayType(ppyobj_type,len(con))(cons)])    
   elif isinstance(con, ir.Function):
      c = ir.GlobalVariable(module,pycode_type,"global_" + str(const_idx))      
      const_map[c] = c
      c.global_constant = True
      c.initializer = pycode_type([[vtable_map['code']],con])

      g = ir.GlobalVariable(module,pyfunc_type,"global_" + str(const_idx+1))      
      const_map[tup] = g
      g.initializer = pyfunc_type([[vtable_map['func']],c,get_constant(con.name).bitcast(ppystr_type),make_tuple_type(0)[1](None)])
   else:
      print(type(con))
      assert(False)   
   g.global_constant = True
   return g


#Add all the members to the various vtables
integrals = {"int" : { "mul" , "add", "xor", "or", "and", "radd", "mod", "floordiv", "float", "str", "gt", "lt", "le", "ge", "ne", "eq" }, 
             "float" : { "mul", "add", "sub", "pow", "radd", "rmul", "rsub", "rpow", "mod", "truediv", "rmod", "rtruediv",
                         "floordiv", "rfloordiv", "str"},
             "tuple" : { "str", "getitem" }, 
             "str" : { "add", "str", "getitem"}, 
             "code" : {}, 
             "func" : { "str"}, 
             "class" : {}, 
             "bool" : { "str" }, 
             "NotImplemented" : {},
             "exception" : {},
             "list" : { "str", }, 
             "dict" : {}}

vtable_map = {}

for t in integrals.keys():
   g = ir.GlobalVariable(module,vtable_type,"vtable_" + t)
   vtable_map[t] = g

for t in integrals.keys():
   g = vtable_map[t]
   vinit = [int64(i),[]]
   for m in magic_methods:
      if m in integrals[t]:
        vinit[1].append(get_constant(ir.Function(module, fnty, name=t + "_" + m)))
      else:
        vinit[1].append(ppyfunc_type(None))
   g.initializer = vtable_type(vinit)
   g.global_constant = True
   i+=1

noimp.initializer = pynoimp_type([[vtable_map['NotImplemented']]])

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

load_attr_type = ir.FunctionType(ppyobj_type, (ppyobj_type, ppyobj_type))
load_attr = ir.Function(module, load_attr_type, name="load_attr")

builtin_print = ir.Function(module, fnty, name="builtin_print")
builtin_buildclass = ir.Function(module, fnty, name="builtin_buildclass")

builtin_repr = ir.Function(module, fnty, name="builtin_repr")
builtin_repr.attributes.add("uwtable")

builtin_str = ir.Function(module, fnty, name="builtin_str")
builtin_str.attributes.add("uwtable")

begin_catch_type = ir.FunctionType(ir.VoidType(), [])
begin_catch = ir.Function(module, begin_catch_type, "__cxa_begin_catch")
end_catch = ir.Function(module, begin_catch_type, "__cxa_end_catch")
resume_type = ir.FunctionType(ir.VoidType(), [int8.as_pointer()])
resume = ir.Function(module, resume_type, "_Unwind_Resume")

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

truth_type = ir.FunctionType(int1, (ppyobj_type,))
truth = ir.Function(module, truth_type, name="truth")
truth.attributes.add("uwtable")

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
  alist = [builder.gep(get_constant(s + "\n","debug_" + s),(int32(0),int32(2),int32(0)))]
  builder.call(printf,alist)

def pyslot(builder, o, slot):
  vtable = builder.load(builder.gep(o,(int32(0),int32(0))))
  f = builder.gep(vtable,(int32(0),int32(1),int32(magic_methods.index(slot))))
  return builder.load(f)

def binary_op(builder,stack_ptr,op,reverse=True):
  v1 = builder.load(stack[stack_ptr-1])
  stack_ptr-=1
  v2 = builder.load(stack[stack_ptr-1])
  stack_ptr-=1

  v = stack[stack_ptr]
  args = (v2,v1,int32(magic_methods.index(op)),int32(magic_methods.index("r" + op) if reverse else -1))
  if len(except_stack):
     newblock,builder,rval = invoke(binop,builder,target,args)
     replace_block(ins,block_idx,newblock,builder)
     builder.store(rval,stack[stack_ptr])
  else:
     builder.store(builder.call(binop,args),v)
  stack_ptr+=1
  return stack_ptr

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
   #lbuilder.call(resume,[lbuilder.extract_value(lp, 0)])
   #lbuilder.resume(lp)
   pad_map[except_stack[-1]] = (lpad,lbuilder)
   return (lpad,lbuilder)

def invoke(func,builder,target,args):
   global block_num

   lpad = get_lpad(func)[0]
   newblock = func.append_basic_block(name="block" + str(block_num+1))
   blocks_by_ofs[ins.offset] = newblock
   block_num += 1
   rval = builder.invoke(target, args, newblock, lpad)
   builder = ir.IRBuilder(newblock)
   return newblock,builder,rval


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
   i+=1

get_constant(False)
get_constant(True)

#Apply get_constant to constant table contents
i=0
for c in codes:
   for con in c.co_consts:
      get_constant(con)

i=0

di_func_type = module.add_debug_info("DISubroutineType", {
        # None as `null`
        "types":           module.add_metadata([None]),
     })


def replace_block(ins,block_idx,newblock,builder):
   blocks_by_ofs[ins.offset] = newblock
   blocks[block_idx][2] = newblock
   blocks[block_idx][3] = builder                    


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
         builder.store(ir.Constant(ppyobj_type,None),l)
      local.append(l)

   name = []
   for s in range(len(c.co_names)):
      l = builder.alloca(ppyobj_type,1)
      if c.co_names[s] == "print":
         builder.store(builder.bitcast(get_constant(builtin_print_wrap),ppyobj_type),l)
      elif c.co_names[s] == "str":
         builder.store(builder.bitcast(get_constant(builtin_str),ppyobj_type),l)
      elif c.co_names[s] == "repr":
         builder.store(builder.bitcast(get_constant(builtin_repr),ppyobj_type),l)
         builder.store(ir.Constant(ppyobj_type,None),l)
      name.append(l)

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
      if ins.opname=='POP_JUMP_IF_FALSE' or ins.opname=='POP_JUMP_IF_TRUE' or ins.opname=='SETUP_EXCEPT' or ins.opname=='JUMP_FORWARD':
        nxt=True
      ins_idx +=1

   ins_idxs = [r[0] for r in blocks] 
   stack_ptr = 0
   ins_idx=0
   branch_stack = {}
   except_stack = []
   finally_stack = []
   unreachable = False

   for ins in dis.get_instructions(c):
       print(ins)
       a,block_idx,block,builder = blocks[bisect.bisect_right(ins_idxs,ins_idx)-1]

       if unreachable and not ins.is_jump_target:
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

       if ins.offset in except_stack: #This is a catch block
          assert(ins.offset == except_stack[-1]) #assumption not sure if true
          func.blocks.remove(block)
          newblock,builder = get_lpad(except_stack[-1])
          replace_block(ins,block_idx,newblock,builder)
          stack_ptr += 3

       #debug(builder,"ins " + str(ins.offset))

       save_stack_ptr = stack_ptr
       if ins.opname=='LOAD_CONST':
         v = stack[stack_ptr]
         tbl = ins.argval
         tup = (type(tbl),tbl)
         if const_map[tup] is None:
            builder.store(ir.Constant(ppyobj_type,None),v)
         else:
            builder.store(builder.bitcast(const_map[tup],ppyobj_type),v)
         stack_ptr+=1
       elif ins.opname=='LOAD_FAST' or ins.opname=='LOAD_NAME':
         v = stack[stack_ptr]
         tbl = builder.load((local if ins.opname=="LOAD_FAST" else name)[ins.arg])
         builder.store(tbl,v)
         stack_ptr+=1
       elif ins.opname=='LOAD_GLOBAL': #TODO
         v = stack[stack_ptr]
         builder.store(ir.Constant(ppyobj_type,None),v)
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
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(1))))
         for te in range(ins.arg):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(obj,(int32(0),int32(2),int32(te))))
            stack_ptr-=1
         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='BUILD_LIST':
         obj = builder.bitcast(builder.call(malloc,[int64(pylist_type.get_abi_size(td))]),ppylist_type)
         builder.store(vtable_map["list"],builder.gep(obj,(int32(0),int32(0),int32(0))))
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(1))))
         builder.store(int64(ins.arg),builder.gep(obj,(int32(0),int32(2))))
         data = builder.bitcast(builder.call(malloc,[int64(pppyobj_type.get_abi_size(td))]),pppyobj_type)
         builder.store(data,builder.gep(obj,(int32(0),int32(3))))

         for te in range(ins.arg):
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(data,(int32(te),)))
            stack_ptr-=1
         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='CALL_FUNCTION': #TODO
         savestack = builder.call(stacksave,[])
         args = builder.alloca(ppyobj_type,ins.arg)
         for i in range(ins.arg): 
            builder.store(builder.load(stack[stack_ptr-1]),builder.gep(args,(int32(i),)))
            stack_ptr-=1
         #debug(builder,"post store " + str(ins.offset))

         cfunc = builder.bitcast(builder.load(stack[stack_ptr-1]),ppyfunc_type)
         stack_ptr-=1
         code = builder.load(builder.gep(cfunc,(int32(0),int32(1))))
         target = builder.load(builder.gep(code,(int32(0),int32(1))))
         #debug(builder,"deref " + str(ins.offset))

         if not ins.arg:
            args = ppyobj_type(None)

         if len(except_stack):
            newblock,builder,rval = invoke(func,builder,target,(args,int64(ins.arg),ppyobj_type(None)))
            replace_block(ins,block_idx,newblock,builder)
            builder.store(rval,stack[stack_ptr])
         else:
            builder.store(builder.call(target,(args,int64(ins.arg),ppyobj_type(None))),stack[stack_ptr])

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
         else:
            assert(False)

         obj = builder.bitcast(builder.call(malloc,[int64(pyfunc_type.get_abi_size(td))]),ppyfunc_type)
         builder.store(vtable_map['func'],builder.gep(obj,(int32(0),int32(0),int32(0))))
         builder.store(builder.bitcast(code,ppycode_type),builder.gep(obj,(int32(0),int32(1))))
         builder.store(builder.bitcast(func_name,ppystr_type),builder.gep(obj,(int32(0),int32(2))))
         if args:
            builder.store(builder.bitcast(args,make_tuple_type(0)[1]),builder.gep(obj,(int32(0),int32(3))))
         else:
            builder.store(make_tuple_type(0)[1](None),builder.gep(obj,(int32(0),int32(3))))

         builder.store(builder.bitcast(obj,ppyobj_type),stack[stack_ptr])
         stack_ptr+=1
       elif ins.opname=='RETURN_VALUE':
         builder.ret(builder.load(stack[stack_ptr-1]))
         stack_ptr-=1
       elif ins.opname=='LOAD_ATTR': #TODO:
         v1 = builder.load(stack[stack_ptr-1])
         stack_ptr-=1
         v2 = builder.bitcast(get_constant(ins.argval),ppyobj_type)
         v = stack[stack_ptr]
         builder.store(builder.call(load_attr,(v1,v2)),v)
         stack_ptr+=1
       elif ins.opname=='LOAD_BUILD_CLASS': #TODO
         v = stack[stack_ptr]
         builder.store(builder.bitcast(get_constant(builtin_buildclass),ppyobj_type),v)
         stack_ptr+=1
       elif ins.opname=='POP_JUMP_IF_FALSE':
         v = builder.load(stack[stack_ptr-1])
         c = builder.call(truth,(v,))
         builder.cbranch(builder.not_(c),blocks_by_ofs[ins.arg],blocks[block_idx+1][2])
         stack_ptr-=1
         branch_stack[ins.arg] = stack_ptr
         did_jmp = True
       elif ins.opname=='JUMP_FORWARD':
         builder.branch(blocks_by_ofs[ins.argval])
         branch_stack[ins.argval] = stack_ptr
         did_jmp=True
         unreachable=True
       elif ins.opname=='LOAD_METHOD': #TODO
         v = stack[stack_ptr]
         builder.store(ir.Constant(ppyobj_type,None),v)
         stack_ptr+=1         
       elif ins.opname=='CALL_METHOD': #TODO
         stack_ptr-=2
         stack_ptr-=ins.arg                  

         v = stack[stack_ptr]
         builder.store(ir.Constant(ppyobj_type,None),v)
         stack_ptr+=1
       elif ins.opname=='BINARY_MULTIPLY':
         stack_ptr = binary_op(builder,stack_ptr,"mul")
       elif ins.opname=='BINARY_ADD':
         stack_ptr = binary_op(builder,stack_ptr,"add")
       elif ins.opname=='BINARY_SUBTRACT':
         stack_ptr = binary_op(builder,stack_ptr,"sub")
       elif ins.opname=='BINARY_OR':
         stack_ptr = binary_op(builder,stack_ptr,"or")
       elif ins.opname=='BINARY_AND':
         stack_ptr = binary_op(builder,stack_ptr,"and")
       elif ins.opname=='BINARY_XOR':
         stack_ptr = binary_op(builder,stack_ptr,"xor")
       elif ins.opname=='BINARY_LSHIFT':
         stack_ptr = binary_op(builder,stack_ptr,"lshift")
       elif ins.opname=='BINARY_RSHIFT':
         stack_ptr = binary_op(builder,stack_ptr,"rshift")
       elif ins.opname=='BINARY_POWER':
         stack_ptr = binary_op(builder,stack_ptr,"pow")
       elif ins.opname=='BINARY_MODULO':
         stack_ptr = binary_op(builder,stack_ptr,"mod")
       elif ins.opname=='BINARY_TRUE_DIVIDE':
         stack_ptr = binary_op(builder,stack_ptr,"truediv")
       elif ins.opname=='BINARY_FLOOR_DIVIDE':
         stack_ptr = binary_op(builder,stack_ptr,"floordiv")
       elif ins.opname=='BINARY_SUBSCR':
         stack_ptr = binary_op(builder,stack_ptr,"getitem",False)
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
           stack_ptr = binary_op(builder,stack_ptr,opmap[ins.argval],False)
       elif ins.opname=='SETUP_FINALLY':
           finally_stack.append(ins.argval)       
       elif ins.opname=='END_FINALLY':
           finally_stack = finally_stack[:-1]
       elif ins.opname=='SETUP_EXCEPT':
           except_stack.append(ins.argval)
           branch_stack[ins.argval] = stack_ptr
       elif ins.opname=='POP_BLOCK':
           pass
       elif ins.opname=='POP_EXCEPT':
           except_stack = except_stack[:-1]
           pass       
       else:
           assert(False)

       if (   ins.opname!='SETUP_FINALLY' and 
              ins.opname!='SETUP_EXCEPT' and
              ins.opname!='POP_EXCEPT' and
              ins.opname!='END_FINALLY' and
              ins.opname!='EXTENDED_ARG' and
              not dis.stack_effect(ins.opcode,ins.arg) == stack_ptr - save_stack_ptr):
          print(dis.stack_effect(ins.opcode,ins.arg), stack_ptr - save_stack_ptr)
          assert(False)
       if did_jmp == False and block_idx+1 < len(blocks) and ins_idx+1==blocks[block_idx+1][0]:
         builder.branch(blocks[block_idx+1][2])
       ins_idx+=1
   i+=1

open("foo.ll", "w+").write(str(module))

