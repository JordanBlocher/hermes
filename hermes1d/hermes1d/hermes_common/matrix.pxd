from cpp cimport matrix

cdef class Matrix:
    cdef matrix.Matrix *thisptr

cdef class SparseMatrix(Matrix):

    cdef matrix.SparseMatrix* as_SparseMatrix(self)

cdef class CSCMatrix(SparseMatrix):

    cdef matrix.UMFPackMatrix* as_UMFPackMatrix(self)

cdef class Vector:
    cdef matrix.Vector *thisptr

cdef class AVector(Vector):
    pass