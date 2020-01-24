#!/usr/bin/python3

a = [1,2,3]

for i in a:
  print(i)
  if i<2:
     continue
  if i>=2:
     break

while True:
  print("hello")
  break

print("done")

b = [ x*x for x in a]
print(b)


f = "a"
cd = {"a": 1, "b": 2}
for k,v in cd.items():
   print(str(k) + "," + str(v))

b = a[:]

