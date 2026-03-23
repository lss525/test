#include <pthread.h>

typedef struct {
  int amount;
  pthread_mutex_t mutex;
}lock_t;

void amountInit(lock_t* account);
void Income(lock_t* account, int amount);
void Expend(lock_t* account, int amount);