#!/usr/bin/python3

class Foo(object):
  pass

def bar(self):
  return 3

a = 4 #Foo()
a.__float__ = 4
print(a.__len__)
print(len(a))
