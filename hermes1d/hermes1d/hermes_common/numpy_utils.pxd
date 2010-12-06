from numpy cimport ndarray

cdef ndarray c2numpy_int(int *A, int len)
cdef ndarray c2numpy_double(double *A, int len)
cdef ndarray c2numpy_int_inplace(int *A, int len)
cdef ndarray c2numpy_double_inplace(double *A, int len)