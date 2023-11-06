#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>

// bounded buffer size
#define NUM_THREADS 10 // 生产者线程数 + 消费者线程数

// sleeping barber problem for customer and coordinator(barber)
sem_t mutex;
sem_t barber_semaphore;
sem_t customer_semaphore;
sem_t Tutor_semaphore;

typedef struct
{
  int id;
  int priority;
  unsigned int time;
  int tutor_id;
  int taken_help_num;
  bool have_seated;
} Student_Data;

typedef struct
{
  Student_Data *data;
  int capacity;
  int size;
  pthread_mutex_t mutex;    // 互斥锁，用于线程安全
  pthread_cond_t not_full;  // 条件变量，用于通知生产者
  pthread_cond_t not_empty; // 条件变量，用于通知消费者
} MaxHeap;

bool done = false;
Student_Data *StudentInfo = NULL;
MaxHeap *heap = NULL;
int taken_seats = 0;
int NUM_STUDENTS;
int NUM_TUTORS;
int HELPS;
int NUM_CHAIRS;
int TOTAL_THREAD;

int compareStudent_Datas(const Student_Data *a, const Student_Data *b)
{
  if (a->priority > b->priority)
  {
    return 1;
  }
  else if (a->priority < b->priority)
  {
    return -1;
  }
  else
  {
    if (a->time < b->time)
    {
      return 1;
    }
    else if (a->time > b->time)
    {
      return -1;
    }
    else
    {
      return 0;
    }
  }
}

MaxHeap *createMaxHeap(int capacity)
{
  if (heap == NULL)
  {
    printf("Memory allocation failed.\n");
    exit(1);
  }
  heap->data = (Student_Data *)malloc(sizeof(Student_Data) * capacity);
  if (heap->data == NULL)
  {
    printf("Memory allocation failed.\n");
    exit(1);
  }
  heap->capacity = capacity;
  heap->size = 0;
  pthread_mutex_init(&heap->mutex, NULL);    // 初始化互斥锁
  pthread_cond_init(&heap->not_full, NULL);  // 初始化条件变量（通知生产者）
  pthread_cond_init(&heap->not_empty, NULL); // 初始化条件变量（通知消费者）
  return heap;
}

void swap(Student_Data *a, Student_Data *b)
{
  Student_Data temp = *a;
  *a = *b;
  *b = temp;
}

void maxHeapify(MaxHeap *heap, int index)
{
  int left = 2 * index + 1;
  int right = 2 * index + 2;
  int largest = index;

  if (left < heap->size &&
      compareStudent_Datas(&heap->data[left], &heap->data[largest]) > 0)
  {
    largest = left;
  }

  if (right < heap->size &&
      compareStudent_Datas(&heap->data[right], &heap->data[largest]) > 0)
  {
    largest = right;
  }

  if (largest != index)
  {
    swap(&heap->data[index], &heap->data[largest]);
    maxHeapify(heap, largest);
  }
}

void insert(MaxHeap *heap, Student_Data value)
{
  pthread_mutex_lock(&heap->mutex); // 加锁
  while (heap->size == heap->capacity)
  {
    // 堆已满，等待消费者的通知
    pthread_cond_wait(&heap->not_full, &heap->mutex);
  }

  heap->data[heap->size] = value;
  int current = heap->size;
  heap->size++;

  while (current > 0 && compareStudent_Datas(&heap->data[current],
                                             &heap->data[(current - 1) / 2]) > 0)
  {
    swap(&heap->data[current], &heap->data[(current - 1) / 2]);
    current = (current - 1) / 2;
  }

  pthread_cond_signal(&heap->not_empty); // 通知等待的消费者
  pthread_mutex_unlock(&heap->mutex);    // 解锁
}

Student_Data extractMax(MaxHeap *heap)
{
  pthread_mutex_lock(&heap->mutex); // 加锁

  while (heap->size == 0 && !done)
  {
    // 堆为空时，等待生产者的通知
    pthread_cond_wait(&heap->not_empty, &heap->mutex);
  }

  if (heap->size == 0 && done)
  {
    pthread_mutex_unlock(&heap->mutex);
    Student_Data emptyStudent_Data = {0, 0};
    return emptyStudent_Data;
  }

  Student_Data max = heap->data[0];
  heap->data[0] = heap->data[heap->size - 1];
  heap->size--;
  pthread_cond_signal(&heap->not_full); // 通知等待的生产者
  maxHeapify(heap, 0);

  pthread_mutex_unlock(&heap->mutex); // 解锁

  return max;
}

void destroyMaxHeap(MaxHeap *heap)
{
  free(heap->data);
  free(heap);
  pthread_mutex_destroy(&heap->mutex);    // 销毁互斥锁
  pthread_cond_destroy(&heap->not_full);  // 销毁条件变量
  pthread_cond_destroy(&heap->not_empty); // 销毁条件变量
}

void *producerFunction(void *student_id)
{
  int id = *(int *)student_id;
  StudentInfo[id].id = id;
  StudentInfo[id].tutor_id = -1;
  StudentInfo[id].taken_help_num = 0;
  StudentInfo[id].have_seated = false;

  while (1)
  {
    sleep(1); // go back to programming for 1 second;
    // try to ask for help from tutor, check if there are availabe seats in the queue

    sem_wait(&mutex);
    if (taken_seats < NUM_CHAIRS)
    {
      taken_seats++;
      int priority = -1 * StudentInfo[id].taken_help_num;
      StudentInfo[id].priority = priority;
      unsigned int time = clock();
      StudentInfo[id].time = time;
      StudentInfo[id].have_seated = true;
      // St: Student x takes a seat. Empty chairs = <# of empty chairs>.
      printf("St: Student %d takes a seat. Empty chairs = %d\n", id, NUM_CHAIRS - taken_seats);
      sem_post(&mutex);
      sem_post(&barber_semaphore);
      sem_wait(&customer_semaphore);
    }
    else
    {
      // S: Student x found no empty chair. Will try again later
      printf("S: Student %d found no empty chair. Will try again later.\n", id);
      sem_post(&mutex);
      continue;
    }

    // wait to be tutored.
    while (StudentInfo[id].tutor_id == -1)
    {
    };

    // S: Student x received help from Tutor y.
    printf("S: Student %d received help from Tutor %d.\n", id, StudentInfo[id].tutor_id);

    // reset tutor id
    StudentInfo[id].tutor_id = -1;

    // reset have been seated
    StudentInfo[id].have_seated = false;
  }
}

void *coordinatorFunction(void *args)
{
  while (1)
  {
    // wait student's notification
    sem_wait(&barber_semaphore);
    sem_wait(&mutex);
    if (taken_seats > 0)
    {
      // put student in the queue, it could acutally be done by student itself, but due to requirements we need coordinator to do it
      int i;
      for (i = 0; i < NUM_STUDENTS; i++)
      {
        if (StudentInfo[i].have_seated)
        {
          insert(heap, StudentInfo[i]);
        }
      }
      taken_seats--;
      printf("The barber is cutting hair for customer %d\n", StudentInfo[i].id);
      sem_post(&mutex);
      printf("The barber has finished cutting hair for customer %d\n", StudentInfo[i].id);
      sem_post(&customer_semaphore);
    }
    else
    {
      sem_post(&mutex);
    }
  }
}

void *consumerFunction(void *args)
{
  // while (1)
  // {
  //   sleep(1);
  //   Student_Data top_element = extractMax(heap);
  //   printf("Consumer: Student_Data with highest priority: (priority: %d, time: %u)\n",
  //          top_element.priority, top_element.time);
  // }
}

int main()
{
  // read in data
  NUM_STUDENTS = 10;
  NUM_TUTORS = 3;
  NUM_CHAIRS = 4;
  HELPS = 5;
  TOTAL_THREAD = NUM_STUDENTS + NUM_TUTORS;

  // init prority queue
  MaxHeap *heap = (MaxHeap *)malloc(sizeof(MaxHeap));
  heap = createMaxHeap(heap);
  // init StudentInfo which is stored in heap
  StudentInfo = (Student_Data *)malloc(sizeof(Student_Data) * TOTAL_THREAD);

  // init threads for students, tutors, coordinator
  pthread_t threads[NUM_TUTORS + NUM_STUDENTS];
  pthread_t coordinator;

  // create mutex and semaphore

  sem_init(&mutex, 0, 1);
  sem_init(&barber_semaphore, 0, 0);
  sem_init(&customer_semaphore, 0, 0);
  sem_init(&Tutor_semaphore, 0, 0);

  // create all threads
  for (int i = 0; i < NUM_THREADS; i++)
  {
    if (i < NUM_STUDENTS)
    {
      if (pthread_create(&threads[i], NULL, &producerFunction, (void *)&i) != 0)
      {
        perror("Failed to create thread");
      }
    }
    else
    {
      if (pthread_create(&threads[i], NULL, &consumerFunction, (void *)&i) != 0)
      {
        perror("Failed to create thread");
      }
    }
  }

  if (pthread_create(&coordinator, NULL, &coordinatorFunction, (void *)0) != 0)
  {
    perror("Failed to create thread");
  }

  // wait till all thread finish
  for (int i = 0; i < NUM_THREADS; i++)
  {
    pthread_join(threads[i], NULL);
  }
  pthread_join(coordinator, NULL);

  // delete dynamic memory and semaphore
  destroyMaxHeap(heap);

  return 0;
}
