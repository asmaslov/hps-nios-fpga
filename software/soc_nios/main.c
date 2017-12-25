#include "alt_types.h"
#include <stdbool.h>
#include "system.h"
#include "io.h"
#include "altera_avalon_pio_regs.h"
#include "altera_avalon_timer_regs.h"
#include "altera_avalon_mailbox_simple.h"
#include "altera_avalon_mailbox_simple_regs.h"
#include "sys/alt_irq.h"
#include "sys/alt_stdio.h"
#include "../protocol.h"

#define NOPRINTF

//NOTE: Variables and constants for led control
alt_8 led;
alt_8 step;
alt_u32 count;
#define LED_MAX  7

//NOTE: Variables and constants for key and switch handling
alt_u8 keys;
alt_u8 history;
#define KEY_REVERSE   (1 << 0)
#define KEY_INFO      (1 << 1)
#define SW_ENABLE     (1 << 2)
#define PORT_IN_MASK  0x3

//NOTE: Variables and pointers for 2 mailboxes
altera_avalon_mailbox_dev* mailbox_hps2nios;
altera_avalon_mailbox_dev* mailbox_nios2hps;
alt_u32 buffer[2];
volatile bool newMail;

//NOTE: Timer 0 interrupt handler
void TIMER_0_ISR(void* context)
{
  IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0);
  IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, ALTERA_AVALON_TIMER_CONTROL_CONT_MSK);
  led += step;
  if(led > LED_MAX)
  {
    led = 0;
  }
  if(led < 0)
  {
    led = LED_MAX;
  }
  IOWR_ALTERA_AVALON_PIO_DATA(PORT_OUT_0_BASE, (1 << led));
  count++;
  IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, ALTERA_AVALON_TIMER_CONTROL_CONT_MSK | ALTERA_AVALON_TIMER_CONTROL_ITO_MSK);
}

//NOTE: Handler of the receiving mailbox
void MAILBOX_HPS2NIOS_ISR(void* context)
{
  IOWR_ALTERA_AVALON_MAILBOX_INTR(MAILBOX_SIMPLE_HPS2NIOS_BASE, 0);
  //NOTE: Order is important! CMD register should be read after PTR register
  buffer[1] = IORD_ALTERA_AVALON_MAILBOX_PTR(MAILBOX_SIMPLE_HPS2NIOS_BASE);
  buffer[0] = IORD_ALTERA_AVALON_MAILBOX_CMD(MAILBOX_SIMPLE_HPS2NIOS_BASE);
#ifndef NOPRINTF
  alt_printf("Reading: 0x%x 0x%x\n\r", buffer[0], buffer[1]);
#endif
  newMail = true;
  IOWR_ALTERA_AVALON_MAILBOX_INTR(MAILBOX_SIMPLE_HPS2NIOS_BASE, ALTERA_AVALON_MAILBOX_SIMPLE_INTR_PEN_MSK);
}

//NOTE: Function for the sending mailbox
void send(ProtocolReply reply, alt_u32 data)
{
  buffer[0] = reply;
  buffer[1] = data;
  //NOTE: Order is important! CMD register should be written after PTR register
  IOWR_ALTERA_AVALON_MAILBOX_PTR(MAILBOX_SIMPLE_NIOS2HPS_BASE, buffer[1]);
  IOWR_ALTERA_AVALON_MAILBOX_CMD(MAILBOX_SIMPLE_NIOS2HPS_BASE, buffer[0]);
#ifndef NOPRINTF
  alt_printf("[M] Writing: 0x%x 0x%x\n\r", buffer[0], buffer[1]);
#endif
}

int main ()
{
  newMail = false;
  led = 0;
  step = 1;
  count = 0;
  keys = 0;
  history = 0;

  //NOTE: Activate initial led
  IOWR_ALTERA_AVALON_PIO_DATA(PORT_OUT_0_BASE, 1);
#ifndef NOPRINTF
  alt_printf("\n\rTurn the switch ON to activate the timer\n\r");
#endif
  //NOTE: Setup the mailboxes
  mailbox_hps2nios = altera_avalon_mailbox_open(MAILBOX_SIMPLE_HPS2NIOS_NAME, NULL, MAILBOX_HPS2NIOS_ISR);
  alt_ic_isr_register(MAILBOX_SIMPLE_HPS2NIOS_IRQ_INTERRUPT_CONTROLLER_ID, MAILBOX_SIMPLE_HPS2NIOS_IRQ, MAILBOX_HPS2NIOS_ISR, NULL, NULL);
  mailbox_nios2hps = altera_avalon_mailbox_open(MAILBOX_SIMPLE_NIOS2HPS_NAME, NULL, NULL);
  //NOTE: Setup the timer
  IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, ALTERA_AVALON_TIMER_CONTROL_STOP_MSK);
  while((IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE) & ALTERA_AVALON_TIMER_STATUS_RUN_MSK) != 0);
  IOWR_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE, 0);
  IOWR_ALTERA_AVALON_TIMER_PERIODH(TIMER_0_BASE, (alt_u16)(ALT_CPU_FREQ >> 16));
  IOWR_ALTERA_AVALON_TIMER_PERIODL(TIMER_0_BASE, (alt_u16)ALT_CPU_FREQ);
  alt_ic_isr_register(TIMER_0_IRQ_INTERRUPT_CONTROLLER_ID, TIMER_0_IRQ, TIMER_0_ISR, NULL, NULL);

  while(1)
  {
    //NOTE: Read and handle the buttons
    keys = IORD_ALTERA_AVALON_PIO_DATA(PORT_IN_0_BASE) ^ PORT_IN_MASK;
    if((keys & SW_ENABLE) != 0)
    {
      if((IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE) & ALTERA_AVALON_TIMER_STATUS_RUN_MSK) == 0)
      {
        IOWR_ALTERA_AVALON_TIMER_SNAPL(TIMER_0_BASE, 0);
        IOWR_ALTERA_AVALON_TIMER_SNAPH(TIMER_0_BASE, 0);
        IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE,
                                         ALTERA_AVALON_TIMER_CONTROL_CONT_MSK |
                                         ALTERA_AVALON_TIMER_CONTROL_START_MSK |
                                         ALTERA_AVALON_TIMER_CONTROL_ITO_MSK);
        while((IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE) & ALTERA_AVALON_TIMER_STATUS_RUN_MSK) == 0);
      #ifndef NOPRINTF
        alt_printf("Timer activated\n\r");
      #endif
      }
    }
    else
    {
      if((IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE) & ALTERA_AVALON_TIMER_STATUS_RUN_MSK) != 0)
      {
        IOWR_ALTERA_AVALON_TIMER_CONTROL(TIMER_0_BASE, ALTERA_AVALON_TIMER_CONTROL_STOP_MSK);
        while((IORD_ALTERA_AVALON_TIMER_STATUS(TIMER_0_BASE) & ALTERA_AVALON_TIMER_STATUS_RUN_MSK) != 0);
      #ifndef NOPRINTF
        alt_printf("Timer stopped\n\r");
      #endif
      }
    }
    if(((keys & KEY_REVERSE) != 0) && ((history & KEY_REVERSE) == 0))
    {
    #ifndef NOPRINTF
      alt_printf("Direction changed\n\r");
    #endif
      step *= -1;
    }
    if(((keys & KEY_INFO) != 0) && ((history & KEY_INFO) == 0))
    {
    #ifndef NOPRINTF
      alt_printf("Counter value sent to mailbox\n\r");
    #endif
      send(SWITCH_COUNT, count);
    }
    history = keys;
    //NOTE: Parse the data got from mailbox
    if(newMail)
    {
      newMail = false;
      switch (buffer[0]) {
        case READ:
	#ifndef NOPRINTF
          alt_printf("Request for active led\n\r");
	#endif
          send(LED_NUMBER, led);
          break;
        case WRITE:
          led = buffer[1];
          IOWR_ALTERA_AVALON_PIO_DATA(PORT_OUT_0_BASE, led);
          IOWR_ALTERA_AVALON_TIMER_SNAPL(TIMER_0_BASE, 0);
          IOWR_ALTERA_AVALON_TIMER_SNAPH(TIMER_0_BASE, 0);
	#ifndef NOPRINTF
          alt_printf("Led changed\n\r");
	#endif
          break;
        case REVERSE:
          step *= -1;
	#ifndef NOPRINTF
          alt_printf("Direction changed\n\r");
	#endif
          break;
        default:
          break;
      }
    }
  }

  return 0;
}
