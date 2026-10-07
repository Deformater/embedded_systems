#define HOST_TEST
#include "../main.c"
#include <assert.h>
int main(void) {
 app_reset();app_key('3',100);app_key('#',100);assert(running&&!test_led);
 app_poll(1099);assert(remaining==3);app_poll(1100);assert(remaining==2);
 app_poll(3100);assert(remaining==0&&finished&&test_led);
 app_key('*',3200);assert(!running&&!finished&&!test_led);
 app_key('#',3200);assert(finished&&test_led);
 app_reset();for(int i=0;i<5;i++)app_key('9',0);assert(input_seconds==9999);
 app_reset();app_key('2',0);app_key('#',0xfffffff0U);app_poll(1984);assert(finished&&test_led);
 app_reset();app_key('5',0);app_key('#',0);app_key('9',100);assert(remaining==5);
 app_key('*',100);app_poll(8000);assert(!finished&&!test_led);
 puts("PASS: second boundary, delayed poll, zero, limit, wraparound, reset");
}
