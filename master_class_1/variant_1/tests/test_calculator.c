#define HOST_TEST
#include "../main.c"
#include <assert.h>
int main(void) {
 for(unsigned op=0;op<4;op++) for(unsigned a=0;a<10;a++) for(unsigned b=0;b<10;b++) {
  app_reset();app_key((char)('0'+a),0);
  for(unsigned k=0;k<=op;k++)app_key('*',0);
  app_key((char)('0'+b),0);app_key('#',0);
  assert(result_ready);
  if(op==3 && b==0) {assert(screen[1]==0x79&&screen[2]==0x50);continue;}
  int n=op==0?(int)(a+b):op==1?(int)a-(int)b:op==2?(int)(a*b):(int)((a*100+b/2)/b);
  uint8_t actual[4];memcpy(actual,screen,4);show_value(n,op==3);assert(!memcmp(actual,screen,4));
 }
 app_reset();app_key('#',0);assert(!result_ready);
 app_key('9',0);app_key('*',0);app_key('9',0);app_key('#',0);app_key('2',0);assert(have_a&&!have_b&&operand_a==2&&!result_ready);
 puts("PASS: 400 operand/operation combinations, incomplete entry, restart");
}
