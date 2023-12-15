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
  pthread_mutex_lock(&event_list->event_list_lock);
  if (event_list == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (get_event_with_delay(event_id) != NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "Event already exists\n");
    return 1;
  }

  struct Event* event = malloc(sizeof(struct Event));

  if (event == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "Error allocating memory for event\n");
    return 1;
  }

  pthread_mutex_init(&event->event_lock, NULL);

  event->id = event_id;
  event->rows = num_rows;
  event->cols = num_cols;
  event->reservations = 0;
  event->data = malloc(num_rows * num_cols * sizeof(struct Seat));

  if (event->data == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "Error allocating memory for event data\n");
    free(event);
    return 1;
  }

  for (size_t i = 0; i < num_rows * num_cols; i++) {
    event->data[i].reservation_id = 0;
  }

  if (append_to_list(event_list, event) != 0) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "Error appending event to list\n");
    free(event->data);
    free(event);
    return 1;
  }
  pthread_mutex_unlock(&event_list->event_list_lock);

  return 0;
}

int ems_reserve(unsigned int event_id, size_t num_seats, size_t* xs, size_t* ys) {
  pthread_mutex_lock(&event_list->event_list_lock);
  if (event_list == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }
  struct Event* event = get_event_with_delay(event_id);
  pthread_mutex_unlock(&event_list->event_list_lock);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }
  
  
  if(sort(xs, ys, num_seats) < 0){
    fprintf(stderr, "Invalid reservation\n");
    return 1;
  }
  
  // Check if reservation is successful
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
    
    pthread_mutex_lock(&seat->seat_lock);
    if (seat->reservation_id != 0) {
      fprintf(stderr, "Seat already reserved\n");
      break;
    }
  }

  // If the reservation was not successful, unlock seats
  if (i < num_seats) {
    for (size_t j = 0; j <= i; j++) {
      pthread_mutex_unlock(&(get_seat_with_delay(event, seat_index(event, xs[j], ys[j]))->seat_lock));
    }
    return 1;
  }

  // If the reservation was successful, change reservation ID and unlock seats
  pthread_mutex_lock(&event->event_lock);
  unsigned int reservation_id = ++event->reservations;
  pthread_mutex_unlock(&event->event_lock);

  for (size_t j = 0; j < num_seats; j++) {
    struct Seat *seat = get_seat_with_delay(event, seat_index(event, xs[j], ys[j]));

    seat->reservation_id = reservation_id;
    pthread_mutex_unlock(&seat->seat_lock);
  }

  return 0;
}

int ems_show(unsigned int event_id, int fdout) {
  pthread_mutex_lock(&event_list->event_list_lock);
  if (event_list == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  struct Event* event = get_event_with_delay(event_id);
  pthread_mutex_unlock(&event_list->event_list_lock);

  if (event == NULL) {
    fprintf(stderr, "Event not found\n");
    return 1;
  }

  char buffer[32];

  pthread_mutex_lock(&write_lock);
  for (size_t i = 1; i <= event->rows; i++) {
    for (size_t j = 1; j <= event->cols; j++) {
      struct Seat* seat = get_seat_with_delay(event, seat_index(event, i, j));

      pthread_mutex_lock(&seat->seat_lock);

      sprintf(buffer, "%u", seat->reservation_id);
      
      if(write_to_file(fdout, buffer)){
        pthread_mutex_unlock(&seat->seat_lock);
        fprintf(stderr, "Error while writing to file.\n");
        return 1;
      }

      if (j < event->cols) {
        if(write_to_file(fdout, " ")){
          pthread_mutex_unlock(&seat->seat_lock);
          fprintf(stderr, "Error while writing to file.\n");
          return 1;
        }
      }

      pthread_mutex_unlock(&seat->seat_lock);
    }

    if(write_to_file(fdout, "\n")){
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }
  }
  pthread_mutex_unlock(&write_lock);
  return 0;
}

int ems_list_events(int fdout) {
  pthread_mutex_lock(&event_list->event_list_lock);
  if (event_list == NULL) {
    pthread_mutex_unlock(&event_list->event_list_lock);
    fprintf(stderr, "EMS state must be initialized\n");
    return 1;
  }

  if (event_list->head == NULL) {
    pthread_mutex_lock(&write_lock);
    if(write_to_file(fdout, "No events\n")){
      pthread_mutex_unlock(&event_list->event_list_lock);
      pthread_mutex_unlock(&write_lock);
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }
    pthread_mutex_unlock(&event_list->event_list_lock);
    pthread_mutex_unlock(&write_lock);
    return 0;
  }

  char buffer[16];

  pthread_mutex_lock(&write_lock);
  struct ListNode* current = event_list->head;
  while (current != NULL) {
    if(write_to_file(fdout, "Event: ")){
      pthread_mutex_unlock(&event_list->event_list_lock);
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    sprintf(buffer, "%u", (current->event)->id);

    if(write_to_file(fdout, buffer)){
      pthread_mutex_unlock(&event_list->event_list_lock);
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    if(write_to_file(fdout, "\n")){
      pthread_mutex_unlock(&event_list->event_list_lock);
      fprintf(stderr, "Error while writing to file.\n");
      return 1;
    }

    current = current->next;
  }
  pthread_mutex_unlock(&event_list->event_list_lock);
  pthread_mutex_unlock(&write_lock);

  return 0;
}

void ems_wait(unsigned int delay_ms) {
  struct timespec delay = delay_to_timespec(delay_ms);
  nanosleep(&delay, NULL);
}
