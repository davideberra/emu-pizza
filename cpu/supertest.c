#include <stdio.h>

int main()
{
    int *p;
    int v;
    long i;

    int a,b,c,r;

    c = 0;

    for(i=0;i<1000000000;i++)
    {
        a = i % 3;
        b = i % 20;
        r = a + b;

//        c = (((a & 0x80) >> 1) |
//            ((b & 0x80) >> 2) |
//            ((r & 0x80) >> 3));
        c = a ^ b ^ r;

        v = (c & 0x80);
    }

}
