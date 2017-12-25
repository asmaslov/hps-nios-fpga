#include "nios.h"
#include "protocol.h"
#include "../nios_mailbox/nios_mailbox.h"
#include <iostream>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/ioctl.h>

using namespace std;

struct sigaction Nios::mailbox_nios2hps_action;
pid_t Nios::pid;
sem_t Nios::mailbox_nios2hps_signal_semaphore;
pthread_t Nios::nios2hps_data_reader_thread;
int Nios::mailbox_nios2hps;
int Nios::mailbox_hps2nios;

void Nios::mailbox_nios2hps_signal_handler(int signo, siginfo_t *info, void *unused)
{
  if(info->si_signo == NIOS_MAILBOX_REALTIME_SIGNO)
  {
    sem_post(&mailbox_nios2hps_signal_semaphore);
  }
}

void *Nios::mailbox_nios2hps_data_reader(void *args)
{
  uint64_t mes;

  while(1)
  {
    while(sem_wait(&mailbox_nios2hps_signal_semaphore));
    //pthread_mutex_lock(&mutex_read);
    if(lseek(mailbox_nios2hps, 0, SEEK_SET) != 0)
    {
      cerr << "Failed to seek mailbox_nios2hps to proper location" << endl;
      //pthread_mutex_unlock(&mutex_read);
      continue;
    }
    read(mailbox_nios2hps, &mes, sizeof(mes));
    printf("[HARDWARE] Reading: 0x%08x 0x%08x\n", (uint32_t)mes, (uint32_t)(mes >> 32));
    switch ((uint32_t)mes) {
      case LED_NUMBER:
	printf("Active led %lu\n", (uint32_t)(mes >> 32));
	break;
      case SWITCH_COUNT:
	printf("Led switched %lu times\n", (uint32_t)(mes >> 32));
	break;
      default:
	break;
    }
    //pthread_mutex_unlock(&mutex_read);
  }
  return NULL;
}

void Nios::mailbox_hps2nios_write(uint64_t mes)
{
  //pthread_mutex_lock(&mutex_write);
  if(lseek(mailbox_hps2nios, 0, SEEK_SET) != 0)
  {
    cerr << "Failed to seek mailbox_hps2nios to proper location" << endl;
  }
  else
  {
    printf("[HARDWARE] Writing: 0x%08x 0x%08x\n", (uint32_t)mes, (uint32_t)(mes >> 32));
    write(mailbox_hps2nios, &mes, sizeof(mes));
  }
  //pthread_mutex_unlock(&mutex_write);
}

Nios::Nios ()
{
  struct sigaction backup_action;

  pid = getpid();

  mailbox_nios2hps = open("/dev/nios_mailbox_0", O_RDONLY);
  if(mailbox_nios2hps < 0)
  {
    cerr << "Could not open \"/dev/nios_mailbox_0\"..." << endl;
    exit(1);
  }
  memset(&mailbox_nios2hps_action, 0, sizeof(struct sigaction));
  mailbox_nios2hps_action.sa_sigaction = mailbox_nios2hps_signal_handler;
  mailbox_nios2hps_action.sa_flags = SA_SIGINFO | SA_NODEFER;
  sigaction(NIOS_MAILBOX_REALTIME_SIGNO, &mailbox_nios2hps_action, &backup_action);
  if(ioctl(mailbox_nios2hps, IOCTL_SET_PID, &pid))
  {
    cerr << "Failed IOCTL_SET_PID" << endl;
    close(mailbox_nios2hps);
    sigaction(NIOS_MAILBOX_REALTIME_SIGNO, &backup_action, NULL);
    exit(1);
  }

  mailbox_hps2nios = open("/dev/nios_mailbox_1", (O_WRONLY | O_SYNC));
  if(mailbox_hps2nios < 0)
  {
    cerr << "Could not open \"/dev/nios_mailbox_1\"..." << endl;
    close(mailbox_nios2hps);
    sigaction(NIOS_MAILBOX_REALTIME_SIGNO, &backup_action, NULL);
    exit(1);
  }

  pthread_create(&nios2hps_data_reader_thread, NULL, mailbox_nios2hps_data_reader, NULL);
}

Nios::~Nios ()
{
  pthread_cancel(nios2hps_data_reader_thread);
  //pthread_mutex_destroy(&mutex_write);
  //pthread_mutex_destroy(&mutex_read);
  sem_destroy(&mailbox_nios2hps_signal_semaphore);
  close(mailbox_nios2hps);
  close(mailbox_hps2nios);
}

void Nios::ledRead()
{
  uint64_t mes;

  mes = READ;
  mailbox_hps2nios_write(mes);
}

void Nios::ledWrite(uint8_t led)
{
  uint64_t mes;

  mes = led;
  mes <<= 32;
  mes += WRITE;
  mailbox_hps2nios_write(mes);
}

void Nios::ledReverse()
{
  uint64_t mes;

  mes = REVERSE;
  mailbox_hps2nios_write(mes);
}
