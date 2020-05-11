#ifndef __IRR_C_DOUBLE_BUFFERING_ALLOCATOR_H__
#define __IRR_C_DOUBLE_BUFFERING_ALLOCATOR_H__

namespace irr
{
namespace core
{

template<class AddressAllocator, bool onlySwapRangesMarkedDirty>
class MultiBufferingAllocatorBase;

//! Specializations for marking dirty areas for swapping
template<class AddressAllocator>
class MultiBufferingAllocatorBase<AddressAllocator,false>
{
    protected:
        std::pair<typename AddressAllocator::size_type,typename AddressAllocator::size_type> pushRange;
        std::pair<typename AddressAllocator::size_type,typename AddressAllocator::size_type> pullRange;

        MultiBufferingAllocatorBase() : pushRange(0u,~static_cast<typename AddressAllocator::size_type>(0u)),pullRange(0u,~static_cast<typename AddressAllocator::size_type>(0u)) {}
        virtual ~MultiBufferingAllocatorBase() {}

        inline void resetPushRange() {}
        inline void resetPullRange() {}

        _IRR_STATIC_INLINE_CONSTEXPR bool alwaysSwapEntireRange = true;
    public:
        inline void markRangeForPush(typename AddressAllocator::size_type begin, typename AddressAllocator::size_type end)
        {
        }
        inline void markRangeForPull(typename AddressAllocator::size_type begin, typename AddressAllocator::size_type end)
        {
        }
};
template<class AddressAllocator>
class MultiBufferingAllocatorBase<AddressAllocator,true>
{
    protected:
        std::pair<typename AddressAllocator::size_type,typename AddressAllocator::size_type> pushRange;
        std::pair<typename AddressAllocator::size_type,typename AddressAllocator::size_type> pullRange;

        MultiBufferingAllocatorBase() {resetPushRange(); resetPullRange();}
        virtual ~MultiBufferingAllocatorBase() {}

        inline void resetPushRange() {pushRange.first = ~static_cast<typename AddressAllocator::size_type>(0u); pushRange.second = 0u;}
        inline void resetPullRange() {pullRange.first = ~static_cast<typename AddressAllocator::size_type>(0u); pullRange.second = 0u;}

        _IRR_STATIC_INLINE_CONSTEXPR bool alwaysSwapEntireRange = false;
    public:
        inline decltype(pushRange) getPushRange() const {return pushRange;}
        inline decltype(pullRange) getPullRange() const {return pullRange;}

        inline void markRangeForPush(typename AddressAllocator::size_type begin, typename AddressAllocator::size_type end)
        {
            if (begin<pushRange.first) pushRange.first = begin;
            if (end>pushRange.second) pushRange.second = end;
        }
        inline void markRangeForPull(typename AddressAllocator::size_type begin, typename AddressAllocator::size_type end)
        {
            if (begin<pushRange.first) pushRange.first = begin;
            if (end>pushRange.second) pushRange.second = end;
        }
};

}
}

#endif // __IRR_C_DOUBLE_BUFFERING_ALLOCATOR_H__


