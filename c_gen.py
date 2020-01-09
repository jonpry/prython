#!/usr/bin/python3

gperf = open("get_slot.gperf", "w")
slots = open("slots.h", "w")

from magic import magic_methods as om

sl = ["int", "float", "tuple", "str", "code", "func", "class", "bool", "noimp", "exception", "list", "dict", "object", "list_iter", "dict_view", "dict_iter"]


gperf.write("""%{
typedef struct SlotResult SlotResult;
%}
struct SlotResult
  {
  const char *name;
  int slot_num;
  };
%%""" + "\n")

for i in range(len(om)):
  gperf.write("__" + om[i] + "__, " + str(i) + "\n") 

slots.write("#ifndef SLOTS_H\n#define SLOTS_H\n\n")

for i in range(len(om)):
  slots.write("#define " + om[i].upper() + "_SLOT " + str(i) + "\n") 

slots.write("\n")

for i in range(len(sl)):
  slots.write("#define " + sl[i].upper() + "_RTTI " + str(1<<i) + "\n") 

slots.write("#endif //SLOTS_H\n")
exit(0)

for i in range(len(om)):
   om[i] = om[i] + "__\x00"

magic_methods = om[:]
magic_methods.sort()
#print(magic_methods)

def indent(s,level):
   for i in range(level):
      s = "    " + s
   print(s)

def print_switch(level,prefix,idx):
   last = ""
   indent("  switch(str[" + str(level) + "]){",level)
   for i in range(idx,len(magic_methods)):
      if len(magic_methods[i]) - 1 < level:
         continue
      if level > 0 and magic_methods[i][:level] != magic_methods[idx][:level]:
         break
      if magic_methods[i][:level+1] != last:
         last = magic_methods[i][:level+1]
         txt = last[level] if last[level]!="\x00" else "\\x00"
         indent("    case '" + txt + "': //" + str(level), level=level) #+ " - " + magic_methods[i],level)
         oldlevel = level
         if level < len(magic_methods[i])-1:
            print_switch(level+1,magic_methods[i][:level+1],i)
         else:
            indent("      slot=" + str(om.index(magic_methods[i])) + ";", level)
         assert(oldlevel == level)
         indent("      break;",level)

   indent("    default: break;",level)
   indent("  }",level)

print("int get_slot(const char * str){")
print("  int slot=-1;")
print_switch(0,"",0)
print("  return slot;")
print("}")
