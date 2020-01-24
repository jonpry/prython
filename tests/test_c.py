#!/usr/bin/python3

class Foo(object):  
   c = 2
   d = lambda x: x
   def __init__(self,a):
      setattr(self,'b',a)

print(Foo.c)
f = Foo(2)
setattr(f,'e',7)
print(f.e)
f.f = 2
f.b = 3
Foo.d = 2
f.d = 3
print(Foo.d)

a = {"a" : "b"}
print(a["a"])
a["n"] = 2
print(a["n"])
print(a)

print(len(a))
print(hash(a))
