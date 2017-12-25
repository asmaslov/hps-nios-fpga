#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <irqreturn.h>

#include "atlas.h"
#include "../../hps_0.h"

#define MAX_STR 100

void* h2p_lw_mailbox_0_hps2nios_addr;
void* h2p_lw_mailbox_0_nios2hps_addr;

void disable_A9_interrupts(void);
void set_A9_IRQ_stack(void);
void config_GIC(void);
void config_KEYs(void);
void enable_A9_interrupts(void);

static irqreturn_t mailbox_0_nios2hps_isr(int irq, void *dev_id, struct pt_regs *regs)
{
  int val;
  
  if(h2p_lw_mailbox_0_nios2hps_addr)
  {
    val = (uint8_t)*(uint32_t *)h2p_lw_mailbox_0_nios2hps_addr;
    printf("Got %d\n", val);
  }

  return IRQ_HANDLED;
}

int main(int argc, char **argv)
{
  int fd;
  void* virtual_base;
  char str[MAX_STR];
  int val;
  
  printf("Wubba lubba dub dub!\nEnter number to send (255 max)\nEnter 'q' to exit\n");

  if((fd = open("/dev/mem", (O_RDWR | O_SYNC))) == -1)
  {
    printf("ERROR: could not open \"/dev/mem\"...\n");
    return(1);
  }
  virtual_base = mmap(NULL, HW_REGS_SPAN, (PROT_READ | PROT_WRITE), MAP_SHARED, fd, HW_REGS_BASE);
  if(virtual_base == MAP_FAILED)
  {
    printf("ERROR: mmap() failed...\n");
    close(fd);
    return(1); 
  }

  h2p_lw_mailbox_0_hps2nios_addr = virtual_base +
                                   (((unsigned long)(ALT_LWFPGASLVS_OFST + MAILBOX_SIMPLE_0_HPS2NIOS_BASE)) & (unsigned long)HW_REGS_MASK);
  h2p_lw_mailbox_0_nios2hps_addr = virtual_base +
                                   (((unsigned long)(ALT_LWFPGASLVS_OFST + MAILBOX_SIMPLE_0_NIOS2HPS_BASE)) & (unsigned long)HW_REGS_MASK);

  /*if (request_irq(MAILBOX_SIMPLE_0_NIOS2HPS_IRQ, mailbox_0_nios2hps_isr, SA_INTERRUPT, "mailbox_0_nios2hps", NULL)
  {
    printf("ERROR: cannot register IRQ %d\n", MAILBOX_SIMPLE_0_NIOS2HPS_IRQ);
    return -EIO;
  }*/

  disable_A9_interrupts();
  set_A9_IRQ_stack();
  config_GIC();
  config_KEYs (); // configure KEYs to generate interrupts
  enable_A9_interrupts();

  while(1)
  {
    scanf("%s", str);
    if (*str == 'q')
    {
      printf("Bye!\n");
      return(0);
    }
    val = (int)strtol(str, NULL, 16);
    if(val > UCHAR_MAX)
    {
      printf("Too much!\n");
    }
    else
    {
      *(uint32_t *)h2p_lw_mailbox_0_hps2nios_addr = (uint8_t)val;
      printf("Send %d\n", val);
    }
  }
  
  return(0);
}


