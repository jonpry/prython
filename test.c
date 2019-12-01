#include <stdio.h>

int foo;
char *bar = (char*)&foo;
//const char *bar = foo;

struct {
  int a;
  char b[];
} foos;

int main(){
  if(bar){
    printf("foo\n");
  }
  return 0;
}
