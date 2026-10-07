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

static unsigned input_seconds, remaining;
static int running, finished;
static uint32_t second_anchor;
static void app_reset(void) {
  input_seconds=remaining=0; running=finished=0;
  led_set(0); show_value(0,0);
  printf("TIMER: enter seconds 0..9999; # start; * reset\n");
}
static void app_key(char key, uint32_t now) {
  if (key=='*') { app_reset(); return; }
  if (running) return;
  if (key>='0' && key<='9') {
    if (finished) app_reset();
    unsigned next=input_seconds*10U+(unsigned)(key-'0');
    if (next>9999U) { printf("INPUT LIMIT: 9999\n"); return; }
    input_seconds=next; show_value((int)input_seconds,0);
    printf("SET: %u s\n",input_seconds);
  } else if (key=='#' && !finished) {
    remaining=input_seconds; second_anchor=now;
    running=remaining!=0; finished=!running;
    led_set(finished); show_value((int)remaining,0);
    printf("START: %u s\n", remaining);
    if (finished) printf("DONE: LED ON\n");
  }
}
static void app_poll(uint32_t now) {
  if (!running) return;
  uint32_t elapsed=(uint32_t)(now-second_anchor)/1000U;
  if (!elapsed) return;
  if (elapsed>=remaining) {
    remaining=0; running=0; finished=1; led_set(1);
    printf("DONE: LED ON\n");
  } else {
    remaining-=elapsed; second_anchor+=elapsed*1000U;
    printf("LEFT: %u s\n",remaining);
  }
  show_value((int)remaining,0);
}

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
