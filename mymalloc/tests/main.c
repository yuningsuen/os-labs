#include <stdio.h>
#include <mymalloc.h>

int main() {
    printf("mymalloc tests\n");
    void *p1 = mymalloc(4096);
    void *p2 = mymalloc(4096*2);
    printf("p1=%p, p2=%p\n", p1, p2);
    myfree(p1);
    myfree(p2);
}
