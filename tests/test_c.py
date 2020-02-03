#!/usr/bin/python3

def bar():
   pass

class Foo(object):  
   c = 2
   d = lambda x: x
   def __init__(self,a):
      #print(__class__)
      bar()
      setattr(self,'b',a)

class Foo2(Foo,object):
   e = 3
   def __init__(self,a):
      print(__class__)
      super().__init__(a)


h = Foo2(1)

print(Foo2.c)
Foo.c = 3
f = Foo(2)
setattr(f,'e',7)
print(f.e)
f.f = 2
f.b = 3
Foo.d = 2
f.d = 3
print(Foo.d)
exit(0)
a = {"a" : "b"}
print(a["a"])
a["n"] = 2
print(a["n"])
print(a)

print(len(a))
print(hash(a))
