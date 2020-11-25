#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>

// Global variables for matrices' dimensions
int a_row, a_col, b_row, b_col;

struct element_data {
    int **a, **b, **c;
    int i, j;
};

// Read each matrix from file and put elements in 2d array of integers
// b_input boolean to switch between 1st matrix file and 2nd matrix file, also helps in confirming dimensions
int **read_file(char *file_path, bool b_input) {
    FILE *fptr = fopen(file_path, "r");     // Open file in read mode

    if (fptr == NULL) {     // Check if file doesn't exist
        fprintf(stderr, "Cannot open one of the files\n");
        exit(1);
    }

    // Take row and column of matrix
    int row, col;
    // If the returned number of inputs taken is not 2 (row & column) exits and print error
    // Or if row and column numbers are not integers
    if (fscanf(fptr, "row=%d col=%d\n", &row, &col) != 2) {
        fprintf(stderr, "Cannot get row or column number\n");
        exit(1);
    }

    // Switch between taking input of a matrix or b matrix
    if (!b_input) {
        a_row = row, a_col = col;
    } else {
        b_row = row, b_col = col;

        // Check for dimensions can multiply after taking dimensions of 2nd matrix
        if (a_col != b_row) {
            fprintf(stderr, "1st matrix column size doesn't match 2nd matrix row size\n");
            exit(1);
        }
    }

    // Allocate 2d array storing matrix elements
    int **arr = (int **) malloc(row * sizeof(int *));
    for (int i = 0; i < row; ++i) {
        arr[i] = (int *) malloc(col * sizeof(int));
    }

    // Taking input line by line from file, then split it according to delimiters,
    // then check each token is made of numbers only and smaller than INT_MAX
    char *line = NULL, *delim = " \n\t";
    size_t len = 0;
    char *split;
    for (int i = 0; i < row; ++i) {
        if (getline(&line, &len, fptr) != -1) {     // Taking line from file
            split = strtok(line, delim);            // Split line according to delimiters
            for (int j = 0; j < col; ++j) {
                if (split == NULL) {                // Check if there is missing element
                    fprintf(stderr, "Wrong number of elements in row %d\n", i + 1);
                    exit(1);
                }

                // Check if number is negative and not a sign only
                bool neg = (split[0] == '-');
                if (neg && strlen(split) == 1) {
                    fprintf(stderr, "Element at row %d column %d is not numeric only\n", i + 1, j + 1);
                    exit(1);
                }
                // Check string if there is non digit
                int k = neg;
                for (; k < strlen(split); ++k) {
                    if (!isdigit(split[k])) {
                        fprintf(stderr, "Element at row %d column %d is not numeric only\n", i + 1, j + 1);
                        exit(1);
                    }
                }
                // Check if string has digits greater than that of int
                int max_digits_int = 10;
                if (k > max_digits_int) {
                    fprintf(stderr, "Element size is greater than int at row %d column %d\n", i + 1, j + 1);
                    exit(1);
                }

                arr[i][j] = atoi(split);            // Convert string to int and put in array
                split = strtok(NULL, delim);     // Continue looping through tokens
            }
        } else {
            fprintf(stderr, "Cannot read matrix elements\n");
            exit(1);
        }
    }

    fclose(fptr);   // Close file pointer and return array
    return arr;
}

// Check for multiplication overflow
int mult_overflow(int x, int y, int i, int j) {
    int z = x * y;
    if (x != 0 && z / x != y) {
        fprintf(stderr, "Overflow will occur while computing element at row %d column %d\n", i + 1, j + 1);
        exit(1);
    }
    return z;
}

// Check for addition overflow
int add_overflow(int x, int y, int i, int j) {
    if (x >= 0) {
        if (INT_MAX - x < y) {  // Overflow will occur
            fprintf(stderr, "Overflow will occur while computing element at row %d column %d\n", i + 1, j + 1);
            exit(1);
        }
    } else {
        if (y < INT_MIN - x) {
            fprintf(stderr, "Overflow will occur while computing element at row %d column %d\n", i + 1, j + 1);
            exit(1);
        }
    }
    return x + y;
}

// Non threaded matrix multiplication used for testing
void compute(int **a, int **b, int **c) {
    for (int i = 0; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            c[i][j] = 0;
            for (int k = 0; k < a_col; ++k) {
                c[i][j] += a[i][k] * b[k][j];
            }
        }
    }
}

// Compute each row of output in a separate thread
void *compute_for_row(void *tdata) {
    // Retrieve thread data
    struct element_data *row_data;
    row_data = (struct element_data *) tdata;
    int i = row_data->i;
    int **a = row_data->a, **b = row_data->b, **c = row_data->c;

    for (; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            c[i][j] = 0;
            for (int k = 0; k < a_col; ++k) {
                // Check for overflow while adding or multiplying
                int tmp = mult_overflow(a[i][k], b[k][j], i, j);
                c[i][j] = add_overflow(c[i][j], tmp, i, j);
            }
        }
    }
    pthread_exit(NULL);
}

// Compute each element of output in a separate thread
void *compute_for_element(void *tdata) {
    // Retrieve thread data
    struct element_data *e_data;
    e_data = (struct element_data *) tdata;
    int i = e_data->i, j = e_data->j;
    int **a = e_data->a, **b = e_data->b, **c = e_data->c;

    c[i][j] = 0;
    for (int k = 0; k < a_col; ++k) {
        // Check for overflow while adding or multiplying
        int tmp = mult_overflow(a[i][k], b[k][j], i, j);
        c[i][j] = add_overflow(c[i][j], tmp, i, j);
    }
    pthread_exit(NULL);
}

// Create thread for each row to be computed
void thread_for_row(int **a, int **b, int **c) {
    pthread_t threads[a_row];
    int rc;
    for (int i = 0; i < a_row; ++i) {
        struct element_data *tdata = malloc(sizeof(struct element_data));
        tdata->a = a, tdata->b = b, tdata->c = c;
        tdata->i = i;
        rc = pthread_create(&threads[i], NULL, compute_for_row, (void *) tdata);

        if (rc) {   // Check if thread is not created
            fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
            exit(-1);
        }
    }

    // Join threads
    for (int i = 0; i < a_row; ++i) {
        pthread_join(threads[i], NULL);

        if (rc) {   // Check if thread is not joined
            fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
            exit(-1);
        }
    }
    printf("Thread for each row method: \nNumber of threads: %d\n", a_row);
}

// Create thread for each element to be computed
void thread_for_element(int **a, int **b, int **c) {
    pthread_t threads[a_row * b_col];
    int rc;
    for (int i = 0; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            struct element_data *tdata = malloc(sizeof(struct element_data));
            tdata->a = a, tdata->b = b, tdata->c = c;
            tdata->i = i, tdata->j = j;
            rc = pthread_create(&threads[i * b_col + j], NULL, compute_for_element, (void *) tdata);

            if (rc) {   // Check if thread is not created
                fprintf(stderr, "ERROR; return code from pthread_create() is %d\n", rc);
                exit(-1);
            }
        }
    }

    // Join threads
    for (int i = 0; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            pthread_join(threads[i * b_col + j], NULL);

            if (rc) {   // Check if thread is not joined
                fprintf(stderr, "ERROR; return code from pthread_join() is %d\n", rc);
                exit(-1);
            }
        }
    }
    printf("Thread for each element method: \nNumber of threads: %d\n", a_row * b_col);
}

// Writes output array to a file specified by user
void write_output(int **c, char *file_path) {
    FILE *fptr = fopen(file_path, "w");
    fprintf(fptr, "row=%d col=%d\n", a_row, b_col);

    for (int i = 0; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            if (j != b_col - 1) {
                fprintf(fptr, "%d\t", c[i][j]);
            } else {
                fprintf(fptr, "%d\n", c[i][j]);
            }
        }
    }
    fclose(fptr);
}

// Prints output array to terminal, used in testing
void show(int **c) {
    for (int i = 0; i < a_row; ++i) {
        for (int j = 0; j < b_col; ++j) {
            printf("%d ", c[i][j]);
        }
        printf("\n");
    }
}


int main(int argc, char *argv[]) {
    // Default files' names
    char *mat_a_path = "a.txt", *mat_b_path = "b.txt", *mat_c_path = "c.out";

    // Taking arguments if exist
    if (argc == 2) {
        mat_a_path = argv[1];
    } else if (argc == 3) {
        mat_a_path = argv[1];
        mat_b_path = argv[2];
    } else if (argc == 4) {
        mat_a_path = argv[1];
        mat_b_path = argv[2];
        mat_c_path = argv[3];
    } else if (argc > 4) {      // Print error if too many arguments
        fprintf(stderr, "Too many arguments\n");
        exit(1);
    }

    // Read each matrix from file and put elements in 2d array of integers
    int **a = read_file(mat_a_path, 0);
    int **b = read_file(mat_b_path, 1);

    // Allocate memory for output matrix
    int **c = (int **) malloc(a_row * sizeof(int *));
    for (int i = 0; i < a_row; ++i) {
        c[i] = (int *) malloc(b_col * sizeof(int));
    }

//    // Non threaded matrix multiplication for testing
//    compute(a, b, c);
//    show(c);

    // Calculating time taken for running 1st method (thread for row)
    struct timeval start, stop;
    gettimeofday(&start, NULL);

    thread_for_row(a, b, c);

    gettimeofday(&stop, NULL);
    printf("Seconds taken %lu\n", stop.tv_sec - start.tv_sec);
    printf("Microseconds taken: %lu\n", stop.tv_usec - start.tv_usec);
//    show(c);

    // Calculating time taken for running 2nd method (thread for element)
    gettimeofday(&start, NULL);

    thread_for_element(a, b, c);

    gettimeofday(&stop, NULL);
    printf("Seconds taken %lu\n", stop.tv_sec - start.tv_sec);
    printf("Microseconds taken: %lu\n", stop.tv_usec - start.tv_usec);
//    show(c);

    write_output(c, mat_c_path);    // Write output matrix to file

    // Free allocated memory
    free(a);
    free(b);
    free(c);
    return 0;
}