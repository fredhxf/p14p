/*
 * PyMite - A flyweight Python interpreter for 8-bit microcontrollers and more.
 * Copyright 2002 Dean Hall
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#undef __FILE_ID__
#define __FILE_ID__ 0x14

/**
 * Sequence
 *
 * Functions that operate on sequences
 *
 * Log
 * ---
 *
 * 2006/11/29   #59: Improve bytecode UNPACK_SEQUENCE
 */

/***************************************************************
 * Includes
 **************************************************************/

#include "pm.h"


/***************************************************************
 * Functions
 **************************************************************/

/*
 * Compares two sequence objects
 * Assumes both objects are of same type (guaranteed by obj_compare)
 */
int8_t
seq_compare(pPmObj_t pobj1, pPmObj_t pobj2)
{
    int16_t l1;
    int16_t l2;
    pPmObj_t pa;
    pPmObj_t pb;
    PmReturn_t retval;
    int8_t retcompare;

    /* Get the lengths of supported types or return differ */
    if (OBJ_GET_TYPE(*pobj1) == OBJ_TYPE_TUP)
    {
        l1 = ((pPmTuple_t)pobj1)->length;
        l2 = ((pPmTuple_t)pobj2)->length;
    }
    else if (OBJ_GET_TYPE(*pobj1) == OBJ_TYPE_LST)
    {
        l1 = ((pPmList_t)pobj2)->length;
        l2 = ((pPmList_t)pobj2)->length;
    }
    else
    {
        return C_DIFFER;
    }

    /* Return if the lengths differ */
    if (l1 != l2)
    {
        return C_DIFFER;
    }

    /* Compare all items in the sequences */
    while (--l1 >= 0)
    {
        retval = seq_getSubscript(pobj1, l1, &pa);
        if (retval != PM_RET_OK)
        {
            return C_DIFFER;
        }
        retval = seq_getSubscript(pobj2, l1, &pb);
        if (retval != PM_RET_OK)
        {
            return C_DIFFER;
        }
        retcompare = obj_compare(pa, pb);
        if (retcompare != C_SAME)
        {
            return retcompare;
        }
    }

    return C_SAME;
}


/* Returns the length of the sequence */
PmReturn_t
seq_getLength(pPmObj_t pobj, int16_t *r_index)
{
    PmReturn_t retval = PM_RET_OK;

    switch (OBJ_GET_TYPE(*pobj))
    {
        case OBJ_TYPE_STR:
            *r_index = ((pPmString_t)pobj)->length;
            break;

        case OBJ_TYPE_TUP:
            *r_index = ((pPmTuple_t)pobj)->length;
            break;

        case OBJ_TYPE_LST:
            *r_index = ((pPmList_t)pobj)->length;
            break;

        default:
            /* Raise TypeError, non-sequence object */
            PM_RAISE(retval, PM_RET_EX_TYPE);
            break;
    }

    return retval;
}


/* Returns the object sequence[index] */
PmReturn_t
seq_getSubscript(pPmObj_t pobj, int16_t index, pPmObj_t *r_pobj)
{
    PmReturn_t retval;
    uint8_t c;

    switch (OBJ_GET_TYPE(*pobj))
    {
        case OBJ_TYPE_STR:
            /* Adjust for negative index */
            if (index < 0)
            {
                index += ((pPmString_t)pobj)->length;
            }

            /* Raise IndexError if index is out of bounds */
            if ((index < 0) || (index > ((pPmString_t)pobj)->length))
            {
                PM_RAISE(retval, PM_RET_EX_INDX);
                break;
            }

            /* Get the character from the string */
            c = ((pPmString_t)pobj)->val[index];

            /* Create a new string from the character */
            retval = string_newFromChar(c, r_pobj);
            break;

        case OBJ_TYPE_TUP:
            /* Get the tuple item */
            retval = tuple_getItem(pobj, index, r_pobj);
			OBJ_INC_REF(*r_pobj);
            break;

        case OBJ_TYPE_LST:
            /* Get the list item */
            retval = list_getItem(pobj, index, r_pobj);
			OBJ_INC_REF(*r_pobj);
            break;

        default:
            /* Raise TypeError, unsubscriptable object */
            PM_RAISE(retval, PM_RET_EX_TYPE);
            break;
    }

    return retval;
}


PmReturn_t
seqiter_getNext(pPmObj_t pobj, pPmObj_t *r_pitem)
{
    PmReturn_t retval;
    int16_t length;

    C_ASSERT(pobj != C_NULL);
    C_ASSERT(*r_pitem != C_NULL);
    C_ASSERT(OBJ_GET_TYPE(*pobj) == OBJ_TYPE_SQI);

    /*
     * Raise TypeError if sequence iterator's object is not a sequence
     * otherwise, the get sequence's length
     */
    retval = seq_getLength(((pPmSeqIter_t)pobj)->si_sequence, &length);
    PM_RETURN_IF_ERROR(retval);

    /* Raise StopIteration if at the end of the sequence */
    if (((pPmSeqIter_t)pobj)->si_index == length)
    {
        /* Make null the pointer to the sequence */
        ((pPmSeqIter_t)pobj)->si_sequence = C_NULL;
        PM_RAISE(retval, PM_RET_EX_STOP);
        return retval;
    }

    /* Get the item at the current index */
    retval = seq_getSubscript(((pPmSeqIter_t)pobj)->si_sequence,
                              ((pPmSeqIter_t)pobj)->si_index, r_pitem);

    /* Increment the index */
    ((pPmSeqIter_t)pobj)->si_index++;

    return retval;
}


PmReturn_t
seqiter_new(pPmObj_t pobj, pPmObj_t *r_pobj)
{
    PmReturn_t retval;
    uint8_t *pchunk;
    pPmSeqIter_t psi;

    C_ASSERT(pobj != C_NULL);
    C_ASSERT(*r_pobj != C_NULL);

    /* Raise a TypeError if pobj is not a sequence */
    if ((OBJ_GET_TYPE(*pobj) != OBJ_TYPE_STR)
        && (OBJ_GET_TYPE(*pobj) != OBJ_TYPE_TUP)
        && (OBJ_GET_TYPE(*pobj) != OBJ_TYPE_LST))
    {
        PM_RAISE(retval, PM_RET_EX_TYPE);
        return retval;
    }

    /* Alloc a chunk for the sequence iterator obj */
    retval = heap_getChunk(sizeof(PmSeqIter_t), &pchunk);
    PM_RETURN_IF_ERROR(retval);

    /* Set the sequence iterator's fields */
    psi = (pPmSeqIter_t)pchunk;
    OBJ_SET_TYPE(*psi, OBJ_TYPE_SQI);
    psi->si_sequence = pobj;
	OBJ_INC_REF(pobj);
    psi->si_index = 0;

    *r_pobj = (pPmObj_t)psi;
    return retval;
}
