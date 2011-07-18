#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"

#define NPY_NO_DEPRECATED_API
#define _MULTIARRAYMODULE
#define NPY_NO_PREFIX
#include "numpy/arrayobject.h"
#include "numpy/arrayscalars.h"

#include "npy_config.h"

#include "numpy/npy_3kcompat.h"

#include "common.h"
#include "mapping.h"

#include "sequence.h"

static int
array_any_nonzero(PyArrayObject *mp);

/*************************************************************************
 ****************   Implement Sequence Protocol **************************
 *************************************************************************/

/* Some of this is repeated in the array_as_mapping protocol.  But
   we fill it in here so that PySequence_XXXX calls work as expected
*/


static PyObject *
array_slice(PyArrayObject *self, Py_ssize_t ilow, Py_ssize_t ihigh)
{
    PyArrayObject *ret;
    PyArray_Descr *dtype;
    Py_ssize_t dim0;
    char *data;
    npy_intp shape[NPY_MAXDIMS];

    if (PyArray_NDIM(self) == 0) {
        PyErr_SetString(PyExc_ValueError, "cannot slice a 0-d array");
        return NULL;
    }

    dim0 = PyArray_DIM(self, 0);
    if (ilow < 0) {
        ilow = 0;
    }
    else if (ilow > dim0) {
        ilow = dim0;
    }
    if (ihigh < ilow) {
        ihigh = ilow;
    }
    else if (ihigh > dim0) {
        ihigh = dim0;
    }

    data = PyArray_DATA(self);
    if (ilow < ihigh) {
        data += ilow * PyArray_STRIDE(self, 0);
    }

    /* Same shape except dimension 0 */
    shape[0] = ihigh - ilow;
    memcpy(shape+1, PyArray_DIMS(self) + 1,
                        (PyArray_NDIM(self)-1)*sizeof(npy_intp));

    dtype = PyArray_DESCR(self);
    Py_INCREF(dtype);
    ret = (PyArrayObject *)PyArray_NewFromDescr(Py_TYPE(self), dtype,
                             PyArray_NDIM(self), shape,
                             PyArray_STRIDES(self), data,
                             PyArray_FLAGS(self), (PyObject *)self);
    if (ret == NULL) {
        return NULL;
    }
    Py_INCREF(self);
    if (PyArray_SetBase(ret, (PyObject *)self) < 0) {
        Py_DECREF(ret);
        return NULL;
    }
    PyArray_UpdateFlags(ret, NPY_ARRAY_UPDATE_ALL);
    return (PyObject *)ret;
}


static int
array_ass_slice(PyArrayObject *self, Py_ssize_t ilow,
                Py_ssize_t ihigh, PyObject *v) {
    int ret;
    PyArrayObject *tmp;

    if (v == NULL) {
        PyErr_SetString(PyExc_ValueError,
                        "cannot delete array elements");
        return -1;
    }
    if (!PyArray_ISWRITEABLE(self)) {
        PyErr_SetString(PyExc_RuntimeError,
                        "array is not writeable");
        return -1;
    }
    if ((tmp = (PyArrayObject *)array_slice(self, ilow, ihigh)) == NULL) {
        return -1;
    }
    ret = PyArray_CopyObject(tmp, v);
    Py_DECREF(tmp);

    return ret;
}

static int
array_contains(PyArrayObject *self, PyObject *el)
{
    /* equivalent to (self == el).any() */

    PyObject *res;
    int ret;

    res = PyArray_EnsureAnyArray(PyObject_RichCompare((PyObject *)self,
                                                      el, Py_EQ));
    if (res == NULL) {
        return -1;
    }
    ret = array_any_nonzero((PyArrayObject *)res);
    Py_DECREF(res);
    return ret;
}

NPY_NO_EXPORT PySequenceMethods array_as_sequence = {
#if PY_VERSION_HEX >= 0x02050000
    (lenfunc)array_length,                  /*sq_length*/
    (binaryfunc)NULL,                       /*sq_concat is handled by nb_add*/
    (ssizeargfunc)NULL,
    (ssizeargfunc)array_item_nice,
    (ssizessizeargfunc)array_slice,
    (ssizeobjargproc)array_ass_item,        /*sq_ass_item*/
    (ssizessizeobjargproc)array_ass_slice,  /*sq_ass_slice*/
    (objobjproc) array_contains,            /*sq_contains */
    (binaryfunc) NULL,                      /*sg_inplace_concat */
    (ssizeargfunc)NULL,
#else
    (inquiry)array_length,                  /*sq_length*/
    (binaryfunc)NULL,                       /*sq_concat is handled by nb_add*/
    (intargfunc)NULL,                       /*sq_repeat is handled nb_multiply*/
    (intargfunc)array_item_nice,            /*sq_item*/
    (intintargfunc)array_slice,             /*sq_slice*/
    (intobjargproc)array_ass_item,          /*sq_ass_item*/
    (intintobjargproc)array_ass_slice,      /*sq_ass_slice*/
    (objobjproc) array_contains,            /*sq_contains */
    (binaryfunc) NULL,                      /*sg_inplace_concat */
    (intargfunc) NULL                       /*sg_inplace_repeat */
#endif
};


/****************** End of Sequence Protocol ****************************/

/*
 * Helpers
 */

/* Array evaluates as "TRUE" if any of the elements are non-zero*/
static int
array_any_nonzero(PyArrayObject *arr)
{
    intp index;
    PyArrayIterObject *it;
    Bool anyTRUE = FALSE;

    it = (PyArrayIterObject *)PyArray_IterNew((PyObject *)arr);
    if (it == NULL) {
        return anyTRUE;
    }
    index = it->size;
    while(index--) {
        if (PyArray_DESCR(arr)->f->nonzero(it->dataptr, arr)) {
            anyTRUE = TRUE;
            break;
        }
        PyArray_ITER_NEXT(it);
    }
    Py_DECREF(it);
    return anyTRUE;
}
