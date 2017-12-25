#ifndef NIOS_H_
#define NIOS_H_

#include <stdint.h>
#include <signal.h>
#include <semaphore.h>
#include <pthread.h>
#include "hps.h"

class Nios
{
public:
  Nios();
  virtual ~Nios();
  static void mailbox_nios2hps_signal_handler(int signo, siginfo_t *info, void *unused);
  static void *mailbox_nios2hps_data_reader(void *args);
  static void mailbox_hps2nios_write(uint64_t mes);
  void ledRead();
  void ledWrite(uint8_t led);
  void ledReverse();

private:
  static struct sigaction mailbox_nios2hps_action;
  static pid_t pid;
  static sem_t mailbox_nios2hps_signal_semaphore;
  static pthread_t nios2hps_data_reader_thread;
  static int mailbox_nios2hps;
  static int mailbox_hps2nios;
  //static pthread_mutex_t mutex_read;
  //static pthread_mutex_t mutex_write;

};

#endif /* NIOS_H_ */
