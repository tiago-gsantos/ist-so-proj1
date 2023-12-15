#ifndef EMS_SORT_H
#define EMS_SORT_H

#include <stddef.h>

void exchange(size_t i, size_t j, size_t *xs, size_t *ys);

int is_greater_than_next(size_t i, size_t *xs, size_t *ys);

int sort(size_t *xs, size_t *ys, size_t size);

#endif  // EMS_SORT_H