#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

// A struct that represents a reader-writer lock
typedef struct {
    // A mutex that protects the shared data and the counters
    pthread_mutex_t mutex;
    // A condition variable that signals when the lock is available
    pthread_cond_t cv;
    // A counter that keeps track of the number of active readers
    int readers;
    // A flag that indicates whether there is an active writer
    int writer;
} rwlock_t;

// A function that initializes a reader-writer lock
void rwlock_init(rwlock_t *lock) {
    // Initialize the mutex
    pthread_mutex_init(&lock->mutex, NULL);
    // Initialize the condition variable
    pthread_cond_init(&lock->cv, NULL);
    // Initialize the number of active readers to zero
    lock->readers = 0;
    // Initialize the flag of active writer to zero
    lock->writer = 0;
}

// A function that acquires a read lock
void readLock(rwlock_t *lock) {
    // Lock the mutex
    pthread_mutex_lock(&lock->mutex);

    // Wait until there is no active writer
    while (lock->writer == 1) {
        pthread_cond_wait(&lock->cv, &lock->mutex);
    }

    // Increment the number of active readers
    lock->readers++;

    // Unlock the mutex
    pthread_mutex_unlock(&lock->mutex);
}

// A function that releases a read lock
void readUnlock(rwlock_t *lock) {
    // Lock the mutex
    pthread_mutex_lock(&lock->mutex);

    // Decrement the number of active readers
    lock->readers--;

    // If there are no more active readers, wake waiting threads
    if (lock->readers == 0) {
        pthread_cond_broadcast(&lock->cv);
    }

    // Unlock the mutex
    pthread_mutex_unlock(&lock->mutex);
}

// A function that acquires a write lock
void writeLock(rwlock_t *lock) {
    // Lock the mutex
    pthread_mutex_lock(&lock->mutex);

    // Wait until there is no active writer or reader
    while (lock->writer == 1 || lock->readers > 0) {
        pthread_cond_wait(&lock->cv, &lock->mutex);
    }

    // Set the writer flag with appropriate value
    lock->writer = 1;

    // Unlock the mutex
    pthread_mutex_unlock(&lock->mutex);
}

// A function that releases a write lock
void writeUnlock(rwlock_t *lock) {
    // Lock the mutex
    pthread_mutex_lock(&lock->mutex);

    // Set the writer flag with appropriate value
    lock->writer = 0;

    // Wake all waiting readers and writers
    pthread_cond_broadcast(&lock->cv);

    // Unlock the mutex
    pthread_mutex_unlock(&lock->mutex);
}

// global variables that represent the shared data
// open a file named "file.txt" in read-write mode, creating it if it doesn't exist.
// Also ensure every write follows append.
int fd;
int data = 0;

// A global variable that represents the reader-writer lock
rwlock_t lock;

// This mutex protects the file offset when readers use lseek() and read()
pthread_mutex_t fileMutex;

// A function that simulates a reader thread
void *reader(void *arg) {
    // Get the thread id from the argument
    int id = *(int *)arg;

    // Acquire a read lock
    readLock(&lock);

    printf("Reader %d: data = %d\n", id, data);

    // Read the file and print its content in the terminal
    char buffer[101];
    int bytesRead;

    pthread_mutex_lock(&fileMutex);

    lseek(fd, 0, SEEK_SET);
    bytesRead = read(fd, buffer, 100);

    if (bytesRead < 0) {
        perror("read");
    } else {
        buffer[bytesRead] = '\0';
        printf("Reader thread id: %d -> with data: %d -> file content: %s\n", id, data, buffer);
    }

    pthread_mutex_unlock(&fileMutex);

    // Release a read lock
    readUnlock(&lock);

    return NULL;
}

// A function that simulates a writer thread
void *writer(void *arg) {
    // Get the thread id from the argument
    int id = *(int *)arg;

    // Acquire a write lock
    writeLock(&lock);

    data++;
    printf("Writer %d: data = %d\n", id, data);

    // Write the writer thread id and data to the file
    char message[100];
    int length;

    length = snprintf(message, sizeof(message), "Writer thread: %d with data: %d\n", id, data);
    write(fd, message, length);

    // Release a write lock
    writeUnlock(&lock);

    return NULL;
}

// A constant that defines the number of reader threads
#define NUM_READERS 5

// A constant that defines the number of writer threads
#define NUM_WRITERS 3

// The main function
int main(int argc, char *argv[]) {
    // Open file.txt in read-write mode, create it if needed, and append on write
    fd = open("file.txt", O_RDWR | O_CREAT | O_APPEND, 0644);

    if (fd < 0) {
        perror("open");
        return 1;
    }

    // Initialize the reader-writer lock
    rwlock_init(&lock);

    // Initialize the file mutex
    pthread_mutex_init(&fileMutex, NULL);

    // Create an array of reader thread ids
    int readerIds[NUM_READERS];

    // Create an array of writer thread ids
    int writerIds[NUM_WRITERS];

    // Create an array of reader thread handles
    pthread_t readerThreads[NUM_READERS];

    // Create an array of writer thread handles
    pthread_t writerThreads[NUM_WRITERS];

    // Loop through the writer thread ids
    for (int i = 0; i < NUM_WRITERS; i++) {
        // Assign the id to the current index
        writerIds[i] = i + 1;

        // Create a writer thread with the corresponding id
        pthread_create(&writerThreads[i], NULL, writer, &writerIds[i]);
    }

    // Loop through the reader thread ids
    for (int i = 0; i < NUM_READERS; i++) {
        // Assign the id to the current index
        readerIds[i] = i + 1;

        // Create a reader thread with the corresponding id
        pthread_create(&readerThreads[i], NULL, reader, &readerIds[i]);
    }

    // Loop through the writer thread handles
    for (int i = 0; i < NUM_WRITERS; i++) {
        // Join the writer thread
        pthread_join(writerThreads[i], NULL);
    }

    // Loop through the reader thread handles
    for (int i = 0; i < NUM_READERS; i++) {
        // Join the reader thread
        pthread_join(readerThreads[i], NULL);
    }

    close(fd);

    return 0;
}
