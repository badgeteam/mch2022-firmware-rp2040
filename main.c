#include "main.h"

int main(void)
{
  board_init();
  tusb_init();

  while (1)
  {
    tud_task(); // tinyusb device task
    cdc_task();
  }

  return 0;
}
