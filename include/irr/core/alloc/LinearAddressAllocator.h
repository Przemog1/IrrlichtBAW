// Copyright (C) 2018 Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW Engine"
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_LINEAR_ADDRESS_ALLOCATOR_H_INCLUDED__
#define __IRR_LINEAR_ADDRESS_ALLOCATOR_H_INCLUDED__

#include "IrrCompileConfig.h"

#include "irr/core/alloc/AddressAllocatorBase.h"

namespace irr
{
namespace core
{


template<typename _size_type>
class LinearAddressAllocator : public AddressAllocatorBase<LinearAddressAllocator<_size_type>,_size_type>
{
        typedef AddressAllocatorBase<LinearAddressAllocator<_size_type>,_size_type> Base;
    public:
        _IRR_DECLARE_ADDRESS_ALLOCATOR_TYPEDEFS(_size_type);

        static constexpr bool supportsNullBuffer = true;

        #define DUMMY_DEFAULT_CONSTRUCTOR LinearAddressAllocator() : bufferSize(invalid_address), cursor(invalid_address) {}
        GCC_CONSTRUCTOR_INHERITANCE_BUG_WORKAROUND(DUMMY_DEFAULT_CONSTRUCTOR)
        #undef DUMMY_DEFAULT_CONSTRUCTOR

        virtual ~LinearAddressAllocator() {}

        LinearAddressAllocator(void* reservedSpc, _size_type addressOffsetToApply, _size_type alignOffsetNeeded, _size_type maxAllocatableAlignment, size_type bufSz) noexcept :
                    Base(reservedSpc,addressOffsetToApply,alignOffsetNeeded,maxAllocatableAlignment), bufferSize(bufSz-Base::alignOffset)
        {
            reset();
        }

        //! When resizing we require that the copying of data buffer has already been handled by the user of the address allocator even if `supportsNullBuffer==true`
        template<typename... Args>
        LinearAddressAllocator(_size_type newBuffSz, LinearAddressAllocator&& other, Args&&... args) :
            Base(std::move(other),std::forward<Args>(args)...), bufferSize(invalid_address), cursor(invalid_address)
        {
            std::swap(bufferSize,other.bufferSize);
            bufferSize = newBuffSz-Base::alignOffset;
            std::swap(cursor,other.cursor);
        }

        LinearAddressAllocator& operator=(LinearAddressAllocator&& other)
        {
            Base::operator=(std::move(other));
            std::swap(bufferSize,other.bufferSize);
            std::swap(cursor,other.cursor);
            return *this;
        }

        //! non-PoT alignments cannot be guaranteed after a resize or move of the backing buffer
        inline size_type    alloc_addr( size_type bytes, size_type alignment, size_type hint=0ull) noexcept
        {
            if (bytes==0 || alignment>Base::maxRequestableAlignment)
                return invalid_address;

            size_type result = core::roundUp(cursor,alignment);
            size_type newCursor = result+bytes;
            if (newCursor>bufferSize || newCursor<cursor) // the extra OR checks for wraparound
                return invalid_address;

            cursor = newCursor;
            return result+Base::combinedOffset;
        }

        // free is a No-OP, only reset can actually reclaim memory
        inline void         free_addr(size_type addr, size_type bytes) noexcept
        {
            return;
        }

        // free all allocations
        inline void         reset()
        {
            cursor = 0u;
        }

        //! Conservative estimate, max_size() gives largest size we are sure to be able to allocate
        inline size_type    max_size() const noexcept
        {
            auto worstCursor = roundUp(cursor,Base::maxRequestableAlignment);
            if (worstCursor<bufferSize)
                return bufferSize-worstCursor;
            return 0u;
        }

        //! Most allocators do not support e.g. 1-byte allocations
        inline size_type    min_size() const noexcept
        {
            return 1u;
        }

        inline size_type        safe_shrink_size(size_type sizeBound, size_type newBuffAlignmentWeCanGuarantee=1u) const noexcept
        {
            size_type retval = get_allocated_size();
            return Base::safe_shrink_size(std::max(retval,sizeBound),newBuffAlignmentWeCanGuarantee);
        }

        template<typename... Args>
        static inline size_type reserved_size(const Args&... args) noexcept
        {
            return 0u;
        }

        // total allocatable size, align offset is not allocatable so it's not included
        inline size_type        get_free_size() const noexcept
        {
            return bufferSize-get_allocated_size();
        }
        // the align offset doesn't count as allocated, its just unusable
        inline size_type        get_allocated_size() const noexcept
        {
            return cursor;
        }
        // total size is the metric that includes the align offset
        inline size_type        get_total_size() const noexcept
        {
            return bufferSize+Base::alignOffset;
        }
    protected:
        size_type bufferSize;
        size_type       cursor;
};


}
}

#include "irr/core/alloc/AddressAllocatorConcurrencyAdaptors.h"
namespace irr
{
namespace core
{


// aliases
template<typename size_type>
using LinearAddressAllocatorST = LinearAddressAllocator<size_type>;

template<typename size_type, class RecursiveLockable>
using LinearAddressAllocatorMT = AddressAllocatorBasicConcurrencyAdaptor<LinearAddressAllocator<size_type>,RecursiveLockable>;

}
}

#endif // __IRR_LINEAR_ADDRESS_ALLOCATOR_H_INCLUDED__
