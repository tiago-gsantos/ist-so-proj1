#ifndef EMS_SORT_H
#define EMS_SORT_H

#include <stddef.h>

/// This function swaps the elements at positions i and j in the arrays xs and ys
/// @param i Index of the first element to be exchanged
/// @param j Index of the second element to be exchanged
/// @param xs Array of values used for primary sorting
/// @param ys Array of values used for secondary sorting in case of ties
void exchange(size_t i, size_t j, size_t *xs, size_t *ys);

/// This function compares the element at position i with the next element (at position i + 1)
/// in the arrays xs and ys.
/// @param i Index of the current element to be compared.
/// @param xs Array of values used for primary sorting
/// @param ys Array of values used for secondary sorting in case of ties
/// @return TRUE if the element at position i is greater than the next element, FALSE otherwise
/// and -1 if theres a tie between the elements at positions i and i + 1
int is_greater_than_next(size_t i, size_t *xs, size_t *ys);

/// This function implements a sorting algorithm that uses the is_greater_than_next function
/// to compare and sort elements in the arrays xs and ys. The sorting is performed in ascending order.
/// @param xs Array of values used for primary sorting
/// @param ys Array of values used for secondary sorting in case of ties
/// @param size number of elements in the arrays to be sorted.
/// @return 0 indicating successful sorting and -1 if theres a duplicated element
int sort(size_t *xs, size_t *ys, size_t size);

#endif  // EMS_SORT_H