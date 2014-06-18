/*
 * Copyright 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ANDROID_PIXELFLINGER_VECTOR_IMPL_H
#define ANDROID_PIXELFLINGER_VECTOR_IMPL_H

#include <assert.h>
#include <stdint.h>
#include <sys/types.h>

// ---------------------------------------------------------------------------
// No user serviceable parts in here...
// ---------------------------------------------------------------------------

namespace stagefright {
namespace tinyutils {

/*!
 * Implementation of the guts of the vector<> class
 * this ensures backward binary compatibility and
 * reduces code size.
 * For performance reasons, we expose mStorage and mCount
 * so these fields are set in stone.
 *
 */

class VectorImpl
{
public:
    enum { // flags passed to the ctor
        HAS_TRIVIAL_CTOR    = 0x00000001,
        HAS_TRIVIAL_DTOR    = 0x00000002,
        HAS_TRIVIAL_COPY    = 0x00000004,
        HAS_TRIVIAL_ASSIGN  = 0x00000008
    };

                            VectorImpl(size_t itemSize, uint32_t flags);
                            VectorImpl(const VectorImpl& rhs);
    virtual                 ~VectorImpl();

    /*! must be called from subclasses destructor */
            void            finish_vector();

            VectorImpl&     operator = (const VectorImpl& rhs);    
            
    /*! C-style array access */
    inline  const void*     arrayImpl() const       { return mStorage; }
            void*           editArrayImpl();
            
    /*! vector stats */
    inline  size_t          size() const        { return mCount; }
    inline  bool            isEmpty() const     { return mCount == 0; }
            size_t          capacity() const;
            ssize_t         setCapacity(size_t size);

            /*! append/insert another vector */
            ssize_t         insertVectorAt(const VectorImpl& vector, size_t index);
            ssize_t         appendVector(const VectorImpl& vector);
            
            /*! add/insert/replace items */
            ssize_t         insertAt(size_t where, size_t numItems = 1);
            ssize_t         insertAt(const void* item, size_t where, size_t numItems = 1);
            void            pop();
            void            push();
            void            push(const void* item);
            ssize_t         add();
            ssize_t         add(const void* item);
            ssize_t         replaceAt(size_t index);
            ssize_t         replaceAt(const void* item, size_t index);

            /*! remove items */
            ssize_t         removeItemsAt(size_t index, size_t count = 1);
            void            clear();

            const void*     itemLocation(size_t index) const;
            void*           editItemLocation(size_t index);

protected:
            size_t          itemSize() const;
            void            release_storage();

    virtual void            do_construct(void* storage, size_t num) const = 0;
    virtual void            do_destroy(void* storage, size_t num) const = 0;
    virtual void            do_copy(void* dest, const void* from, size_t num) const = 0;
    virtual void            do_splat(void* dest, const void* item, size_t num) const = 0;
    virtual void            do_move_forward(void* dest, const void* from, size_t num) const = 0;
    virtual void            do_move_backward(void* dest, const void* from, size_t num) const = 0;

    // take care of FBC...
    virtual void            reservedVectorImpl1();
    virtual void            reservedVectorImpl2();
    virtual void            reservedVectorImpl3();
    virtual void            reservedVectorImpl4();
    virtual void            reservedVectorImpl5();
    virtual void            reservedVectorImpl6();
    virtual void            reservedVectorImpl7();
    virtual void            reservedVectorImpl8();
    
private:
        void* _grow(size_t where, size_t amount);
        void  _shrink(size_t where, size_t amount);

        inline void _do_construct(void* storage, size_t num) const;
        inline void _do_destroy(void* storage, size_t num) const;
        inline void _do_copy(void* dest, const void* from, size_t num) const;
        inline void _do_splat(void* dest, const void* item, size_t num) const;
        inline void _do_move_forward(void* dest, const void* from, size_t num) const;
        inline void _do_move_backward(void* dest, const void* from, size_t num) const;

            // These 2 fields are exposed in the inlines below,
            // so they're set in stone.
            void *      mStorage;   // base address of the vector
            size_t      mCount;     // number of items

    const   uint32_t    mFlags;
    const   size_t      mItemSize;
};



class SortedVectorImpl : public VectorImpl
{
public:
                            SortedVectorImpl(size_t itemSize, uint32_t flags);
                            SortedVectorImpl(const VectorImpl& rhs);
    virtual                 ~SortedVectorImpl();
    
    SortedVectorImpl&     operator = (const SortedVectorImpl& rhs);    

    //! finds the index of an item
            ssize_t         indexOf(const void* item) const;

    //! finds where this item should be inserted
            size_t          orderOf(const void* item) const;

    //! add an item in the right place (or replaces it if there is one)
            ssize_t         add(const void* item);

    //! merges a vector into this one
            ssize_t         merge(const VectorImpl& vector);
            ssize_t         merge(const SortedVectorImpl& vector);
             
    //! removes an item
            ssize_t         remove(const void* item);
        
protected:
    virtual int             do_compare(const void* lhs, const void* rhs) const = 0;

    // take care of FBC...
    virtual void            reservedSortedVectorImpl1();
    virtual void            reservedSortedVectorImpl2();
    virtual void            reservedSortedVectorImpl3();
    virtual void            reservedSortedVectorImpl4();
    virtual void            reservedSortedVectorImpl5();
    virtual void            reservedSortedVectorImpl6();
    virtual void            reservedSortedVectorImpl7();
    virtual void            reservedSortedVectorImpl8();

private:
            ssize_t         _indexOrderOf(const void* item, size_t* order = 0) const;

            // these are made private, because they can't be used on a SortedVector
            // (they don't have an implementation either)
            ssize_t         add();
            void            pop();
            void            push();
            void            push(const void* item);
            ssize_t         insertVectorAt(const VectorImpl& vector, size_t index);
            ssize_t         appendVector(const VectorImpl& vector);
            ssize_t         insertAt(size_t where, size_t numItems = 1);
            ssize_t         insertAt(const void* item, size_t where, size_t numItems = 1);
            ssize_t         replaceAt(size_t index);
            ssize_t         replaceAt(const void* item, size_t index);
};

} // namespace tinyutils
} // namespace stagefright


// ---------------------------------------------------------------------------

#endif // ANDROID_PIXELFLINGER_VECTOR_IMPL_H
