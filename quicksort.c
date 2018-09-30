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

// TODO: implement
int parallel_quicksort(UINT* A, int lo, int hi) {
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

    // Experiments loop
    for (int j = 0; j < evalue; ++j) {
        UINT *readbuf = malloc(sizeof(UINT) * numvalues);
        // Send request to datagen
        if (write(fd, data, strlen(data)) == -1) {
            perror("[quicksort] Sending message error.\n");
            exit(-1);
        }
        // Read stream (by CLAUDIO ALVAREZ GOMEZ)
        while (readvalues < numvalues) {
            /* read the bytestream */
            readbytes = read(fd, readbuf + readvalues, sizeof(UINT) * 1000);
            readvalues += readbytes / 4;
        }
        // Print unordered array
        printf("\nE%d:", j+1);
        for (int i = 0; i < 10; ++i) {
            printf("%lu,", (unsigned long) readbuf[i]);
        }


        free(readbuf);
        readbuf = NULL;
    }

    // Send END to datagen
    if (write(fd, DATAGEN_END_CMD, strlen(DATAGEN_END_CMD)) == -1) {
        perror("[quicksort] Sending END message error.\n");
        exit(-1);
    }

//    free(readbuf);

//    /* DEMO: request two sets of unsorted random numbers to datagen */
//    for (int i = 0; i < 2; i++) {
//        /* T value 3 hardcoded just for testing. */
//        char *begin = "BEGIN U 3";
//        int rc = strlen(begin);
//
//        /* Request the random number stream to datagen */
//        if (write(fd, begin, strlen(begin)) != rc) {
//            if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
//            else {
//                perror("[quicksort] write error.\n");
//                exit(-1);
//            }
//        }
//
//        /* validate the response */
//        char respbuf[10];
//        read(fd, respbuf, strlen(DATAGEN_OK_RESPONSE));
//        respbuf[strlen(DATAGEN_OK_RESPONSE)] = '\0';
//
//        if (strcmp(respbuf, DATAGEN_OK_RESPONSE)) {
//            perror("[quicksort] Response from datagen failed.\n");
//            close(fd);
//            exit(-1);
//        }
//
//        UINT readvalues = 0;
//        size_t numvalues = pow(10, 3);
//        size_t readbytes = 0;
//
//        UINT *readbuf = malloc(sizeof(UINT) * numvalues);
//
//        while (readvalues < numvalues) {
//            /* read the bytestream */
//            readbytes = read(fd, readbuf + readvalues, sizeof(UINT) * 1000);
//            readvalues += readbytes / 4;
//        }
//
//        /* Print out the values obtained from datagen */
//        for (UINT *pv = readbuf; pv < readbuf + numvalues; pv++) {
//            printf("%u\n", *pv);
//        }
//
//        free(readbuf);
//    }
//
//    /* Issue the END command to datagen */
//    int rc = strlen(DATAGEN_END_CMD);
//    if (write(fd, DATAGEN_END_CMD, strlen(DATAGEN_END_CMD)) != rc) {
//        if (rc > 0) fprintf(stderr, "[quicksort] partial write.\n");
//        else {
//            perror("[quicksort] write error.\n");
//            close(fd);
//            exit(-1);
//        }
//    }

//    free(readbuf);
    close(fd);
    exit(0);
}