#!/usr/bin/python3

import sys
print(sys.argv[0])

class foo:
  def bar(self,a,b,c):
    def bam(a,b,c=c):
      pass
    def bam2(a,b,c=c if a else b):
      print(3)
      print(a)
    return (bam,bam2)

def bar():
  return (1,2)

a = foo()
a.bar(0,0,0)[0](1,2)
