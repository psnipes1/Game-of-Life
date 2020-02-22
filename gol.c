/*
 * Swarthmore College, CS 31
 * Copyright (c) 2018 Swarthmore College Computer Science Department,
 * Swarthmore PA, Professors Tia Newhall and Kevin Webb
 * Parker Snipes and Akshay Srinivasan
 * A parallelized implementation of Conway's Game of Life.
 */

#include <pthread.h>
#include <pthreadGridVisi.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#include "colors.h"

#define OUTPUT_NONE (0)
#define OUTPUT_TEXT (1)
#define OUTPUT_VISI (2)

/* For counting the number of live cells in each round. */
static int live = 0;

/* The global board updated by all threads and printed by the printing thread
 * at the end of each round. */
static char* master_board;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;


/* These values need to be shared by all the threads for visualization.
 * You do NOT need to worry about synchronized access to these, the graphics
 * library will take care of that for you. */
static visi_handle handle;
static color3 *image_buf;
static char *visi_name = "Parallel GOL";


/* This struct represents all the data you need to keep track of your GOL
 * simulation.  Rather than passing individual arguments into each function,
 * we'll pass in everything in just one of these structs.
 *
 * NOTE: You need to use the provided fields here, but you'll also need to
 * add some of your own. */
struct gol_data {
    /* The number of rows on your GOL game board. */
    int rows;

    /* The number of columns on your GOL game board. */
    int cols;

    /* The number of iterations to run your GOL simulation. */
    int iters;

    /* Which form of output we're generating:
     * 0: only print the first and last board (good for timing).
     * 1: print the board to the terminal at each round.
     * 2: draw the board using the visualization library. */
    int output_mode;

    /* TODO: Use this as an ID to give each thread a unique number. */
    int id;

    char* board_memory;

    // TODO: add partitioning information here

    int row_start;

    int row_end;

    int col_start;

    int col_end;

    int print_partition;

    int is_printing;
};
static pthread_barrier_t barrier;
static pthread_barrier_t barrier2;


void print_board(struct gol_data *data, int round) {
    int i, j;

    /* Print the round number. */
    fprintf(stderr, "Round: %d\n", round);

    for (i = 0; i < data->rows; ++i) {
        for (j = 0; j < data->cols; ++j) {

          /* Checks to see if the square in question is alive. */

          if(master_board[i*data->cols+j] == '@'){

                fprintf(stderr, " @");
              }
            else{
                fprintf(stderr, " _");
              }
        }
        fprintf(stderr, "\n");
    }

    /* Print the total number of live cells. */
    fprintf(stderr, "Total live cells: %d\n", live);

    /* Add some blank space between rounds. */
    fprintf(stderr, "\n\n");


}

char* init_board(struct gol_data* board, char filename[], int mode){

  /* Opens the file in question. */
  FILE *infile;
  infile = fopen(filename, "r");
  if (infile == NULL) {
    perror("Bad filename! ");
    exit(1);
  }

  /* Sets the output mode, reads the relevant values from the file into the struct. */

  board->output_mode = mode;
  int pair_number;

  /* Scans the first four values the program needs to put into gol_data.
     If four values aren't found, or an error is returned, print an error
     message and exit. */

  if(fscanf(infile, "%d %d %d %d", &(board->rows),  &(board->cols), &(board->iters), &(pair_number))!= 4){
    perror("Something went wrong reading the first four values from the file!");
    exit(1);
  }

 /* Allocates the chunk of memory for the board. */

  char *array = malloc((board->rows)*(board->cols)*sizeof(char));

 /* Initializes the array to all '_'. */

  for(int i=0; i<((board->rows)*(board->cols)); i++){
    array[i]='_';
  }

  /* Changes the squares specified in the file to be alive on the board. */

  for(int i=0; i<pair_number; i++){
    int row, col;
    /* Scans the next pair of values. If there aren't two values to scan or if
       an error is returned, print an error message and exit. */
    if(fscanf(infile, "%d %d", &row, &col)!=2){
      perror("Something went wrong reading a pair of values from the file!");
      exit(1);
    }
    array[row*(board->cols)+col] = '@';
    live++;
  }

  /* Closes the file, checks for errors just in case. */

  if(fclose(infile)!=0){
    perror("Somehow an error happened trying to close the file. Go figure.");
    exit(1);
  }

  return array;
}

int checknear(struct gol_data* data, int i, int j){

  int to_return = 0;

  /* So we don't have to type them every time. */

  int colnum = data->cols;
  char* mem = master_board;

  /* Holds the squares "above," "below," "to the left" and "to the right" of
    the square index to maintain the torus property. */

  int i_above;
  int i_below;
  int j_below;
  int j_above;

  /* Conditionals to make sure the above variables will adequately wrap around
     the board. */

 /* At the left edge, j_below wraps around. */
  if(j==0){
    j_below = data->cols-1;
    j_above = j+1;
  }
  /* At the right edge, j_above wraps around. */
  else if(j==data->cols-1){
    j_above = 0;
    j_below = j-1;
  }
   /* Or else no side-wrapping occurs.*/
   else{
    j_below = j-1;
    j_above = j+1;
  }

  /* At the bottom edge, i_below wraps around. */
  if(i==0){
    i_below = data->rows-1;
    i_above = i+1;
  }
  /* At the top edge, i_above wraps around. */
  else if(i==data->rows-1){
    i_above = 0;
    i_below = i-1;
  }
  /* Or else no vertical wrapping occurs.*/
  else{
    i_below = i-1;
    i_above = i+1;
  }

 /* Eight conditionals to check each neighbor of i,j and increment the
    return count for each neighbor that is alive. */

  if(mem[i_below*colnum+j_below] == '@'){
    to_return++;
  }
  if(mem[i_below*colnum+j] == '@'){
    to_return++;
  }
  if(mem[i_below*colnum+j_above] == '@'){
    to_return++;
  }
  if(mem[i*colnum+j_below] == '@'){
    to_return++;
  }
  if(mem[i*colnum+j_above] == '@'){
    to_return++;
  }
  if(mem[i_above*colnum+j_below] == '@'){
    to_return++;
  }
  if(mem[i_above*colnum+j] == '@'){
    to_return++;
  }
  if(mem[i_above*colnum+j_above] == '@'){
    to_return++;
  }

  return to_return;
}


void gol_step(color3 *buff, struct gol_data* data) {
    int i, j;
    int updatelive=0;
    for (i = data->row_start; i <=data->row_end; ++i) {
        for (j = data->col_start; j <=data->col_end; ++j) {

            int neighbors = checknear(data,i,j);

            /* Sets a given cell alive if it has exactly 3 neighbors or is
               alive and has exactly two neighbors, and dead otherwise. */

            if(neighbors == 3 || (master_board[i*data->cols+j] == '@' && neighbors == 2)){
              data->board_memory[i*data->cols+j] = '@';
            }
            else{
              data->board_memory[i*data->cols+j] = '_';
            }

            /* When using visualization, also update the graphical board. */
            if (buff != NULL) {
              int buff_index = (data->rows - (i + 1)) * data->cols + j;
              if (data->board_memory[i * data->cols + j]) {
                  /* Live cells get the color using this thread's ID as the index */
                  buff[buff_index] = colors[data->id];
              } else {
                  /* Dead cells are blank. */
                  buff[buff_index] = c3_black;
              }
}
        }
      }

        //Copies the contents of board2 to board.
        pthread_barrier_wait(&barrier);
        pthread_mutex_lock(&m);
        for (i = data->row_start; i <=data->row_end; ++i) {
            for (j = data->col_start; j <=data->col_end; ++j) {
              if(data->board_memory[i*data->cols+j] == '@'&&master_board[i*data->cols+j]!='@'){
                    updatelive++;
                  }
              else if(data->board_memory[i*data->cols+j]!='@'&&master_board[i*data->cols+j]=='@')
              {
                updatelive--;
              }

              master_board[i*data->cols+j] = data->board_memory[i*data->cols+j];
            }
          }


          live=live+updatelive;
         pthread_mutex_unlock(&m);
    /* When using text/graphical output, add a delay and clear the terminal.
     * You can adjust/disable these however you like while debugging. */
    if (data->output_mode > 0) {
        usleep(100000);
        system("clear");
    }
}

void run_gol(color3 *buff, void *appl_data) {
    gol_step(buff, (struct gol_data *)appl_data);
}

void visi_run_animation(struct gol_data *data) {
    init_and_run_animation(data->rows, data->cols, (void *)data, run_gol,
            visi_name, data->iters);
}

/* TODO: copy in your gol_step and other helper functions that implement core
 * game logic.  Most of the important logic (e.g., counting live neighbors and
 * setting cells to live/dead) doesn't need to change.
 *
 * NOTE 1: when doing visualization, it's helpful to have each thread use a
 * different color.  You can use the 'colors' array defined to assign each
 * thread a different color, e.g., buff[buff_index] = colors[data->id];
 *
 * NOTE 2: you'll need to adjust your loops to ensure that each thread is only
 * iterating over its assigned rows and columns rather than the whole board. */



void *worker(void *datastruct) {
    //struct gol_data *data = (struct gol_data*) datastruct;
    struct gol_data *data =datastruct;

    for(int i = 0; i < data->iters; i++){
      if((data->output_mode == OUTPUT_TEXT)){
        gol_step(NULL,data);
        pthread_barrier_wait(&barrier2);
        if(data->is_printing){
          print_board(data,i);

        }
      }
      else if((data->output_mode == OUTPUT_VISI)){
        draw_ready(handle);
        pthread_barrier_wait(&barrier2);
        if(data->is_printing){
        visi_run_animation(data);
      }
    }
      else if((data->output_mode==OUTPUT_NONE))
      {
          gol_step(NULL,data);
          pthread_barrier_wait(&barrier2);
      }
      else
      {
        exit(1);
      }


    if(data->output_mode >= 0){
      usleep(100);
    }


    }


    pthread_barrier_wait(&barrier);

    if(data->print_partition)
      {
    printf("tid: %d, rows: %d : %d (%d) cols: %d : %d (%d) \n",data->id,data->row_start,data->row_end,data->rows,data->col_start,data->col_end, data->cols);
      }
    // TODO: for each round:
    //
    // 1) if the output mode is OUTPUT_TEXT, only one thread should print the
    // board.
    //
    // 2) with appropriate synchronization, execute the core logic of the round
    // (e.g., by calling gol_step)
    //
    // 3) if the output mode is > 0, usleep() for a short time to slow down the
    // speed of the animations.
    //
    // 4) if output mode is OUTPUT_VISI, call draw_ready(handle)

    // TODO: After the final round, one thread should print the final board state

    /* You're not expecting the workers to return anything important. */

    /**
   free(data->board_memory);
    free(data);
    */
    return NULL;
}

int main(int argc, char *argv[]) {
    //TODO declare main's local variables
    int output_mode;
    double secs = 0.0;
    int num_threads = 0;
    int partition = 0;
    int print_partition = 0;
    struct timeval time1;
    struct timeval time2;
    struct gol_data *data = malloc(sizeof(struct gol_data));
    pthread_t *threads;
    int wholeTime;
    int fractionalTime;

    if (argc != 6) {
        printf("Wrong number of arguments.\n");
        printf("Usage: %s <input file> <0|1|2> <num threads> <partition> <print_partition>\n", argv[0]);
        return 1;
    }
    output_mode = atoi(argv[2]);
    data->output_mode=output_mode;
    master_board = init_board(data, argv[1], atoi(argv[2]));
    //data->board_memory=master_board;
    data->board_memory = malloc((data->rows)*(data->cols)*sizeof(char));
    for(int i=0;i<data->rows;i++)
    {
      for(int j=0;j<data->cols;j++)
      {
        data->board_memory[i*data->cols+j]=master_board[i*data->cols+j];
      }
    }
    num_threads = atoi(argv[3]);
    partition = atoi(argv[4]);
    print_partition = atoi(argv[5]);
    pthread_barrier_init(&barrier, NULL, num_threads);
    pthread_barrier_init(&barrier2, NULL, num_threads);
    threads = malloc(num_threads * sizeof(pthread_t));

    /* If we're doing graphics, we need to set up a few things in advance.
     * Other than calling init_pthread_animation, you shouldn't need to change
     * this block.*/
    if (output_mode == OUTPUT_VISI) {
        handle = init_pthread_animation(num_threads,data->rows,data->cols, visi_name,data->iters);
        if (handle == NULL) {
            printf("visi init error\n");
            return 1;
        }

        image_buf = get_animation_buffer(handle);
        if (image_buf == NULL) {
            printf("visi buffer error\n");
            return 1;
        }
    } else {
        handle = NULL;
        image_buf = NULL;
    }

    if(gettimeofday(&time1,NULL)!=0){
        perror("Something went wrong with the time!");
        exit(1);}


    for(int i = 0; i < num_threads; i++){
      struct gol_data *thread_data = malloc(sizeof(struct gol_data));

      //Set the fields of the thread's struct.
      thread_data->board_memory = malloc((data->rows)*(data->cols)*sizeof(char));
      for(int i=0;i<data->rows;i++)
      {
        for(int j=0;j<data->cols;j++)
        {
          thread_data->board_memory[i*data->cols+j]=master_board[i*data->cols+j];
        }
      }
      thread_data->rows = data->rows;
      thread_data->cols = data->cols;
      thread_data->iters = data->iters;
      thread_data->output_mode = output_mode;
      thread_data->id = i;
      thread_data->print_partition = print_partition;
      thread_data->is_printing = 0;
      //Row partition.
      if(partition==0){
        if(num_threads>=data->rows+1){
          if(i <=data->rows){
            thread_data->row_start = i;
            thread_data->col_start = 0;
            thread_data->row_end = i;
            thread_data->col_end = data->cols-1;
          }
          //User can't give you more threads than rows.
          else{
            perror("Too many threads for the number of rows.");
            exit(1);
          }
        }
        //Cases with potentially more than one row per thread.
        else{
          int base = data->rows/num_threads;
          int mod = data->rows % num_threads+1;
          //Case where you get an extra row from the remainder.
          if(i < mod){
            //For row_start, this is intended to be base*i
            //plus the number of previous, extra rows.
            if(i==0)
            {
              thread_data->row_start = 0;
            }
            else
            {
              thread_data->row_start = base*i+i;
            }
            thread_data->col_start = 0;
            thread_data->row_end = thread_data->row_start+base;
            thread_data->col_end = data->cols;
          }
          //Cases where you don't get an extra.
          else{
            thread_data->row_start = base*i+mod;
            thread_data->col_start = 0;
            thread_data->row_end = thread_data->row_start+base-1;
            thread_data->col_end = data->cols;
          }
        }
      }

      //Column partition.
      else{
        if(num_threads >= data->cols+1){
          if(i <=data->cols){
            thread_data->col_start = i;
            thread_data->row_start = 0;
            thread_data->col_end = i;
            thread_data->row_end = data->rows;
          }
          //User can't give you more threads than rows.
          else{
            perror("Too many threads for the number of rows.");
            exit(1);
          }
        }
        //Cases with potentially more than one row per thread.
        else{
          int base = data->cols/num_threads;
          int mod = data->cols % num_threads+1;
          //Case where you get an extra row from the remainder.
          if(i < mod){
            //For row_start, this is intended to be base*i
            //plus the number of previous, extra rows.
            if(i==0)
            {
              thread_data->col_start = 0;
            }
            else
            {
              thread_data->col_start = base*i+i;
            }
            thread_data->row_start = 0;
            thread_data->col_end = thread_data->col_start+base;
            thread_data->row_end = data->rows;
          }
          //Cases where you don't get an extra.
          else{
            thread_data->col_start = base*i+mod;
            thread_data->row_start = 0;
            thread_data->col_end = thread_data->col_start+base-1;
            thread_data->row_end = data->rows;
          }
        }
      }

      if(i == 0){
        thread_data->is_printing = 1;
      }

      pthread_create(&threads[i],NULL,worker,thread_data);

    }

    /* If we're doing graphics, call run_animation to tell it how many
     * iterations there will be. */
    if (output_mode == OUTPUT_VISI) {
        run_animation(handle,data->iters);
    }

    // TODO: join all the threads (that is, wait in this main thread until all
    // the workers are finished.

    for(int i = 0; i < num_threads; i++){
    pthread_join(threads[i], NULL);
  }

    if(gettimeofday(&time2,NULL)!=0){
        perror("Something went wrong with the time!");
        exit(1);}

      wholeTime = time2.tv_sec - time1.tv_sec;
      fractionalTime = (time2.tv_usec - time1.tv_usec);

      secs = wholeTime;
      secs+= fractionalTime/1000000.0;

    /* Print the total runtime, in seconds. */
    printf("\nTotal time: %0.3f seconds.\n", secs);
      pthread_barrier_destroy(&barrier);
        pthread_barrier_destroy(&barrier2);

        /**
        for(int i=0;i<num_threads;i++)
        {
          free(&threads[i]);
        }




      */
      pthread_mutex_destroy(&m);
      free(threads);
      free(master_board);
      free(data);


        return 0;
}
