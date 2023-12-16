#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "eventlist.h"
#include "filehandler.h"
#include "sort.h"

static struct EventList* event_list = NULL;
static unsigned int state_access_delay_ms = 0;

pthread_mutex_t write_lock = PTHREAD_MUTEX_INITIALIZER;

/// Calculates a timespec from a delay in milliseconds.
/// @param delay_ms Delay in milliseconds.
/// @return Timespec with the given delay.
static struct timespec delay_to_timespec(unsigned int delay_ms) {
  return (struct timespec){delay_ms / 1000, (delay_ms % 1000) * 1000000};
}

/// Gets the event with the given ID from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event_id The ID of the event to get.
/// @return Pointer to the event if found, NULL otherwise.
static struct Event* get_event_with_delay(unsigned int event_id) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return get_event(event_list, event_id);
}

/// Gets the seat with the given index from the state.
/// @note Will wait to simulate a real system accessing a costly memory resource.
/// @param event Event to get the seat from.
/// @param index Index of the seat to get.
/// @return Pointer to the seat.
static struct Seat* get_seat_with_delay(struct Event* event, size_t index) {
  struct timespec delay = delay_to_timespec(state_access_delay_ms);
  nanosleep(&delay, NULL);  // Should not be removed

  return &event->data[index];
}

/// Gets the index of a seat.
/// @note This function assumes that the seat exists.
/// @param event Event to get the seat index from.
/// @param row Row of the seat.
/// @param col Column of the seat.
/// @return Index of the seat.
static size_t seat_index(struct Event* event, size_t row, size_t col) { return (row - 1) * event->cols + col - 1; }

int ems_init(unsigned int delay_ms) {
  if (event_list != NULL) {
    fprintf(stderr, "EMS state has already been initialized\n");
    return 1;
  }

  event_list = create_list();
  state_access_delay_ms = delay_ms;

  return event_list == NULL;
}

int ems_terminate() {
  if (event_list == NULL) {
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  free_list(event_list);
  return 0;
}

int ems_create(unsigned int event_id, size_t num_rows, size_t num_cols) {
  if(pthread_mutex_lock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }

  if (event_list == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to lock mutex\n");
      exit(1);
    }
    
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }

    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  // Initialize event
  if(pthread_mutex_init(&event->event_lock, NULL) != 0){
    fprintf(stderr, "Failed to initialize mutex\n");
    exit(1);
  }

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(struct Seat));

  if (event->data == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    free(event);
    
    fprintf(stderr, "Error allocating memory for event data\n");
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i].reservation_id = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    free(event->data);
    free(event);
    
    fprintf(stderr, "Error appending event to list\n");
    return 1;
  }
  
  if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  if(pthread_mutex_lock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }
  
  if (event_list == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);
  
  if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  // Sort reservation seats
  if(sort(xs, ys, num_seats) < 0){
    fprintf(stderr, "Invalid reservation\n");
    return 1;
  }
  
  // Check if reservation is successful, locking each seat
  size_t i = 0;
  for (; i < num_seats; i++) {
    size_t row = xs[i];
    size_t col = ys[i];

    if (row <= 0 || row > event->rows || col <= 0 || col > event->cols) {
      fprintf(stderr, "Invalid seat\n");
      i--;
      break;
    }

    struct Seat *seat = get_seat_with_delay(event, seat_index(event, row, col));
    
    if(pthread_mutex_lock(&seat->seat_lock) != 0){
      fprintf(stderr, "Failed to lock mutex\n");
      exit(1);
    }

    // If it's already reserved break
    if (seat->reservation_id != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }
  }

  // First seat is invalid
  if((int) i < 0) return 1;

  // If one of the seats is invalid or already reserved, unlock previous seats
  if (i < num_seats) {
    for (size_t j = 0; j <= i; j++) {
      if(pthread_mutex_unlock(&(get_seat_with_delay(event, seat_index(event, xs[j], ys[j]))->seat_lock)) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
    }
    return 1;
  }

  // If all seats are valid, change number of reservations of the event...
  if(pthread_mutex_lock(&event->event_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }

  unsigned int reservation_id = ++event->reservations;
  
  if(pthread_mutex_unlock(&event->event_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }

  for (size_t j = 0; j < num_seats; j++) {
    struct Seat *seat = get_seat_with_delay(event, seat_index(event, xs[j], ys[j]));
    
    // ... change the reservation ID of each seat ...
    seat->reservation_id = reservation_id;
    
    // ... and unlock it
    if(pthread_mutex_unlock(&seat->seat_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
  }

  return 0;
}

int ems_show(unsigned int event_id, int fdout) {
  if(pthread_mutex_lock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }
  
  if (event_list == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);
  
  if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  char buffer[32];

  if(pthread_mutex_lock(&write_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }
  
  // Lock each seat, print it's reservation ID and unlock it
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      struct Seat* seat = get_seat_with_delay(event, seat_index(event, i, j));

      if(pthread_mutex_lock(&seat->seat_lock) != 0){
        fprintf(stderr, "Failed to lock mutex\n");
        exit(1);
      }

      sprintf(buffer, "%u", seat->reservation_id);
      
      if(write_to_file(fdout, buffer)){
        if(pthread_mutex_unlock(&seat->seat_lock) != 0){
          fprintf(stderr, "Failed to unlock mutex\n");
          exit(1);
        }
        
        fprintf(stderr, "Error while writing to file.\n");
        return 1;
      }

      if (j < event->cols) {
        if(write_to_file(fdout, " ")){
          if(pthread_mutex_unlock(&seat->seat_lock) != 0){
            fprintf(stderr, "Failed to unlock mutex\n");
            exit(1);
          }
          
          fprintf(stderr, "Error while writing to file.\n");
          return 1;
        }
      }

      if(pthread_mutex_unlock(&seat->seat_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
    }
    
    if(write_to_file(fdout, "\n")){
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }
  }
  
  if(pthread_mutex_unlock(&write_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }
  
  return 0;
}

int ems_list_events(int fdout) {
  if(pthread_mutex_lock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }
  
  if (event_list == NULL) {
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (event_list->head == NULL) {
    if(pthread_mutex_lock(&write_lock) != 0){
      fprintf(stderr, "Failed to lock mutex\n");
      exit(1);
    }
    
    if(write_to_file(fdout, "No events\n")){
      if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
      
      if(pthread_mutex_unlock(&write_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
      
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }
    
    if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    if(pthread_mutex_unlock(&write_lock) != 0){
      fprintf(stderr, "Failed to unlock mutex\n");
      exit(1);
    }
    
    return 0;
  }

  char buffer[16];

  if(pthread_mutex_lock(&write_lock) != 0){
    fprintf(stderr, "Failed to lock mutex\n");
    exit(1);
  }
  
  struct ListNode* current = event_list->head;
  
  while (current != NULL) {
    if(write_to_file(fdout, "Event: ")){
      if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }

      if(pthread_mutex_unlock(&write_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
      
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    sprintf(buffer, "%u", (current->event)->id);

    if(write_to_file(fdout, buffer)){
      if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }

      if(pthread_mutex_unlock(&write_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
      
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    if(write_to_file(fdout, "\n")){
      if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }

      if(pthread_mutex_unlock(&write_lock) != 0){
        fprintf(stderr, "Failed to unlock mutex\n");
        exit(1);
      }
      
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    current = current->next;
  }
  
  if(pthread_mutex_unlock(&event_list->event_list_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }
  
  if(pthread_mutex_unlock(&write_lock) != 0){
    fprintf(stderr, "Failed to unlock mutex\n");
    exit(1);
  }

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
