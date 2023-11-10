#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

#define MAX_studentS 5
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

// Define the semaphores for the coordinator, the students, and the mutex
sem_t coordinator_semaphore;
sem_t student_semaphore;
sem_t mutex;

// Define the semaphores for tutors
sem_t tutor_semaphore;

// Define a mutex for priority queue
pthread_mutex_t pq_mutex;

// Define a mutex for finsih flag
pthread_mutex_t finish_mutex;

// Define the total number of studnets who have taken required number of help from tutors
int finished_student = 0;

// Define a list to keep track of the waiting students
int *waiting_students = NULL;
int num_waiting = 0;
int next_student = 0;

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
  if (a < 0 || a >= NUM_HELPS)
  {
    fprintf(stderr, "Access Memory out of range of adjacent list\n");
    exit(1);
  }

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

// Define the coordinator thread function
void *
coordinator(void *arg)
{
  while (1)
  {
    // printf("The coordinator is sleeping...\n");
    sem_wait(&coordinator_semaphore);
    sem_wait(&mutex);
    if (num_waiting > 0)
    {
      int student_id = waiting_students[next_student];
      // printf("next_student index is %d, student is %d\n", next_student, student_id);
      next_student = (next_student + 1) % NUM_CHAIRS;
      total_request++;
      // printf("The coordinator is cutting hair for student %d\n", student_id);
      // this is where coordinator will put student id inside the priority queue
      pthread_mutex_lock(&pq_mutex);
      printf("C: Student %d with priority %d added to the queue. Waiting students now = %d, Total requests = %d.\n",
             student_id, student_array[student_id].help, num_waiting, total_request);
      add(student_array[student_id].help, student_id);
      pthread_mutex_unlock(&pq_mutex);
      // printf("C: Student %d with priority %d added to the queue. Waiting students now = %d, Total requests = %d.\n",
      //        student_id, student_array[student_id].help, num_waiting, total_request);
      for (int i = 0; i < NUM_HELPS; i++)
      {
        printPath(i);
      }
      num_waiting--;
      // notify tutor
      sem_post(&tutor_semaphore);
      sem_post(&mutex);
      sleep(rand() % 5 + 1);
      // printf("The coordinator has finished cutting hair for student %d\n", student_id);
      //  do not want coordinator wake up student, we want tutor wake up student with highest priority
      //  sem_post(&student_semaphore);
    }
    else
    {
      sem_post(&mutex);
    }
  }
  return NULL;
}

// Define the student thread function
void *
student(void *arg)
{
  int student_id = *(int *)arg;
  student_array[student_id].id = student_id;
  student_array[student_id].tutor_id = -1;
  student_array[student_id].help = 0;
  while (1)
  {
    sleep(rand() % 5 + 1);
    if (student_array[student_id].help >= NUM_HELPS)
    {
      pthread_mutex_lock(&finish_mutex);
      finished_student++;
      pthread_mutex_unlock(&finish_mutex);
      pthread_exit(NULL);
    }
    sem_wait(&mutex);
    if (num_waiting < NUM_CHAIRS)
    {
      waiting_students[(next_student + num_waiting) % NUM_CHAIRS] = student_id;
      // printf("waiting_students index is %d, id is %d\n", (next_student + num_waiting) % NUM_CHAIRS, student_id);
      num_waiting++;
      printf("S: student %d is waiting in the waiting room\n", student_id);
      sem_post(&mutex);
      sem_post(&coordinator_semaphore);
      // sem_wait(&student_semaphore);
      sem_wait(&student_array[student_id].Student);
      printf("S: Student %d received help from Tutor %d.\n", student_id, student_array[student_id].tutor_id);
    }
    else
    {
      printf("S: Student %d found no empty chair. Will try again later.\n", student_id);
      sem_post(&mutex);
    }
  }

  return NULL;
}

void *
tutor(void *arg)
{
  int tutor_id = *(int *)arg;
  // printf("tutor id is %d\n", tutor_id);

  while (1)
  {
    // printf("The tutor is sleeping...\n");
    sem_wait(&tutor_semaphore);

    pthread_mutex_lock(&pq_mutex);
    for (int i = 0; i < NUM_HELPS; i++)
    {
      if (findTheFirstElement(i) != -1)
      {
        int student_id = findTheFirstElement(i);
        tutoring_now[tutor_id] = student_id;
        num_session++;
        helping_now++;
        student_array[student_id].tutor_id = tutor_id;
        student_array[student_id].help++;
        printf("T: Student %d tutored by Tutor %d, Students tutored now = %d, Total sessions tutored = %d.\n", student_id, tutor_id, helping_now, num_session);
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

  // Initialize waiting_students array
  waiting_students = (int *)malloc(NUM_CHAIRS * sizeof(int));

  // Initialize the tutoring_now array
  tutoring_now = (int *)malloc(MAX_TUTORS * sizeof(int));

  // Initialize student_array
  student_array = (Student *)malloc(MAX_studentS * sizeof(Student));

  // Initialize semaphores for each student in the array
  for (int i = 0; i < MAX_studentS; ++i)
  {
    sem_init(&student_array[i].Student, 0, 0);
  }

  // Initialize Priority Queue
  initialize();

  // Initialize the semaphores and mutex
  sem_init(&coordinator_semaphore, 0, 0);
  sem_init(&student_semaphore, 0, 0);
  sem_init(&mutex, 0, 1);
  sem_init(&tutor_semaphore, 0, 0);
  pthread_mutex_init(&pq_mutex, NULL);
  pthread_mutex_init(&finish_mutex, NULL);

  // Create a thread for the coordinator
  pthread_t coordinator_thread;
  pthread_create(&coordinator_thread, NULL, coordinator, NULL);

  // Create a thread for each student
  pthread_t student_threads[MAX_studentS];
  int student_ids[MAX_studentS];
  for (int i = 0; i < MAX_studentS; i++)
  {
    student_ids[i] = i;
    pthread_create(&student_threads[i], NULL, student, &student_ids[i]);
  }

  // Create a thread for each tutor
  pthread_t tutor_threads[MAX_TUTORS];
  int tutor_ids[MAX_TUTORS];
  for (int i = 0; i < MAX_TUTORS; i++)
  {
    tutor_ids[i] = i;
    pthread_create(&tutor_threads[i], NULL, tutor, &tutor_ids[i]);
  }

  // Join the coordinator and student threads
  pthread_join(coordinator_thread, NULL);
  for (int i = 0; i < MAX_studentS; i++)
  {
    pthread_join(student_threads[i], NULL);
  }

  // Join the tutor threads
  for (int i = 0; i < MAX_studentS; i++)
  {
    pthread_join(tutor_threads[i], NULL);
  }

  // Destroy the semaphores
  sem_destroy(&coordinator_semaphore);
  sem_destroy(&student_semaphore);
  sem_destroy(&mutex);
  pthread_mutex_destroy(&pq_mutex);
  pthread_mutex_destroy(&finish_mutex);

  return 0;
}
