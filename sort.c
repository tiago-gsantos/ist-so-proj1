#include "sort.h"
#include <stdio.h>

#define FALSE (0)
#define TRUE (1)

void exchange(size_t i, size_t j, size_t *xs, size_t *ys) {
    size_t temp_x = xs[i];
    size_t temp_y = ys[i];

    xs[i] = xs[j];
    ys[i] = ys[j];

    xs[j] = temp_x;
    ys[j] = temp_y;
}

int is_greater_than_next(size_t i, size_t *xs, size_t *ys){
  if(xs[i] > xs[i + 1])
    return TRUE;
  else if(xs[i] == xs[i + 1]){
    if(ys[i] > ys[i + 1])
      return TRUE;
    else if(ys[i] == ys[i + 1])
      return -1;
    return FALSE;
  }
  return FALSE;
}

int sort(size_t *xs, size_t *ys, size_t size){
  int exchanged;
  for(size_t i = 0; i < size - 1; i++){
    exchanged = FALSE;
    for(size_t j = 0; j < size - i - 1; j++){
      int compare_result = is_greater_than_next(j, xs, ys);
      if(compare_result == TRUE){
        exchange(j, j + 1, xs, ys);
        exchanged = TRUE;
      }
      else if(compare_result == -1){
        return -1;
      }
    }
    if(exchanged == FALSE) return 0;
  }
  return 0;
}