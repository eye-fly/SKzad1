#ifndef __CBUFFER_H__
#define __CBUFFER_H__

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct CircularBuffer {
  char *buf;
  size_t pos;
  size_t capacity;
  size_t size;
  pthread_mutex_t mutex;
  sem_t whait_for_buffer;
  bool is_whaiting;
} CircularBuffer;

void cbInit(CircularBuffer *b);
void cbDestroy(CircularBuffer *b);
void cbPushBack(CircularBuffer *b, char const *data, size_t n);
void cbDropFront(CircularBuffer *b, size_t n);
size_t cbGetContinuousCount(CircularBuffer *b);
char *cbGetData(CircularBuffer const *b);


static const size_t CB_INITIAL_SIZE = 16384;

void cbInit(CircularBuffer* b)
{
  b->buf = malloc(CB_INITIAL_SIZE);
  if (!b->buf)
    fatal("Out of memory.");
  b->pos = 0;
  b->capacity = CB_INITIAL_SIZE;
  b->size = 0;
  pthread_mutex_init(&b->mutex, NULL);
  sem_init(&b->whait_for_buffer, 0, 0);
}

void cbDestroy(CircularBuffer* b)
{
  if (b->buf) {
    free(b->buf);
    b->buf = NULL;
  }
  b->size = 0;
  b->capacity = 0;
  b->pos = 0;
  pthread_mutex_destroy(&b->mutex);
  sem_destroy(&b->whait_for_buffer);
}

void cbPushBack(CircularBuffer* b, char const* data, size_t n)
{
  pthread_mutex_lock(&b->mutex);
  if (n + b->size > b->capacity) {
    size_t oldCapacity = b->capacity;
    size_t cm = 2;
    while (n + b->size > cm * b->capacity)
      cm *= 2;
    void* newBuf = reallocarray(b->buf, cm, b->capacity);
    if (!newBuf)
      fatal("Out of memory.");
    b->buf = newBuf;
    b->capacity = cm * oldCapacity;

    if (b->pos + b->size > oldCapacity)
      memmove(b->buf + oldCapacity, b->buf, b->pos + b->size - oldCapacity);
    if (b->pos) {
      memmove(b->buf, b->buf + b->pos, b->size);
      b->pos = 0;
    }
  }

  char* dataEnd = b->buf + ((b->pos + b->size) % b->capacity);
  size_t spaceAtEnd = b->capacity - (dataEnd - b->buf);
  memcpy(dataEnd, data, n > spaceAtEnd ? spaceAtEnd : n);
  if (n > spaceAtEnd)
    memcpy(b->buf, data + spaceAtEnd, n - spaceAtEnd);
  b->size += n;

  if (b->is_whaiting) {
    sem_post(&b->whait_for_buffer);
  }
  else {
    pthread_mutex_unlock(&b->mutex);
  }
}

void cbDropFront(CircularBuffer* b, size_t n)
{
  pthread_mutex_lock(&b->mutex);
  assert(n <= b->size);

  b->pos = (b->pos + n) % b->capacity;
  b->size -= n;

  pthread_mutex_unlock(&b->mutex);
}

size_t cbGetContinuousCount(CircularBuffer * b)
{

  size_t possible = b->capacity - b->pos;
  size_t ret = possible > b->size ? b->size : possible;
  if (ret == 0) {
    pthread_mutex_lock(&b->mutex);
    b->is_whaiting = true;
    pthread_mutex_unlock(&b->mutex);
    sem_wait(&b->whait_for_buffer);
    b->is_whaiting = false;
    pthread_mutex_unlock(&b->mutex);

    possible = b->capacity - b->pos;
    ret = possible > b->size ? b->size : possible;
  }
  return ret;
}

char* cbGetData(CircularBuffer const* b)
{
  return b->buf + b->pos;
}

#endif
