#include "main.h"
#include "tm1637.h"
#include "keyboard.h"

volatile uint32_t tickCount;
/* Compatibility globals for the original demonstration drivers. */
uint32_t last_display_update, lastScanTime;
uint16_t counter;
char lastKey;
static const uint8_t segments[10] =
  {0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
static uint8_t screen[4];
static void show_value(int value, int hundredths) {
  unsigned n = value < 0 ? (unsigned)-value : (unsigned)value;
  for (int i = 3; i >= 0; --i) {
    screen[i] = (n || i == 3 || (hundredths && i >= 1)) ? segments[n % 10] : 0;
    n /= 10;
  }
  if (value < 0) screen[2] = 0x40; /* subtraction range: -9..9 */
  if (hundredths) screen[1] |= 0x80; /* TM1637 colon separates hundredths */
}
static void show_error(void) {
  screen[0]=0; screen[1]=0x79; screen[2]=0x50; screen[3]=0x50;
}
static void app_key(char key, uint32_t now);
static void app_poll(uint32_t now);
static void app_reset(void);
#ifndef HOST_TEST
void osSystickHandler(void) { ++tickCount; }
static void display_flush(void) {
  tm1637_start(); tm1637_write_byte(0x40); tm1637_stop();
  tm1637_start(); tm1637_write_byte(0xc0);
  for (int i=0; i<4; ++i) tm1637_write_byte(screen[i]);
  tm1637_stop();
  tm1637_start(); tm1637_write_byte(0x8f); tm1637_stop();
}
static void led_set(int on) {
  GPIOA->BSRR = on ? (1U<<5) : (1U<<21);
}
static void init_board(void) {
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;
  GPIOA->MODER = (GPIOA->MODER & ~(3U<<10)) | (1U<<10);
  GPIOA->OTYPER &= ~(1U<<5);
  led_set(0);
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  GPIOA->MODER = (GPIOA->MODER & ~(15U<<4)) | (10U<<4);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(255U<<8)) | (1U<<8) | (1U<<12);
  USART2->BRR = 417; /* original Wokwi setup: 48 MHz, 115200 baud */
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
  initKeyboard();
  tm1637_init();
  SysTick->LOAD = 47999;
  SysTick->VAL = 0;
  SysTick->CTRL = 7;
}
int _write(int file, uint8_t *ptr, int len) {
  (void)file;
  for (int i=0; i<len; ++i) {
    while (!(USART2->ISR & USART_ISR_TXE)) {}
    USART2->TDR=ptr[i];
  }
  return len;
}
#else
static int test_led;
static void led_set(int on) { test_led=on; }
#endif

static unsigned operand_a, operand_b, operation;
static int have_a, have_b, choosing, result_ready;
static const char operation_names[4]={'+','-','*','/'};
static void app_reset(void) {
  operand_a=operand_b=operation=0;
  have_a=have_b=choosing=result_ready=0;
  led_set(0); show_value(0,0);
  printf("CALC: a; * selects +,-,*,/; b; # result\n");
}
static void app_key(char key, uint32_t now) {
  (void)now;
  if (key>='0' && key<='9') {
    if (result_ready) app_reset();
    if (!have_a || !choosing) {
      operand_a=(unsigned)(key-'0'); have_a=1;
      printf("A: %u\n",operand_a); show_value((int)operand_a,0);
    } else {
      operand_b=(unsigned)(key-'0'); have_b=1;
      printf("B: %u\n",operand_b); show_value((int)operand_b,0);
    }
  } else if (key=='*' && have_a && !result_ready) {
    if (choosing) operation=(operation+1U)%4U;
    else choosing=1; /* first * selects addition */
    printf("OP: %c\n",operation_names[operation]);
    /* Operation number is unambiguous on seven-segment display. */
    screen[0]=segments[operand_a]; screen[1]=0x40;
    screen[2]=segments[operation+1]; screen[3]=have_b?segments[operand_b]:0x40;
  } else if (key=='#' && !result_ready) {
    if (!have_a || !have_b || !choosing) {
      printf("INCOMPLETE: enter a, operation and b\n"); return;
    }
    result_ready=1;
    if (operation==3 && operand_b==0) {
      show_error(); printf("ERROR: division by zero\n"); return;
    }
    int result;
    switch(operation) {
      case 0: result=(int)operand_a+(int)operand_b; break;
      case 1: result=(int)operand_a-(int)operand_b; break;
      case 2: result=(int)operand_a*(int)operand_b; break;
      default: result=(int)((operand_a*100U+operand_b/2U)/operand_b); break;
    }
    show_value(result,operation==3);
    if (operation==3)
      printf("RESULT: %u / %u = %d.%02d (display HH:hh)\n",operand_a,operand_b,result/100,result%100);
    else
      printf("RESULT: %u %c %u = %d\n",operand_a,operation_names[operation],operand_b,result);
  }
}
static void app_poll(uint32_t now) { (void)now; }

#ifndef HOST_TEST
int main(void) {
  init_board(); app_reset(); display_flush();
  uint32_t last_scan=0, changed_at=0;
  char candidate=0, stable=0;
  uint8_t previous[4]; memcpy(previous,screen,4);
  while (1) {
    uint32_t now=tickCount;
    if ((uint32_t)(now-last_scan)>=5U) {
      last_scan=now;
      char raw=readKey();
      if (raw!=candidate) { candidate=raw; changed_at=now; }
      if (candidate!=stable && (uint32_t)(now-changed_at)>=30U) {
        stable=candidate;
        if (stable) app_key(stable,now);
      }
    }
    app_poll(now);
    if (memcmp(previous,screen,4)!=0) {
      display_flush(); memcpy(previous,screen,4);
    }
  }
}
#endif
