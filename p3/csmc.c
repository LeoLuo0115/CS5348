#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_SIZE 12
#define NUM_THREADS 10 // 生产者线程数 + 消费者线程数

typedef struct {
  int priority;
  unsigned int time;
} Pair;

typedef struct {
  Pair *data;
  int capacity;
  int size;
  pthread_mutex_t mutex;    // 互斥锁，用于线程安全
  pthread_cond_t not_full;  // 条件变量，用于通知生产者
  pthread_cond_t not_empty; // 条件变量，用于通知消费者
} MaxHeap;

bool done = false;
int producers_finished = 0;

int comparePairs(const Pair *a, const Pair *b) {
  if (a->priority > b->priority) {
    return 1;
  } else if (a->priority < b->priority) {
    return -1;
  } else {
    if (a->time < b->time) {
      return 1;
    } else if (a->time > b->time) {
      return -1;
    } else {
      return 0;
    }
  }
}

MaxHeap *createMaxHeap(int capacity) {
  MaxHeap *heap = (MaxHeap *)malloc(sizeof(MaxHeap));
  if (heap == NULL) {
    printf("Memory allocation failed.\n");
    exit(1);
  }
  heap->data = (Pair *)malloc(sizeof(Pair) * capacity);
  if (heap->data == NULL) {
    printf("Memory allocation failed.\n");
    exit(1);
  }
  heap->capacity = capacity;
  heap->size = 0;
  pthread_mutex_init(&heap->mutex, NULL); // 初始化互斥锁
  pthread_cond_init(&heap->not_full, NULL); // 初始化条件变量（通知生产者）
  pthread_cond_init(&heap->not_empty, NULL); // 初始化条件变量（通知消费者）
  return heap;
}

void swap(Pair *a, Pair *b) {
  Pair temp = *a;
  *a = *b;
  *b = temp;
}

void maxHeapify(MaxHeap *heap, int index) {
  int left = 2 * index + 1;
  int right = 2 * index + 2;
  int largest = index;

  if (left < heap->size &&
      comparePairs(&heap->data[left], &heap->data[largest]) > 0) {
    largest = left;
  }

  if (right < heap->size &&
      comparePairs(&heap->data[right], &heap->data[largest]) > 0) {
    largest = right;
  }

  if (largest != index) {
    swap(&heap->data[index], &heap->data[largest]);
    maxHeapify(heap, largest);
  }
}

void insert(MaxHeap *heap, Pair value) {
  pthread_mutex_lock(&heap->mutex); // 加锁
  while (heap->size == heap->capacity) {
    // 堆已满，等待消费者的通知
    pthread_cond_wait(&heap->not_full, &heap->mutex);
  }

  heap->data[heap->size] = value;
  int current = heap->size;
  heap->size++;

  while (current > 0 && comparePairs(&heap->data[current],
                                     &heap->data[(current - 1) / 2]) > 0) {
    swap(&heap->data[current], &heap->data[(current - 1) / 2]);
    current = (current - 1) / 2;
  }

  pthread_cond_signal(&heap->not_empty); // 通知等待的消费者
  pthread_mutex_unlock(&heap->mutex);    // 解锁
}

Pair extractMax(MaxHeap *heap) {
  pthread_mutex_lock(&heap->mutex); // 加锁

  while (heap->size == 0 && !done) {
    // 堆为空时，等待生产者的通知
    pthread_cond_wait(&heap->not_empty, &heap->mutex);
  }

  if (heap->size == 0 && done) {
    pthread_mutex_unlock(&heap->mutex);
    Pair emptyPair = {0, 0};
    return emptyPair;
  }

  Pair max = heap->data[0];
  heap->data[0] = heap->data[heap->size - 1];
  heap->size--;
  pthread_cond_signal(&heap->not_full); // 通知等待的生产者
  maxHeapify(heap, 0);

  pthread_mutex_unlock(&heap->mutex); // 解锁

  return max;
}

void destroyMaxHeap(MaxHeap *heap) {
  free(heap->data);
  free(heap);
  pthread_mutex_destroy(&heap->mutex);    // 销毁互斥锁
  pthread_cond_destroy(&heap->not_full);  // 销毁条件变量
  pthread_cond_destroy(&heap->not_empty); // 销毁条件变量
}

void *producerFunction(void *args) {
  while (1) {
    MaxHeap *heap = (MaxHeap *)args;

    Pair pair;
    pair.priority = rand() % 100;

    // 使用全局时间计数器作为time参数
    pthread_mutex_lock(&heap->mutex);
    pair.time = clock();
    pthread_mutex_unlock(&heap->mutex);
    insert(heap, pair);
  }
  
  //

  return NULL;
}

void *consumerFunction(void *args) {
  MaxHeap *heap = (MaxHeap *)args;

  while (1) {
    sleep(1);
    Pair top_element = extractMax(heap);
    printf("Consumer: Pair with highest priority: (priority: %d, time: %u)\n",
           top_element.priority, top_element.time);
  }

  return NULL;
}

int main() {
  MaxHeap *heap = createMaxHeap(MAX_SIZE);
  pthread_t threads[NUM_THREADS];

  for (int i = 0; i < NUM_THREADS; i++) {
    if (i > 3) {
      if (pthread_create(&threads[i], NULL, &producerFunction, (void *)heap) != 0) {
        perror("Failed to create thread");
      }
    } else {
      if (pthread_create(&threads[i], NULL, &consumerFunction, (void *)heap) != 0) {
        perror("Failed to create thread");
      }
    }
  }

  for (int i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  destroyMaxHeap(heap);

  return 0;
}
