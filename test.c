
int foo;
char *bar = (char*)&foo;
//const char *bar = foo;

struct {
  int a;
  char b[];
} foos;

int main(){
  return 0;
}
