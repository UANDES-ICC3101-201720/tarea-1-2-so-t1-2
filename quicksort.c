#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include "types.h"
#include "const.h"
#include "util.h"

#define SWAP(x, y) do { typeof(x) SWAP = x; x = y; y = SWAP; } while (0)

int partition(UINT* A, int lo, int hi){
    int pivot = A[hi];
    int i = lo;
    for (int j = lo; j < hi; ++j) {
        if (A[j] < pivot) {
            SWAP(A[i], A[j]);
            i++;
        }
    }
    SWAP(A[i], A[hi]);
    return i;
}

int quicksort(UINT* A, int lo, int hi) {
    if (lo < hi){
        int p = partition(A, lo, hi);
        quicksort(A, lo, p - 1);
        quicksort(A, p + 1, hi);
    }
    return 0;
}

typedef struct __lrret_t{
    int lo;
    int hi;
    int small;
} lrret_t;

typedef struct __lrargs_t{
    UINT *A;
    int lo;
    int hi;
    int *pivot;
    lrret_t *ret;
} lrargs_t;

int global_rearrangement(UINT* A, int lo, int hi, int p, lrret_t* props){
    /* Receives the array A, number of sub-blocks, and an array with the properties of each sub-block i.e. small index, lo, hi.
     * It copies each small value to a temporary array, then copies the rest of the array
     * Finally, transfers the temporary array onto A
     * Returns position of last small*/

    // Calculate how many smalls there are in A
    int total_smalls = 0;
    for (int i = 0; i < p; ++i) {
        total_smalls = total_smalls + props[i].small - props[i].lo + 1;
    }
    // Rearrange A using method described in http://parallelcomp.uw.hu/ch09lev1sec4.html Fig.9.19
    int small_prefix_sum[total_smalls+1];
    small_prefix_sum[0] = 0;
    for (int j = 0; j < p; ++j) {
        small_prefix_sum[j+1] = small_prefix_sum[j] + props[j].small - props[j].lo + 1;
    }
    UINT* temp = malloc(sizeof(UINT) * (hi-lo+1)); // Can also be in stack
    int i = 0;
    for (int k = 0; k < p; ++k) {
        for (int l = props[k].lo; l <= props[k].small; ++l) {
            temp[i++] = A[l];
            A[l] = UINT_MAX;
        }
    }
    int last_small = i - lo + 1; // Value to be returned
    for (int m = lo; m < hi - lo + 1; ++m) {
        if (A[m] != UINT_MAX){
            temp[i++] = A[m];
        }
    }
    // Copy work back to A
    for (int n = lo; n <= hi; ++n) {
        A[n] = temp[n-lo];
    }

    free(temp);
    return last_small;


}

/* Global mutex */
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t pivot_selected = PTHREAD_COND_INITIALIZER;

void* local_rearrangement(void *args){
    /* Receives a struct lrargs_t with input parameters
     * Returns position of last small, lo and hi */
    lrargs_t *m = (lrargs_t *) args;

    pthread_mutex_lock(&mut);
    // Select a pivot and broadcast it to other threads
    if (*m->pivot == -1){
        *m->pivot = m->A[m->hi]; // Select the last element as pivot
    }
    while(*m->pivot == -1){
        pthread_cond_wait (&pivot_selected, &mut);
    }
    pthread_mutex_unlock (&mut);
    printf("out of lock\n");
    pthread_cond_broadcast (&pivot_selected);

    // Do the actual arrangement
    int i = m->lo;
    for (int j = m->lo; j <= m->hi; ++j) {
        if (m->A[j] <= *m->pivot) {
            SWAP(m->A[i], m->A[j]);
            i++;
        }
    }

//    lrret_t *r = malloc(sizeof(lrret_t));
    lrret_t *r = m->ret;


    r->lo = m->lo;
    r->hi = m->hi;
    r->small = i - 1; // TODO: i-1?
    return 0;
}

int recursive_parallel_quicksort(UINT* A, int lo, int hi, int p) {
    /* Receives a block and a number of threads */
    if (p == 1){ // End of recursion if only one process is assigned
        quicksort(A, lo, hi);
    } else if (p > 1) {
        int itemsPerThread = (hi - lo + 1) / p;

        /* Set-up for threads */
        pthread_t threads[p];
        lrargs_t *lrarg_arr = malloc(sizeof(lrargs_t)*p);
        // Somewhere to receive return values
        lrret_t *lrret_arr = malloc(sizeof(lrret_t) * p);

        int* pivot = malloc(sizeof(int));
        *pivot = -1;

        int i;
        for (i = 0; i < p - 1; ++i) {
            // pass to function (lo + i*itemsPerThread) and (lo + (i+1)*itemsPerThread -1)
            lrarg_arr[i].pivot = pivot;
            lrarg_arr[i].A = A;
            lrarg_arr[i].lo = lo + i*itemsPerThread;
            lrarg_arr[i].hi = lo + (i+1)*itemsPerThread -1;
            lrarg_arr[i].ret = &lrret_arr[i];

            if (pthread_create(&threads[i], NULL, local_rearrangement, &lrarg_arr[i]) != 0){ // lrarg_arr[i] o &lrarg_arr[i]??
                perror("Filed to create thread");
                exit(1);
            }
        }
        // pass to function (lo + i*itemsPerThread) and (hi)
        lrarg_arr[i].pivot = pivot;
        lrarg_arr[i].A = A;
        lrarg_arr[i].lo = lo + i*itemsPerThread;
        lrarg_arr[i].hi = hi;
        lrarg_arr[i].ret = &lrret_arr[i];
        if (pthread_create(&threads[i], NULL, local_rearrangement, &lrarg_arr[i]) != 0){
            perror("Filed to create thread");
            exit(1);
        }


        // Wait until all threads are completed (join) TODO: return values to struct! Is right?
        for (int j = 0; j < p; j++)
//            pthread_join(threads[j], (void **) &lrret_arr[j]); // is return correct?
            pthread_join(threads[j], NULL);
        // TODO: unpack return values. Is right?

        // Pass the modified array to global_rearrangement along with the small-index-array and p
        int last_small = global_rearrangement(A, lo, hi, p, lrret_arr); // TODO: right?

        // According to previous return value, calculate p for next iterations
        //int new_p = floor(|S|*p/n + 0.5)
        int new_p = (int) floor( (float) last_small * ((float)p / (float)(hi-lo+1) ) + 0.5);

        // Recursive call to the new partition
        if (new_p < 0){
            printf("new_p is negative!");
            new_p = 1;
        }
        recursive_parallel_quicksort(A, lo, last_small, new_p);
        recursive_parallel_quicksort(A, last_small + 1, hi, p - new_p);


        free(pivot); free(lrarg_arr); free(lrret_arr);
        return 0;

    } else { // There was an error with the number of threads assigned to this block
        return 1;
    }
}

int parallel_quicksort(UINT* A, int lo, int hi) {
    /* Get the number of CPU cores available */
    int cores = sysconf(_SC_NPROCESSORS_ONLN);
    int p = cores * 2;

    int rpqs = recursive_parallel_quicksort(A, lo, hi, p);
    if (rpqs != 0){
        printf("There was an error");
    }

    return 0;
}

int main(int argc, char** argv) {
    printf("[quicksort] Starting up...\n");

    /* Get the number of CPU cores available */
    int cores = sysconf(_SC_NPROCESSORS_ONLN);

    /* parse arguments with getopt */

    int tvalue = 3;
    int evalue = 1;
    int pvalue = 1000;

    int index;
    int c;

    opterr = 0;


    while ((c = getopt (argc, argv, "T:E:P:")) != -1)
        switch (c)
        {
            case 'T':
                tvalue = atoi(optarg);
                break;
            case 'E':
                evalue = atoi(optarg);
                break;
            case '?':
                if (optopt == 'T')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (optopt == 'E')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else if (isprint (optopt))
                    fprintf (stderr, "Unknown option `-%c'.\n", optopt);
                else
                    fprintf (stderr,
                             "Unknown option character `\\x%x'.\n",
                             optopt);
                return 1;
            default:
                abort ();
        }

    /* Start datagen as a child process. */

    int child = fork();
    if (child < 0) {
        fprintf(stderr, "forking a child failed\n");
        exit(-1);
    } else if (child == 0) { // Child process
        close(STDOUT_FILENO); // Close stdout for datagen
        char *myargs[2];
        myargs[0] = strdup("./datagen");
        myargs[1] = NULL;
        execvp(myargs[0], myargs);
    } else { // parent
        sleep(1); // wait till child is up and running
    }

    /* Create the domain socket to talk to datagen. */
    /* Allocate memory for the data set array according with size T */
    UINT readvalues = 0;
    size_t numvalues = pow(10, tvalue);
    size_t readbytes = 0;

    UINT *readbuf = malloc(sizeof(UINT) * numvalues);

    // Begin string to datagen
    char data[10];
    strcpy(data,"BEGIN U ");
    char t[2];
    sprintf(t, "%d", tvalue);
    strcat(data, t);

    // pre req socket
    struct sockaddr_un addr;
    int fd,cl,rc;

    // Create socket
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("[quicksort] Error creating socket.\n");
        exit(-1);
    }
    // Set address pointing DSOCKET_PATH (i.e. /tmp/dg.sock)
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, DSOCKET_PATH, sizeof(addr.sun_path) - 1);

    // Connect to datagen
    if (connect(fd, (struct sockaddr *) &addr, sizeof(addr)) == -1) {
        perror("[quicksort] Connection error.\n");
        exit(-1);
    }


    /* DEMO: request two sets of unsorted random numbers to datagen */
    for (int i = 0; i < 2; i++) {
        /* T value 3 hardcoded just for testing. */
        char *begin = "BEGIN U 3";
        int rc = strlen(begin);

        /* Request the random number stream to datagen */
        if (write(fd, begin, strlen(begin)) != rc) {
            if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
            else {
                perror("[quicksort] write error.\n");
                exit(-1);
            }
        }

        /* validate the response */
        char respbuf[10];
        read(fd, respbuf, strlen(DATAGEN_OK_RESPONSE));
        respbuf[strlen(DATAGEN_OK_RESPONSE)] = '\0';

        if (strcmp(respbuf, DATAGEN_OK_RESPONSE)) {
            perror("[quicksort] Response from datagen failed.\n");
            close(fd);
            exit(-1);
        }

        UINT readvalues = 0;
        size_t numvalues = pow(10, 3);
        size_t readbytes = 0;

        UINT *readbuf = malloc(sizeof(UINT) * numvalues);

        while (readvalues < numvalues) {
            /* read the bytestream */
            readbytes = read(fd, readbuf + readvalues, sizeof(UINT) * 1000);
            readvalues += readbytes / 4;
        }

        /* Print out the values obtained from datagen */
        for (UINT *pv = readbuf; pv < readbuf + numvalues; pv++) {
            printf("%u\n", *pv);
        }

        free(readbuf);
    }

    /* Issue the END command to datagen */
    int rc = strlen(DATAGEN_END_CMD);
    if (write(fd, DATAGEN_END_CMD, strlen(DATAGEN_END_CMD)) != rc) {
        if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
        else {
            perror("[quicksort] write error.\n");
            close(fd);
            exit(-1);
        }
    }

    free(readbuf);
    close(fd);
    exit(0);
}