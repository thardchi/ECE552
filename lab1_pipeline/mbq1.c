#include <stdio.h>

static volatile int MEM = 1; /* one volatile location to force a real lw */

int main(void){
 const int N = 1000000;
 volatile const int *p = &MEM;   /* forces a real load */
 int a=1,b=2,c=3,d=4,y=6,m=0,n=0;
 int i;

 for (i = 0; i < N; i++) {
//    / 2-cycle: ALU -> ALU, distance 1 /
   a = b ^ y;      // producer: XOR prevents algebraic folding /
   c = a + 2;      // consumer immediately /

//    / 1-cycle: ALU, one independent ALU, then dependent ALU /
   a = d + 3;      // producer /
   y = y + 1;      // independent filler /
   d = a + 4;      // consumer after one gap /

//    / 2-cycle: LOAD -> use, distance 1 /
   m = *p;         //* real lw /
   n = m + 3;      // immediate use */
}

printf("%d %d %d %d %d\n", a, b, c, d, n);
return 0;
}