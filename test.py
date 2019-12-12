#!/usr/bin/python3

x = 2
y = 3
z = 1
a = x * x
a = a + 1
a = 1 << 2
x = 4.0
a = x * x
a = a + 5.0
a = a - 2.5
a = a ** 2
a = a % 31
a = a / 2
print(a)
if(a):
  print("true")

def bar(a):
  return a * 2

print(bar(a))
print(str(a) + " true")

a = (2,3)
print(a)
print(True)
print(False)
print(bar)
print(a[0])

a = [0,1]

def bam(a,b,*kwargs):
   return a * 2

print("hello")
print(a)

a = [[1,2],(0,1)]
print(a)

print(1 > 2)
print(a is None)

def aaa(a,b=None,c=1):
   return a + b + c

print(aaa(1.1,2.2,3.3))
