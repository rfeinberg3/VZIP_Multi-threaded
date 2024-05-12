#include <dirent.h> 
#include <stdio.h> 
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <time.h>
#include <pthread.h>
#include <semaphore.h>

#define BUFFER_SIZE 1048576 // 1MB
#define MAX_THREADS 19
#define MAX_NODES 100

// Producer arg type structures
typedef struct {
	char *full_path;
	int index;
} pthread_produce_arg;

// Consumer return type structure
typedef struct {
	unsigned char *buffer_out;
	int nbytes_zipped;
	int index;
} pthread_consumer_ret;

// Linked List structure
struct Node {
	unsigned char *buffer_in; // to be read by consumers 
    int size; // Size of the data in the buffer in bytes (nbytes)
	int index; // index of current nodes file
    struct Node *next; // Pointer to the next node in the list
};

// Globals: Initialize compression vars
pthread_mutex_t lock;
pthread_cond_t item, space;
sem_t thread_lim;
struct Node *head; // Initialize head of the linked list
int number_nodes = 0; // to keep track of list size/whos turn it is (producer or consumer?)

// lock and condition initializers
void rwlock_init() {
	pthread_mutex_init(&lock, NULL);
	pthread_cond_init(&item, NULL);
	pthread_cond_init(&space, NULL);
	sem_init(&thread_lim, 0, MAX_THREADS);
}

// Initialize producer data arguments
pthread_produce_arg *initialize_values(char *full_path_str, int index) {
	pthread_produce_arg *obj = (pthread_produce_arg *) malloc(sizeof(pthread_produce_arg));
	obj->full_path = full_path_str;
	obj->index = index;
    return obj;
}

// Add node to global linked list
void add(struct Node **p, struct Node *new, unsigned char *buffer_in, int nbytes, int index) {
    if(*p == NULL || (*p)->index >= index) { // try sorting
		new->buffer_in = buffer_in;
		new->size = nbytes;
        new->index = index;

        new->next = *p;
        *p = new;
    }
    else { // try sorting
        add(&(*p)->next, new, buffer_in, nbytes, index);
	}
}

// Producer Thread Function !!!!
void *adder(void *arg) {
	sem_wait(&thread_lim);
    /*    
      Adds to the data structure by calling add() on the head of the linked list
	to add the data node (buffer_in, nbytes, index) to be used by the consumer thread.

    :param arg: data->full_path(char *) = path of file to read in, data->index(int) = index of file read in
    :returns: nbytes(int) = number of bytes read into buffer_in to add to total_in
    */

    pthread_produce_arg *data = (pthread_produce_arg *) arg;

	// Initializee variables
	FILE *f_in = fopen(data->full_path, "r"); // open this files path
	assert(f_in != NULL);
	unsigned char *buffer_in = (unsigned char *)malloc(sizeof(unsigned char)*BUFFER_SIZE); 
	int nbytes = fread(buffer_in, sizeof(unsigned char), BUFFER_SIZE, f_in); // Read in file data to compress and get size (nbytes)
	fclose(f_in);

	int index = data->index;
    struct Node *new = (struct Node *)malloc(sizeof(struct Node));
	// Lock conditional and critical section
	pthread_mutex_lock(&lock);
		while(number_nodes >= MAX_NODES) 
			pthread_cond_wait(&space, &lock);
		add(&head, new, buffer_in, nbytes, index); // Add element to linked list
		number_nodes++;
		pthread_cond_signal(&item);
	pthread_mutex_unlock(&lock); 
	int *result = malloc(sizeof(int));
	*result = nbytes; // return the number of bytes read in (to add to total_in)
	sem_post(&thread_lim);
	return (void *) result;
}

// Consumer Thread Function!!!
void *reader(void *arg) {
	sem_wait(&thread_lim);
	/*
		Get buffer_in and relevant data from linked list to compress and return as buffer_out.
	
	:returns: 
		data->buffer_out(unsigned char*) = compressed file data to write,
		data->nbytes_zipped(int) = number of bytes in buffer_out (after compression)
		data->index(int) = index of file (what order it is supposed to be written in)
	*/

	// Initialize variables
	unsigned char *buffer_in = malloc(sizeof(unsigned char)*BUFFER_SIZE);
	unsigned char *buffer_out = malloc(sizeof(unsigned char)*BUFFER_SIZE); 
	char *name = malloc(sizeof(unsigned char*));
	int size;
	int index;

	// zip file intializers
	z_stream strm;
	int ret = deflateInit(&strm, 9);
	assert(ret == Z_OK);

	// Get data at the head of the linked list
	pthread_mutex_lock(&lock);
		while(number_nodes == 0) 
			pthread_cond_wait(&item, &lock);
		// Get function begin
		assert(head != NULL);
		buffer_in = head->buffer_in;
		size = head->size;
		index = head->index;
		head = head->next; // Chop Head
		// Get function end
		number_nodes--;
		pthread_cond_signal(&space);
	pthread_mutex_unlock(&lock);
	strm.avail_in = size;
	strm.next_in = buffer_in;
	strm.avail_out = BUFFER_SIZE;
	strm.next_out = buffer_out;

	ret = deflate(&strm, Z_FINISH);
	assert(ret == Z_STREAM_END);
	free(buffer_in);
	// dump zipped file
	int nbytes_zipped = BUFFER_SIZE-strm.avail_out;
	pthread_consumer_ret *result = (pthread_consumer_ret *)malloc(sizeof(pthread_consumer_ret));
	result->buffer_out = buffer_out;
	result->nbytes_zipped = nbytes_zipped;
	result->index = index;
	sem_post(&thread_lim);
	return (void *) result;
}

// Compare file paths
int cmp(const void *a, const void *b) {
	return strcmp(*(char **) a, *(char **) b);
}

// Compare compressed output structures to find correct write order
int cmp_rets(const void *a, const void *b) {
	pthread_consumer_ret* retA = *(pthread_consumer_ret**) a;
    pthread_consumer_ret* retB = *(pthread_consumer_ret**) b;

    // Return negative if index of a < index of b
    // Return zero if index of a == index of b
    // Return positive if index of a > index of b
    return retA->index - retB->index ;
}

// BEGIN MAIN //
int main(int argc, char **argv) {
	// time computation header
	struct timespec start, end;
	clock_gettime(CLOCK_MONOTONIC, &start);
	// end of time computation header

	// do not modify the main function before this point!
	assert(argc == 2);

	DIR *d;
	struct dirent *dir;
	char **files = NULL;
	int nfiles = 0;

	d = opendir(argv[1]);
	if(d == NULL) {
		printf("An error has occurred\n");
		return 0;
	}

	// create sorted list of PPM files
	while ((dir = readdir(d)) != NULL) {
		files = realloc(files, (nfiles+1)*sizeof(char *));
		assert(files != NULL);

		int len = strlen(dir->d_name);
		if(dir->d_name[len-4] == '.' && dir->d_name[len-3] == 'p' && dir->d_name[len-2] == 'p' && dir->d_name[len-1] == 'm') {
			files[nfiles] = strdup(dir->d_name);
			assert(files[nfiles] != NULL);
			nfiles++;
		}
	}
	closedir(d);
	qsort(files, nfiles, sizeof(char *), cmp);
	  //printf("nfiles= %d\n", nfiles);

	// create a single zipped package with all PPM files in lexicographical order
	int total_in = 0, total_out = 0;
	FILE *f_out = fopen("video.vzip", "w");
	assert(f_out != NULL);

	// Initialize thread helper variables
	pthread_t writer_threads[nfiles];
	pthread_t reader_threads[nfiles];
	int writer_thread_index[nfiles];
	int reader_thread_index[nfiles];
	pthread_consumer_ret **rets = malloc((nfiles)*sizeof(pthread_consumer_ret*));
	rwlock_init(); // Initialize locks,condition, and semaphores

	// Enter main loop
	for(int i=0; i < nfiles; i++) {
		int len = strlen(argv[1])+strlen(files[i])+2;
		char *full_path = malloc(len*sizeof(char));
		assert(full_path != NULL);
		strcpy(full_path, argv[1]);
		strcat(full_path, "/");
		strcat(full_path, files[i]);

		// Create Producer Thread
		pthread_produce_arg *vals1 = initialize_values(full_path, i);
		writer_thread_index[i] = pthread_create(
										&writer_threads[i], // thread id
										NULL, // attributes
										adder, // thread function
										(void *) vals1); // thread arguments
		
		// Create Consumer Thread
		reader_thread_index[i] = pthread_create(
										&reader_threads[i], // thread id
										NULL, // attribute
										reader, // thread function
										NULL); // thread arguments
	} // End thread creation

	// Join Producer Threads
	for(int i = 0; i < nfiles; i++) {
		int *ret;
		if(writer_thread_index[i] == 0) {
			writer_thread_index[i] = pthread_join ( 
								writer_threads[i], (void **) &ret 
								) + 1;// join the thread at the index that 
										// one will be created next. 
										// Set return to show thread data was taken, 
										// no need to work with this thread further.
			total_in += *ret;
		}					
	}
	// Join Consumer Threads
	for(int i = 0; i < nfiles; i++) {
		if(reader_thread_index[i] == 0) {
			pthread_consumer_ret *ret;
			reader_thread_index[i] = pthread_join ( 
								reader_threads[i], (void **) &ret 
								) + 1; 
			
			rets[i] = ret; 
		}
	} // End threads joins

	// Write zip file
	qsort(rets, nfiles, sizeof(pthread_consumer_ret *), cmp_rets); // sort order to write
	for(int i = 0; i < nfiles; i++) {	
		//printf("writes file: |%s| at index: %d\n", rets[i]->name, rets[i]->index);
		fwrite(&(rets[i]->nbytes_zipped), sizeof(int), 1, f_out);
		total_out += rets[i]->nbytes_zipped;
		fwrite(rets[i]->buffer_out, sizeof(unsigned char), rets[i]->nbytes_zipped, f_out);
		free(rets[i]);
	}
	fclose(f_out);
	//printf("total_in = %d\ntotal_out = %d\n", total_in, total_out);

	printf("Compression rate: %.2lf%%\n", 100.0*(total_in-total_out)/total_in);

	// release list of files
	for(int i=0; i < nfiles; i++)
		free(files[i]);
	free(files);

	// do not modify the main function after this point!

	// time computation footer
	clock_gettime(CLOCK_MONOTONIC, &end);
	printf("Time: %.2f seconds\n", ((double)end.tv_sec+1.0e-9*end.tv_nsec)-((double)start.tv_sec+1.0e-9*start.tv_nsec));
	// end of time computation footer
	return 0;
}
