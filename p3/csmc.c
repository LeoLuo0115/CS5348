#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define MAX_CUSTOMERS 5
#define MAX_TUTORS 2
#define NUM_CHAIRS 3
#define NUM_HELPS 2

// Define student structure
typedef struct
{
  int id;
  int help;
  int tutor_id;
  sem_t Student;
} Student;

// Define the semaphores for the barber, the customers, and the mutex
sem_t barber_semaphore;
sem_t customer_semaphore;
sem_t mutex;

// Define the semaphores for tutors
sem_t tutor_semaphore;

// Deinfe a mutex for priority queue
pthread_mutex_t pq_mutex;

// Define a list to keep track of the waiting customers
int *waiting_customers = NULL;
int num_waiting = 0;
int next_customer = 0;

// Define a Student array
Student *student_array = NULL;

// Define a Priority Queue to keep track priority (MLFQ with adjacent list)
int *head = NULL;
int *e = NULL;
int *ne = NULL;
int idx = 0;

// Define the total number of tutoring sessions completed so far by all the tutors
int num_session = 0;

// Define number of students receiving help now
int helping_now = 0;

// Define total number of requests (notifications sent) by students for tutoring so far
int total_request = 0;

// Define the tutor list which store student id for each tutor
int *tutoring_now = NULL;

// Define the init function for priority queue
void initialize()
{
  head = (int *)malloc(NUM_HELPS * sizeof(int));
  e = (int *)malloc(NUM_HELPS * sizeof(int));
  ne = (int *)malloc(NUM_HELPS * sizeof(int));

  if (head == NULL || e == NULL || ne == NULL)
  {
    fprintf(stderr, "Memory allocation failed\n");
    exit(1);
  }
  memset(head, -1, NUM_HELPS * sizeof(int));
}

// Define the add function, a -> b, add at end of linklist
void add(int a, int b)
{
  e[idx] = b;
  ne[idx] = -1;
  if (head[a] == -1)
  {
    head[a] = idx;
  }
  else
  {
    int cur = head[a];
    while (ne[cur] != -1)
    {
      cur = ne[cur];
    }
    ne[cur] = idx;
  }
  idx++;
}

// Define the delete at head function
void Delete(int a)
{
  if (head[a] != -1)
  {
    head[a] = ne[head[a]];
  }
}

int findTheFirstElement(int i)
{
  if (i < 0 || i >= NUM_HELPS)
  {
    fprintf(stderr, "Access Memory out of range of adjacent list\n");
    exit(1);
  }

  int cur = head[i];
  if (cur != -1)
  {
    return e[cur];
  }
  else
  {
    return -1;
  }
}

// Define the print function to print all node along with vertex i
void printPath(int i)
{
  if (i > NUM_HELPS)
  {
    fprintf(stderr, "Access Memory out of range of adjacent list\n");
    exit(1);
  }
  printf("%d -> ", i);

  int cur = head[i];
  while (cur != -1)
  {
    printf("%d -> ", e[cur]);
    cur = ne[cur];
  }

  printf("NULL\n");
}

// Define the free function for priority queue
void cleanup()
{
  free(head);
  free(e);
  free(ne);
}

// Define the barber thread function
void *
barber(void *arg)
{
  while (1)
  {
    printf("The barber is sleeping...\n");
    sem_wait(&barber_semaphore);
    sem_wait(&mutex);
    if (num_waiting > 0)
    {
      int customer_id = waiting_customers[next_customer];
      printf("next_customer index is %d, customer is %d\n", next_customer, customer_id);
      next_customer = (next_customer + 1) % NUM_CHAIRS;
      num_waiting--;
      total_request++;
      printf("The barber is cutting hair for customer %d\n", customer_id);
      // this is where barber will put customer id inside the priority queue
      pthread_mutex_lock(&pq_mutex);
      add(student_array[customer_id].help, customer_id);
      pthread_mutex_unlock(&pq_mutex);
      printPath(0);
      // notify tutor
      sem_post(&tutor_semaphore);
      sem_post(&mutex);
      sleep(rand() % 5 + 1);
      printf("The barber has finished cutting hair for customer %d\n", customer_id);
      // do not want barber wake up student, we want tutor wake up studnet with highest priority
      // sem_post(&customer_semaphore);
    }
    else
    {
      sem_post(&mutex);
    }
  }
  return NULL;
}

// Define the customer thread function
void *
customer(void *arg)
{
  int customer_id = *(int *)arg;
  student_array[customer_id].id = customer_id;
  student_array[customer_id].tutor_id = -1;
  student_array[customer_id].help = 0;
  // while (1) {
  sleep(rand() % 5 + 1);
  sem_wait(&mutex);
  if (num_waiting < NUM_CHAIRS)
  {
    waiting_customers[(next_customer + num_waiting) % NUM_CHAIRS] = customer_id;
    printf("waiting_customers index is %d, id is %d\n", (next_customer + num_waiting) % NUM_CHAIRS, customer_id);
    num_waiting++;
    printf("Customer %d is waiting in the waiting room\n", customer_id);
    sem_post(&mutex);
    sem_post(&barber_semaphore);
    // sem_wait(&customer_semaphore);
    sem_wait(&student_array[customer_id].Student);
    printf("Customer %d has finished getting a haircut\n", customer_id);
  }
  else
  {
    printf("Customer %d is leaving because the waiting room is full\n", customer_id);
    sem_post(&mutex);
  }
  // }

  return NULL;
}

void *
tutor(void *arg)
{
  int tutor_id = *(int *)arg;
  // printf("tutor id is %d\n", tutor_id);

  while (1)
  {
    printf("The tutor is sleeping...\n");
    sem_wait(&tutor_semaphore);

    pthread_mutex_lock(&pq_mutex);
    for (int i = 0; i < NUM_HELPS; i++)
    {
      if (findTheFirstElement(i) != -1)
      {
        int studnet_id = findTheFirstElement(i);
        tutoring_now[tutor_id] = studnet_id;
        num_session++;
        helping_now++;
        printf("Student %d tutored by Tutor %d, Students tutored now = %d, Total sessions tutored = %d\n", studnet_id, tutor_id, helping_now, num_session);
        // we need remove top priority student when when we select it from the priority queue
        Delete(i);
        break;
      }
    }
    pthread_mutex_unlock(&pq_mutex);

    // tutor
    sleep(rand() % 2 + 1);
    helping_now--;

    sem_post(&student_array[tutoring_now[tutor_id]].Student);
  }
}

int main()
{
  srand(time(NULL));

  // Initialize waiting_customers array
  waiting_customers = (int *)malloc(NUM_CHAIRS * sizeof(int));

  // Initialize the tutoring_now array
  tutoring_now = (int *)malloc(MAX_TUTORS * sizeof(int));

  // Initialize student_array
  student_array = (Student *)malloc(MAX_CUSTOMERS * sizeof(Student));

  // Initialize semaphores for each student in the array
  for (int i = 0; i < MAX_CUSTOMERS; ++i)
  {
    sem_init(&student_array[i].Student, 0, 0);
  }

  // Initialize Priority Queue
  initialize();

  // Initialize the semaphores and mutex
  sem_init(&barber_semaphore, 0, 0);
  sem_init(&customer_semaphore, 0, 0);
  sem_init(&mutex, 0, 1);
  sem_init(&tutor_semaphore, 0, 0);
  pthread_mutex_init(&pq_mutex, NULL);

  // Create a thread for the barber
  pthread_t barber_thread;
  pthread_create(&barber_thread, NULL, barber, NULL);

  // Create a thread for each customer
  pthread_t customer_threads[MAX_CUSTOMERS];
  int customer_ids[MAX_CUSTOMERS];
  for (int i = 0; i < MAX_CUSTOMERS; i++)
  {
    customer_ids[i] = i;
    pthread_create(&customer_threads[i], NULL, customer, &customer_ids[i]);
  }

  // Create a thread for each tutor
  pthread_t tutor_threads[MAX_TUTORS];
  int tutor_ids[MAX_TUTORS];
  for (int i = 0; i < MAX_TUTORS; i++)
  {
    tutor_ids[i] = i;
    pthread_create(&tutor_threads[i], NULL, tutor, &tutor_ids[i]);
  }

  // Join the barber and customer threads
  pthread_join(barber_thread, NULL);
  for (int i = 0; i < MAX_CUSTOMERS; i++)
  {
    pthread_join(customer_threads[i], NULL);
  }

  // Join the tutor threads
  for (int i = 0; i < MAX_CUSTOMERS; i++)
  {
    pthread_join(tutor_threads[i], NULL);
  }

  // Destroy the semaphores
  sem_destroy(&barber_semaphore);
  sem_destroy(&customer_semaphore);
  sem_destroy(&mutex);
  pthread_mutex_destroy(&pq_mutex);

  return 0;
}
