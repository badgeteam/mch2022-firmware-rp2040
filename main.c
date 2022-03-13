#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "hardware/irq.h"
#include "hardware.h"

#include "bsp/board.h"
#include "tusb.h"

//------------- prototypes -------------//
static void cdc_task(void);

/*------------- MAIN -------------*/
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

static void cdc_task(void)
{
  uint8_t itf;

  for (itf = 0; itf < CFG_TUD_CDC; itf++)
  {
    {
      if ( tud_cdc_n_available(itf) )
      {
        uint8_t buf[64];

        uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));
      }
    }
  }
}
