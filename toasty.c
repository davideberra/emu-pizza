#include <stdio.h>
#include <time.h>

struct vaffanculo
{
    char b0:1;
    char b1:1;
    char b2:1;
    char b3:1;
    char b4:1;
    char b5:1;
    char b6:1;
    char b7:1;
};

struct vaffanculo v;
char v2;

int num = 323232;

int fun()
{
    return (num++);
}
 
int main()
{
    time_t ts, tc;
    long i;
    int crc = 0;
    char *v3;    
 
    time(&ts);

    bzero(&v, 1);
    v3 = &v;

    num = 31;

    for (i=0; i<8000000000; i++)
    {
        num ^= (i + 1);
 
        v.b0 = ((num % 2) != 0);
        v.b1 = ((num % 5) != 0);
        v.b2 = ((num % 7) != 0);
        v.b3 = ((num % 9) != 0);
    }

    time(&tc);
    printf("CIOMMESSO: %d SECONDS. CRC: %d\n", (tc - ts), *v3);

    time(&ts);

    bzero(&v, 1);
    num = 31;

    for (i=0; i<8000000000; i++)
    {
        *v3 &= 0xF0;

        num ^= (i + 1);

        if ((num % 2) != 0)
            *v3 |= 0x01;
        if ((num % 5) != 0)
            *v3 |= 0x02;
        if ((num % 7) != 0)
            *v3 |= 0x04;
        if ((num % 9) != 0)
            *v3 |= 0x08;
    }

    time(&tc);
    printf("CIOMMESSO: %d SECONDS. CRC: %d\n", (tc - ts), *v3);

}

