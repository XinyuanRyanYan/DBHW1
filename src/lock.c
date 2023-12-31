#if !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "lock.h"

#define NUM_COUNTERS 16

//init the lock
// called by the thread running the benchmark before any child threads are spawned.
void rw_lock_init(ReaderWriterLock *rwlock) {
  for(int i = 0;i < NUM_COUNTERS; i++){
    rwlock->reader_counters[i] = 0;
  }
  rwlock->writer = 0;
}

bool check_if_reader_exists(ReaderWriterLock *rwlock) {
  for(int i = 0;i < NUM_COUNTERS; i++){
    if(rwlock->reader_counters[i]>0){
      return true;
    }
  }
  return false;
}

/**
 * Try to acquire a lock and spin until the lock is available.
 * Readers should add themselves to the read counter,
 * then check if a writer is waiting
 * If a writer is waiting, decrement the counter and wait for the writer to finish
 * then retry
 */
void read_lock(ReaderWriterLock *rwlock, uint8_t thread_id) {
  int group_id = thread_id % NUM_COUNTERS;
  //acq read lock
  while (true){
    //atomic_add_fetch returns current value, but not needed
    __atomic_add_fetch(&rwlock->reader_counters[group_id], 1, __ATOMIC_SEQ_CST);


    if (rwlock->writer){
      //cancel
      __atomic_add_fetch(&rwlock->reader_counters[group_id], -1, __ATOMIC_SEQ_CST);
      //wait
      while (rwlock->writer);
    } else {
      return;
    }
  }
}

//release an acquired read lock for thread `thread_id`
void read_unlock(ReaderWriterLock *rwlock, uint8_t thread_id) {
  int group_id = thread_id % NUM_COUNTERS;
  __atomic_add_fetch(&rwlock->reader_counters[group_id], -1, __ATOMIC_SEQ_CST);
  return;
}

/**
 * Try to acquire a write lock and spin until the lock is available.
 * Spin on the writer mutex.
 * Once it is acquired, wait for the number of readers to drop to 0.
 */
void write_lock(ReaderWriterLock *rwlock) {
  // acquire write lock.
  while (__sync_lock_test_and_set(&rwlock->writer, 1))
    while (rwlock->writer != 0)
      ;
  //once acquired, wait on readers
  while (check_if_reader_exists(rwlock));
  return;
}

//Release an acquired write lock.
void write_unlock(ReaderWriterLock *rwlock) {
  __sync_lock_release(&rwlock->writer);
  return;
}

