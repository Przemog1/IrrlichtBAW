#ifndef __IRR_I_VIRTUAL_TEXTURE_H_INCLUDED__
#define __IRR_I_VIRTUAL_TEXTURE_H_INCLUDED__

#include "irr/asset/format/EFormat.h"
#include "irr/core/alloc/GeneralpurposeAddressAllocator.h"
#include "irr/core/alloc/PoolAddressAllocator.h"
#include "irr/core/math/morton.h"
#include "irr/core/alloc/address_allocator_traits.h"
#include "irr/core/memory/memory.h"
#include "irr/asset/filters/CPaddedCopyImageFilter.h"
#include "irr/asset/filters/CFillImageFilter.h"
#include "irr/asset/ISampler.h"

namespace irr {
namespace asset
{

class IVirtualTextureBase
{
public:
    _IRR_STATIC_INLINE_CONSTEXPR uint32_t MAX_PAGE_TABLE_LAYERS = 256u;
#include "irr/irrpack.h"
    //! std430-compatible layout
    struct SPrecomputedData
    {
        uint32_t pgtab_sz_log2;
        float vtex_sz_rcp;
        float layer_to_phys_pg_tex_sz_rcp[MAX_PAGE_TABLE_LAYERS];
        uint32_t layer_to_sampler_ix[MAX_PAGE_TABLE_LAYERS];
    } PACK_STRUCT;
#include "irr/irrunpack.h"
};

template <typename image_view_t, typename sampler_t>
class IVirtualTexture : public core::IReferenceCounted, public IVirtualTextureBase
{
    using this_type = IVirtualTexture<image_view_t, sampler_t>;
protected:
    //! SPhysPgOffset is what is stored in texels of page table!
    struct SPhysPgOffset
    {
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t invalid_addr = 0xffffu;

        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_BITLENGTH = 16u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_MASK = (1u<<PAGE_ADDR_BITLENGTH)-1u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_X_BITS = 4u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_X_MASK = (1u<<PAGE_ADDR_X_BITS)-1u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_Y_BITS = 4u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_Y_MASK = (1u<<PAGE_ADDR_Y_BITS)-1u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t PAGE_ADDR_LAYER_SHIFT = PAGE_ADDR_BITLENGTH - PAGE_ADDR_X_BITS - PAGE_ADDR_Y_BITS;

        inline uint32_t x() const { return addr & PAGE_ADDR_X_MASK; }
        inline uint32_t y() const { return (addr >> PAGE_ADDR_X_BITS)& PAGE_ADDR_Y_MASK; }
        inline uint32_t layer() const { return (addr & PAGE_ADDR_MASK) >> PAGE_ADDR_LAYER_SHIFT; }
        inline bool valid() const { return (addr & PAGE_ADDR_MASK) != invalid_addr; }
        inline SPhysPgOffset mipTailAddr() const { return addr >> PAGE_ADDR_BITLENGTH; }
        inline bool hasMipTailAddr() const { return mipTailAddr().valid(); }

        SPhysPgOffset(uint32_t _addr) : addr(_addr) {}

        //upper 16 bits are used for address of mip-tail page
        uint32_t addr;
    };

    uint32_t neededPageCountForSide(uint32_t _sideExtent, uint32_t _level)
    {
        return (((_sideExtent+(1u<<_level)-1u)>>_level) + m_pgSzxy-1u) / m_pgSzxy;
    }

public:
    _IRR_STATIC_INLINE_CONSTEXPR uint32_t MAX_PAGE_TABLE_EXTENT_LOG2 = 8u;
    _IRR_STATIC_INLINE_CONSTEXPR uint32_t MAX_PHYSICAL_PAGE_SIZE_LOG2 = 9u;
    struct SMiptailPacker
    {
        struct rect
        {
            int x, y, mx, my;

            inline core::vector2du32_SIMD extent() const { return core::vector2du32_SIMD(mx, my)+core::vector2du32_SIMD(1u)-core::vector2du32_SIMD(x,y); }
        };
        static inline bool computeMiptailOffsets(rect* res, int log2SIZE, int padding);
    };

#include "irr/irrpack.h"
    //must be 64bit
    template <typename CRTP>
    struct IRR_FORCE_EBO STextureData
    {
        enum E_WRAP_MODE
        {
            EWM_REPEAT = 0b00,
            EWM_CLAMP = 0b01,
            EWM_MIRROR = 0b10,
            EWM_INVALID = 0b11
        };
        static E_WRAP_MODE ETC_to_EWM(ISampler::E_TEXTURE_CLAMP _etc)
        {
            switch (_etc)
            {
            case ISampler::ETC_REPEAT:
                return EWM_REPEAT;
            case ISampler::ETC_CLAMP_TO_EDGE: _IRR_FALLTHROUGH;
            case ISampler::ETC_CLAMP_TO_BORDER:
                return EWM_CLAMP;
            case ISampler::ETC_MIRROR: _IRR_FALLTHROUGH;
            case ISampler::ETC_MIRROR_CLAMP_TO_EDGE: _IRR_FALLTHROUGH;
            case ISampler::ETC_MIRROR_CLAMP_TO_BORDER:
                return EWM_MIRROR;
            default:
                return EWM_INVALID;
            }
        }
        static ISampler::E_TEXTURE_CLAMP EWM_to_ETC(E_WRAP_MODE _ewm)
        {
            switch (_ewm)
            {
            case EWM_INVALID: _IRR_FALLTHROUGH;
            case EWM_REPEAT:
                return ISampler::ETC_REPEAT;
            case EWM_CLAMP:
                return ISampler::ETC_CLAMP_TO_EDGE;
            case EWM_MIRROR:
                return ISampler::ETC_MIRROR;
            }
        }

        //1st dword
        uint64_t origsize_x : 16;
        uint64_t origsize_y : 16;

        //2nd dword
        uint64_t pgTab_x : 8;
        uint64_t pgTab_y : 8;
        uint64_t pgTab_layer : 8;
        uint64_t maxMip : 4;
        uint64_t wrap_x : 2;
        uint64_t wrap_y : 2;

        static CRTP invalid()
        {
            CRTP inv;
            memset(&inv,0,sizeof(inv));
            inv.wrap_x = EWM_INVALID;
            inv.wrap_y = EWM_INVALID;
            return inv;
        }
        static bool is_invalid(const CRTP& _td)
        {
            return _td.wrap_x==EWM_INVALID||_td.wrap_y==EWM_INVALID;
        }

    protected:
        STextureData() = default;
    } PACK_STRUCT;
#include "irr/irrunpack.h"

    struct IRR_FORCE_EBO SMasterTextureData : STextureData<SMasterTextureData> 
    {
        friend class this_type;
    private:
        SMasterTextureData() = default;
    };
    static_assert(sizeof(SMasterTextureData)==sizeof(uint64_t), "SMasterTextureData is not 64bit!");

    struct IRR_FORCE_EBO SViewAliasTextureData : STextureData<SViewAliasTextureData>
    {
        friend class this_type;
    private:
        SViewAliasTextureData() = default;
    };
    static_assert(sizeof(SViewAliasTextureData)==sizeof(uint64_t), "SViewAliasTextureData is not 64bit!");

protected:
    static SMasterTextureData createMasterTextureData() { return SMasterTextureData(); }
    static SViewAliasTextureData createAliasTextureData() { return SViewAliasTextureData(); }

    using image_t = typename decltype(image_view_t::SCreationParams::image)::pointee;

    using page_tab_offset_t = core::vector3du32_SIMD;
    static page_tab_offset_t page_tab_offset_invalid() { return page_tab_offset_t(~0u,~0u,~0u); }

    SMasterTextureData offsetToTextureData(const page_tab_offset_t& _offset, const VkExtent3D& _extent, uint32_t _mipCount, ISampler::E_TEXTURE_CLAMP _wrapu, ISampler::E_TEXTURE_CLAMP _wrapv)
    {
        auto texData = createMasterTextureData();
        texData.origsize_x = _extent.width;
        texData.origsize_y = _extent.height;

		texData.pgTab_x = _offset.x;
		texData.pgTab_y = _offset.y;
        texData.pgTab_layer = _offset.z;

        texData.maxMip = _mipCount-1u-m_pgSzxy_log2;

        texData.wrap_x = SMasterTextureData::ETC_to_EWM(_wrapu);
        texData.wrap_y = SMasterTextureData::ETC_to_EWM(_wrapv);

        return texData;
    }

    uint32_t computeSquareSz(uint32_t _w, uint32_t _h, uint32_t _baseLevel = 0u)
    {
        const uint32_t w = neededPageCountForSide(_w, _baseLevel);
        const uint32_t h = neededPageCountForSide(_h, _baseLevel);

        return core::roundUpToPoT(std::max(w, h));
    }

    ISampler::SParams getPageTableSamplerParams() const
    {
        ISampler::SParams params;
        params.AnisotropicFilter = 0u;
        params.BorderColor = ISampler::ETBC_FLOAT_OPAQUE_WHITE;
        params.CompareEnable = false;
        params.CompareFunc = ISampler::ECO_NEVER;
        params.LodBias = 0.f;
        params.MaxLod = 10000.f;
        params.MinLod = 0.f;
        params.MaxFilter = ISampler::ETF_NEAREST;
        params.MinFilter = ISampler::ETF_NEAREST;
        params.MipmapMode = ISampler::ESMM_NEAREST;
        params.TextureWrapU = params.TextureWrapV = params.TextureWrapW = ISampler::ETC_CLAMP_TO_EDGE;

        return params;
    }
    ISampler::SParams getPhysicalPageSamplerParams() const
    {
        ISampler::SParams params;
        params.AnisotropicFilter = m_tilePadding ? core::findMSB(m_tilePadding<<1) : 0u;
        params.BorderColor = ISampler::ETBC_FLOAT_OPAQUE_WHITE;
        params.CompareEnable = false;
        params.CompareFunc = ISampler::ECO_NEVER;
        params.LodBias = 0.f;
        params.MaxLod = 0.f;
        params.MinLod = 0.f;
        params.MaxFilter = m_tilePadding ? ISampler::ETF_LINEAR : ISampler::ETF_NEAREST;
        params.MinFilter = m_tilePadding ? ISampler::ETF_LINEAR : ISampler::ETF_NEAREST;
        params.MipmapMode = ISampler::ESMM_NEAREST;
        params.TextureWrapU = params.TextureWrapV = params.TextureWrapW = ISampler::ETC_CLAMP_TO_EDGE;

        return params;
    }

    virtual core::smart_refctd_ptr<sampler_t> createSampler(const ISampler::SParams& _params) const = 0;

    core::smart_refctd_ptr<sampler_t> getPhysicalPageSampler() const
    {
        if (!m_physicalPageSampler)
            m_physicalPageSampler = createSampler(getPhysicalPageSamplerParams());
        return m_physicalPageSampler;
    }
    core::smart_refctd_ptr<sampler_t> getPageTableSampler() const
    {
        if (!m_pageTableSampler)
            m_pageTableSampler = createSampler(getPageTableSamplerParams());
        return m_pageTableSampler;
    }

    uint32_t getMaxAllocationPageCount() const
    {
        auto pgtExtent = m_pageTable()->getCreationParameters().extent;
        return pgtExtent.width*pgtExtent.height;
    }

    _IRR_STATIC_INLINE_CONSTEXPR uint32_t INVALID_SAMPLER_INDEX = 0xdeadbeefu;
    _IRR_STATIC_INLINE_CONSTEXPR uint32_t INVALID_LAYER_INDEX = 0xdeadbeefu;

    uint32_t findFreePageTableLayer() const
    {
        const uint32_t pgtLayers = m_pageTable->getCreationParameters().arrayLayers;
        auto end = m_precomputed.layer_to_sampler_ix+pgtLayers;
        auto found = std::find(m_precomputed.layer_to_sampler_ix, end, INVALID_SAMPLER_INDEX);
        if (found == end)
            return INVALID_LAYER_INDEX;
        return found-m_precomputed.layer_to_sampler_ix;
    }

    core::vector2du32_SIMD getMaxAllocatableTextureSize() const
    {
        return (core::vector2du32_SIMD(&m_pageTable->getCreationParameters().extent.width)*m_pgSzxy) & core::vector2du32_SIMD(~0u,~0u,0u,0u);
    }
    bool isAllocatable(const VkExtent3D& _extent)
    {
        return (core::vector2du32_SIMD(&_extent.width)<=getMaxAllocatableTextureSize()).xyxy().all();
    }

    uint32_t countLevelsTakingAtLeastOnePage(const VkExtent3D& _extent, uint32_t _baseLevel = 0u)
    {
        const uint32_t baseMaxDim = core::roundUpToPoT(core::max(_extent.width, _extent.height))>>_baseLevel;
        const int32_t lastFullMip = core::findMSB(baseMaxDim-1u)+1 - static_cast<int32_t>(m_pgSzxy_log2);

        assert(lastFullMip<static_cast<int32_t>(m_pageTable->getCreationParameters().mipLevels));

        return std::max(lastFullMip+1, 0);
    }

    //this is not static only because it has to call virtual member function
    core::smart_refctd_ptr<image_t> createPageTable(uint32_t _pgTabSzxy_log2, uint32_t _pgTabLayers, uint32_t _pgSzxy_log2, uint32_t _maxAllocatableTexSz_log2)
    {
        assert(_pgTabSzxy_log2<=MAX_PAGE_TABLE_EXTENT_LOG2);//otherwise STextureData encoding falls apart
        assert(_pgTabLayers<=MAX_PAGE_TABLE_LAYERS);

        const uint32_t pgTabSzxy = 1u<<_pgTabSzxy_log2;
        typename image_t::SCreationParams params;
        params.arrayLayers = _pgTabLayers;
        params.extent = {pgTabSzxy,pgTabSzxy,1u};
        params.format = EF_R16G16_UINT;
        params.mipLevels = std::max(static_cast<int32_t>(_maxAllocatableTexSz_log2-_pgSzxy_log2+1u), 1);
        params.samples = IImage::ESCF_1_BIT;
        params.type = IImage::ET_2D;
        params.flags = static_cast<IImage::E_CREATE_FLAGS>(0);

        auto pgtab = createPageTableImage(std::move(params));
        IRR_PSEUDO_IF_CONSTEXPR_BEGIN(std::is_same<image_t,ICPUImage>::value)
        {
            const uint32_t texelSz = getTexelOrBlockBytesize(pgtab->getCreationParameters().format);
        
            auto regions = core::make_refctd_dynamic_array<core::smart_refctd_dynamic_array<ICPUImage::SBufferCopy>>(pgtab->getCreationParameters().mipLevels);

            uint32_t bufOffset = 0u;
            for (uint32_t i = 0u; i < pgtab->getCreationParameters().mipLevels; ++i)
            {
                const uint32_t tilesPerLodDim = pgTabSzxy>>i;
                const uint32_t regionSz = _pgTabLayers*tilesPerLodDim*tilesPerLodDim*texelSz;
                auto& region = (*regions)[i];
                region.bufferOffset = bufOffset;
                region.bufferImageHeight = 0u;
                region.bufferRowLength = tilesPerLodDim;
                region.imageExtent = {tilesPerLodDim,tilesPerLodDim,1u};
                region.imageOffset = {0,0,0};
                region.imageSubresource.baseArrayLayer = 0u;
                region.imageSubresource.layerCount = _pgTabLayers;
                region.imageSubresource.mipLevel = i;
                region.imageSubresource.aspectMask = static_cast<IImage::E_ASPECT_FLAGS>(0);

                bufOffset += regionSz;
            }
            auto buf = core::make_smart_refctd_ptr<ICPUBuffer>(bufOffset);
#ifdef _IRR_DEBUG
            uint32_t* bufptr = reinterpret_cast<uint32_t*>(buf->getPointer());
            std::fill(bufptr, bufptr+bufOffset/sizeof(uint32_t), SPhysPgOffset::invalid_addr);
#endif
            pgtab->setBufferAndRegions(std::move(buf), regions);
        } IRR_PSEUDO_IF_CONSTEXPR_END
        return pgtab;
    }

    void updatePrecomputedData(uint32_t _layer, uint32_t _smplrIndex, float _physTexRcpSz, E_FORMAT _format)
    {
        m_precomputed.layer_to_sampler_ix[_layer] = _smplrIndex;
        m_precomputed.layer_to_phys_pg_tex_sz_rcp[_layer] = _physTexRcpSz;
        m_layerToFormat[_layer] = _format;
        m_precomputedWasUpdatedSinceLastQuery = true;
    }

    const uint32_t m_pgSzxy;
    const uint32_t m_pgSzxy_log2;
    const uint32_t m_pgtabSzxy_log2;
    const uint32_t m_tilePadding;
    core::smart_refctd_ptr<image_t> m_pageTable;
    mutable core::smart_refctd_ptr<image_view_t> m_pageTableView;
    mutable core::smart_refctd_ptr<sampler_t> m_pageTableSampler;
    mutable core::smart_refctd_ptr<sampler_t> m_physicalPageSampler;

    using pg_tab_addr_alctr_t = core::GeneralpurposeAddressAllocator<uint32_t>;
    std::array<pg_tab_addr_alctr_t, MAX_PAGE_TABLE_LAYERS> m_pageTableLayerAllocators;
    uint8_t* m_pgTabAddrAlctr_reservedSpc = nullptr;

    mutable bool m_precomputedWasUpdatedSinceLastQuery = true;
    SPrecomputedData m_precomputed;
    std::array<E_FORMAT, MAX_PAGE_TABLE_LAYERS> m_layerToFormat;

    E_FORMAT getFormatInLayer(uint32_t _layer) const
    {
        return m_layerToFormat[_layer];
    }

    struct SamplerArray
    {
        using range_t = core::SRange<const core::smart_refctd_ptr<image_view_t>>;
        // constant length, specified during construction
        core::smart_refctd_dynamic_array<core::smart_refctd_ptr<image_view_t>> views;

        range_t getViews() const {
            return views ? range_t(views->begin(),views->end()) : range_t(nullptr,nullptr);
        }
    };
    SamplerArray m_fsamplers, m_isamplers, m_usamplers;

    //preallocated arrays for multi_free_addr()
    core::smart_refctd_dynamic_array<uint32_t> m_addrsArray;
    core::smart_refctd_dynamic_array<uint32_t> m_sizesArray;

    class IVTResidentStorage : public core::IReferenceCounted
    {
    protected:
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t MAX_TILES_PER_DIM = std::min(SPhysPgOffset::PAGE_ADDR_X_MASK,SPhysPgOffset::PAGE_ADDR_Y_MASK) + 1u;
        _IRR_STATIC_INLINE_CONSTEXPR uint32_t MAX_LAYERS = (1u<<(SPhysPgOffset::PAGE_ADDR_BITLENGTH-SPhysPgOffset::PAGE_ADDR_LAYER_SHIFT));

        virtual ~IVTResidentStorage()
        {
            if (m_alctrReservedSpace)
                _IRR_ALIGNED_FREE(m_alctrReservedSpace);
        }

    public:
        struct SCreationParams
        {
            E_FORMAT_CLASS formatClass;
            const E_FORMAT* formats;
            uint32_t formatCount;
            uint32_t tilesPerDim_log2;
            uint32_t layerCount;
        };
        using phys_pg_addr_alctr_t = core::PoolAddressAllocator<uint32_t>;

        //_format implies format class and also is the format image is created with
        IVTResidentStorage(uint32_t _layers, uint32_t _tilesPerDim) :
            image(nullptr),//initialized in derived class's constructor
            m_alctrReservedSpace(reinterpret_cast<uint8_t*>(_IRR_ALIGNED_MALLOC(phys_pg_addr_alctr_t::reserved_size(1u, _layers*_tilesPerDim*_tilesPerDim, 1u), _IRR_SIMD_ALIGNMENT))),
            tileAlctr(m_alctrReservedSpace, 0u, 0u, 1u, _layers*_tilesPerDim*_tilesPerDim, 1u),
            m_decodeAddr_layerShift(core::findLSB(_tilesPerDim)<<1),
            m_decodeAddr_xMask((1u<<(m_decodeAddr_layerShift>>1))-1u)
        {
            assert(_tilesPerDim<=MAX_TILES_PER_DIM);
            assert(_layers<=MAX_LAYERS);
        }
        
        IVTResidentStorage(core::smart_refctd_ptr<image_t>&& _image, const phys_pg_addr_alctr_t& _alctr, const void* _reservedSpc, uint32_t _layerShift, uint32_t _xmask) :
            image(std::move(_image)),
            m_alctrReservedSpace(reinterpret_cast<uint8_t*>(_IRR_ALIGNED_MALLOC(phys_pg_addr_alctr_t::reserved_size(_alctr, _alctr.get_total_size()),_IRR_SIMD_ALIGNMENT))),
            tileAlctr(_alctr.get_total_size(), _alctr, m_alctrReservedSpace),
            m_decodeAddr_layerShift(_layerShift),
            m_decodeAddr_xMask(_xmask)
        {

        }

        float getReciprocalSize() const
        {
            return 1.0 / static_cast<double>(image->getCreationParameters().extent.width);
        }

        uint16_t encodePageAddress(uint16_t _addr) const
        {
            const uint16_t x = _addr & m_decodeAddr_xMask;
            const uint16_t y = (_addr>>(m_decodeAddr_layerShift>>1)) & m_decodeAddr_xMask;
            const uint16_t layer = _addr >> m_decodeAddr_layerShift;

            return x | (y<<SPhysPgOffset::PAGE_ADDR_X_BITS) | (layer<<SPhysPgOffset::PAGE_ADDR_LAYER_SHIFT);
        }

        core::smart_refctd_ptr<image_view_t> createView(E_FORMAT _format) const
        {
            auto found = m_viewsCache.find(_format);
            if (found!=m_viewsCache.end())
                return found->second;

            typename image_view_t::SCreationParams params;
            params.flags = static_cast<IImageView<image_t>::E_CREATE_FLAGS>(0);
            params.format = _format;
            params.subresourceRange.aspectMask = static_cast<IImage::E_ASPECT_FLAGS>(0);
            params.subresourceRange.baseArrayLayer = 0u;
            params.subresourceRange.layerCount = image->getCreationParameters().arrayLayers;
            params.subresourceRange.baseMipLevel = 0u;
            params.subresourceRange.levelCount = image->getCreationParameters().mipLevels;
            params.image = core::smart_refctd_ptr<image_t>(image);
            params.viewType = asset::IImageView<image_t>::ET_2D_ARRAY;

            return m_viewsCache.insert({_format,createView_internal(std::move(params))}).first->second;
        }

        //! @returns texel-wise offset of physical page
        static core::vector3du32_SIMD pageCoords(SPhysPgOffset _txoffset, uint32_t _pageSz, uint32_t _padding)
        {
            core::vector3du32_SIMD coords(_txoffset.x(), _txoffset.y(), 0u);
            coords *= (_pageSz + 2u*_padding);
            coords += _padding;
            coords.z = _txoffset.layer();
            return coords;
        }

        core::smart_refctd_ptr<image_t> image;
        uint8_t* m_alctrReservedSpace = nullptr;
        phys_pg_addr_alctr_t tileAlctr;
        const uint32_t m_decodeAddr_layerShift;
        const uint32_t m_decodeAddr_xMask;

    protected:
        virtual core::smart_refctd_ptr<image_view_t> createView_internal(typename image_view_t::SCreationParams&& _params) const = 0;

    private:
        mutable core::unordered_map<E_FORMAT, core::smart_refctd_ptr<image_view_t>> m_viewsCache;
    };
    //since c++14 std::hash specialization for all enum types are given by standard
    core::unordered_map<E_FORMAT_CLASS, core::smart_refctd_ptr<IVTResidentStorage>> m_storage;

    core::unordered_multimap<E_FORMAT, uint32_t> m_viewFormatToLayer;

    typename SMiptailPacker::rect m_miptailOffsets[MAX_PHYSICAL_PAGE_SIZE_LOG2];

    auto getPageTableLayersForFormat(E_FORMAT _format) const
    {
        return m_viewFormatToLayer.equal_range(_format);
    }
    void addPageTableLayerForFormat(E_FORMAT _format, uint32_t _layer)
    {
        m_viewFormatToLayer.insert({_format,_layer});
    }
    void removePageTableLayerForFormat(E_FORMAT _format, uint32_t _layer)
    {
        auto rng = getPageTableLayersForFormat(_format);
        for (auto it = rng.first; it!=rng.second; ++it)
            if  (it->second==_layer)
            {
                m_viewFormatToLayer.erase(it);
                return;
            }
    }

    virtual core::smart_refctd_ptr<image_t> createPageTableImage(typename image_t::SCreationParams&& _params) const = 0;
    virtual core::smart_refctd_ptr<IVTResidentStorage> createVTResidentStorage(E_FORMAT _format, uint32_t _extent, uint32_t _layers, uint32_t _tilesPerDim) = 0;

    virtual ~IVirtualTexture()
    {
        if (m_pgTabAddrAlctr_reservedSpc)
            _IRR_ALIGNED_FREE(m_pgTabAddrAlctr_reservedSpc);
    }

    IVTResidentStorage* getStorageForFormatClass(E_FORMAT_CLASS _fc) const
    {
        auto found = m_storage.find(_fc);
        if (found == m_storage.end())
            return nullptr;
        return found->second.get();
    }

    // Delegated to separate method, because is strongly dependent on derived class and has to be called once derived class's object exist
    void initResidentStorage(
        const typename IVTResidentStorage::SCreationParams* _residentStorageParams,
        uint32_t _residentStorageCount
    )
    {
        {
            const uint32_t tileSz = m_pgSzxy+2u*m_tilePadding;
            for (uint32_t i = 0u; i < _residentStorageCount; ++i)
            {
                const auto& params = _residentStorageParams[i];
                const uint32_t tilesPerDim = (1u<<params.tilesPerDim_log2);
                const uint32_t extent = tilesPerDim*tileSz;
                assert(params.formatCount>0u);
                const E_FORMAT fmt = params.formats[0];
                const uint32_t layers = params.layerCount;
                m_storage.insert({params.formatClass, createVTResidentStorage(fmt, extent, layers, tilesPerDim)});
            }
        }
        {
            auto execPerFormat = [_residentStorageCount, _residentStorageParams,this] (auto f_fmtf, auto f_fmti, auto f_fmtu)
            {
                for (uint32_t i = 0u; i < _residentStorageCount; ++i)
                {
                    const auto& params = _residentStorageParams[i];
                    for (uint32_t j = 0u; j < params.formatCount; ++j)
                    {
                        const E_FORMAT fmt = params.formats[j];
                        auto* storage = m_storage[params.formatClass].get();
                        if (isNormalizedFormat(fmt)||isFloatingPointFormat(fmt)||isScaledFormat(fmt))
                            f_fmtf(fmt,storage);
                        else { //integer formats
                            if (isSignedFormat(fmt))
                                f_fmti(fmt,storage);
                            else
                                f_fmtu(fmt,storage);
                        }
                    }
                }
            };
            {
                uint32_t fcount = 0u, icount = 0u, ucount = 0u;
                execPerFormat([&](auto,auto) {++fcount; }, [&](auto,auto) {++icount; }, [&](auto,auto) {++ucount; });
                m_fsamplers.views = fcount ? core::make_refctd_dynamic_array<decltype(SamplerArray::views)>(fcount) : nullptr;
                m_isamplers.views = icount ? core::make_refctd_dynamic_array<decltype(SamplerArray::views)>(icount) : nullptr;
                m_usamplers.views = ucount ? core::make_refctd_dynamic_array<decltype(SamplerArray::views)>(ucount) : nullptr;
            }
            {
                uint32_t fi = 0u, ii = 0u, ui = 0u;

                execPerFormat(
                    [&fi, this](E_FORMAT _fmt, IVTResidentStorage* _storage) { (*m_fsamplers.views)[fi++] = _storage->createView(_fmt); },
                    [&ii, this](E_FORMAT _fmt, IVTResidentStorage* _storage) { (*m_isamplers.views)[ii++] = _storage->createView(_fmt); },
                    [&ui, this](E_FORMAT _fmt, IVTResidentStorage* _storage) { (*m_usamplers.views)[ui++] = _storage->createView(_fmt); }
                );
            }
        }
    }

    auto createPageTableViewCreationParams() const
    {
        typename image_view_t::SCreationParams params;
        params.flags = static_cast<image_view_t::E_CREATE_FLAGS>(0);
        params.format = m_pageTable->getCreationParameters().format;
        params.subresourceRange.aspectMask = static_cast<IImage::E_ASPECT_FLAGS>(0);
        params.subresourceRange.baseArrayLayer = 0u;
        params.subresourceRange.layerCount = m_pageTable->getCreationParameters().arrayLayers;
        params.subresourceRange.baseMipLevel = 0u;
        params.subresourceRange.levelCount = m_pageTable->getCreationParameters().mipLevels;
        params.viewType = image_view_t::ET_2D_ARRAY;
        params.image = m_pageTable;

        return params;
    }
    virtual core::smart_refctd_ptr<image_view_t> createPageTableView() const = 0;

    bool validateCommit(const SMasterTextureData& _addr, const IImage::SSubresourceRange& _subres, ISampler::E_TEXTURE_CLAMP _uwrap, ISampler::E_TEXTURE_CLAMP _vwrap)
    {
        if (_subres.layerCount != 1u)
            return false;
        if (SMasterTextureData::ETC_to_EWM(_uwrap)!=static_cast<SMasterTextureData::E_WRAP_MODE>(_addr.wrap_x))
            return false;
        if (SMasterTextureData::ETC_to_EWM(_vwrap)!=static_cast<SMasterTextureData::E_WRAP_MODE>(_addr.wrap_y))
            return false;
        return true;
    }

    bool validateAliasCreation(const SMasterTextureData& _addr, E_FORMAT _viewingFormat, const IImage::SSubresourceRange& _subresRelativeToMaster)
    {
        if (_subresRelativeToMaster.baseMipLevel+_subresRelativeToMaster.levelCount > _addr.maxMip+1u)
            return false;
        return true;
    }

public:
    IVirtualTexture(
        uint32_t _pgTabSzxy_log2 = 7u,
        uint32_t _pgTabLayers = 32u,
        uint32_t _pgSzxy_log2 = 7u,
        uint32_t _tilePadding = 9u,
        bool _initSharedResources = true
    ) :
        m_pgSzxy(1u<<_pgSzxy_log2), m_pgSzxy_log2(_pgSzxy_log2), m_pgtabSzxy_log2(_pgTabSzxy_log2), m_tilePadding(_tilePadding)
    {
        {
            m_precomputed.pgtab_sz_log2 = _pgTabSzxy_log2;
            double f = 1.0;
            f /= static_cast<double>(1u<<(m_pgSzxy_log2+m_pgtabSzxy_log2));
            m_precomputed.vtex_sz_rcp = f;
        }

        std::fill(m_precomputed.layer_to_sampler_ix, m_precomputed.layer_to_sampler_ix+MAX_PAGE_TABLE_LAYERS, INVALID_SAMPLER_INDEX);
        std::fill(m_precomputed.layer_to_phys_pg_tex_sz_rcp, m_precomputed.layer_to_phys_pg_tex_sz_rcp+MAX_PAGE_TABLE_LAYERS, core::nan<float>());
        std::fill(m_layerToFormat.begin(), m_layerToFormat.end(), EF_UNKNOWN);

        if (_initSharedResources)
        {
            uint32_t pgtabSzSqr = (1u << _pgTabSzxy_log2);
            pgtabSzSqr *= pgtabSzSqr;
            const size_t spacePerAllocator = pg_tab_addr_alctr_t::reserved_size(pgtabSzSqr, pgtabSzSqr, 1u);
            m_pgTabAddrAlctr_reservedSpc = reinterpret_cast<uint8_t*>( _IRR_ALIGNED_MALLOC(spacePerAllocator*_pgTabLayers, _IRR_SIMD_ALIGNMENT) );
            for (uint32_t i = 0u; i < _pgTabLayers; ++i)
            {
                auto& alctr = m_pageTableLayerAllocators[i];
                alctr = pg_tab_addr_alctr_t(m_pgTabAddrAlctr_reservedSpc+i*spacePerAllocator, 0u, 0u, pgtabSzSqr, pgtabSzSqr, 1u);
            }
        }
        {
            const uint32_t pgSzLog2 = getPageExtent_log2();
            bool ok = SMiptailPacker::computeMiptailOffsets(m_miptailOffsets, pgSzLog2, m_tilePadding);
            assert(ok);
        }

        m_addrsArray = core::make_refctd_dynamic_array<decltype(m_addrsArray)>(1u<<(2u*_pgTabSzxy_log2 + 1u));
        std::fill(m_addrsArray->begin(), m_addrsArray->end(), IVTResidentStorage::phys_pg_addr_alctr_t::invalid_address);
        m_sizesArray = core::make_refctd_dynamic_array<decltype(m_sizesArray)>(1u<<(2u*_pgTabSzxy_log2 + 1u));
        std::fill(m_sizesArray->begin(), m_sizesArray->end(), 1u);
    }

    SMasterTextureData alloc(E_FORMAT _primaryFormat, const VkExtent3D& _mip0extent, const IImage::SSubresourceRange& _subres, ISampler::E_TEXTURE_CLAMP _wrapu, ISampler::E_TEXTURE_CLAMP _wrapv)
    {
        if (_subres.layerCount != 1u)
            return SMasterTextureData::invalid();

        const VkExtent3D extent = {_mip0extent.width>>_subres.baseMipLevel, _mip0extent.height>>_subres.baseMipLevel, 1u};
        if (!isAllocatable(extent))
            return SMasterTextureData::invalid();

        const E_FORMAT format = _primaryFormat;
        uint32_t smplrIndex = 0u;
        IVTResidentStorage* const storage = getStorageForFormatClass(getFormatClass(format));
        {
            if (!storage)
                return SMasterTextureData::invalid();

            SamplerArray* views = nullptr;
            if (isFloatingPointFormat(format)||isNormalizedFormat(format)||isScaledFormat(format))
                views = &m_fsamplers;
            else if (isSignedFormat(format))
                views = &m_isamplers;
            else
                views = &m_usamplers;
            auto view_it = std::find_if(views->views->begin(), views->views->end(), [format](const core::smart_refctd_ptr<ICPUImageView>& _view) {return _view->getCreationParameters().format==format;});
            if (view_it==views->views->end()) //no physical page texture view/sampler for requested format
                return SMasterTextureData::invalid();

            smplrIndex = std::distance(views->views->begin(), view_it);
        }
        auto assignedLayers = getPageTableLayersForFormat(format);

        uint32_t szAndAlignment = computeSquareSz(extent.width, extent.height);
        szAndAlignment *= szAndAlignment;

        uint32_t pgtLayer = 0u;
        uint32_t addr = pg_tab_addr_alctr_t::invalid_address;
        for (auto it = assignedLayers.first; it != assignedLayers.second; ++it)
        {
            pgtLayer = it->second;
            core::address_allocator_traits<pg_tab_addr_alctr_t>::multi_alloc_addr(m_pageTableLayerAllocators[pgtLayer], 1u, &addr, &szAndAlignment, &szAndAlignment, nullptr);
            if (addr!=pg_tab_addr_alctr_t::invalid_address)
                break;
        }
        if (addr==pg_tab_addr_alctr_t::invalid_address)
        {
            pgtLayer = findFreePageTableLayer();
            if (pgtLayer==INVALID_LAYER_INDEX)
                return SMasterTextureData::invalid();
            core::address_allocator_traits<pg_tab_addr_alctr_t>::multi_alloc_addr(m_pageTableLayerAllocators[pgtLayer], 1u, &addr, &szAndAlignment, &szAndAlignment, nullptr);
            assert(addr!=pg_tab_addr_alctr_t::invalid_address);
            addPageTableLayerForFormat(format, pgtLayer);

            updatePrecomputedData(pgtLayer, smplrIndex, storage->getReciprocalSize(), format);
        }

        return offsetToTextureData(
            page_tab_offset_t(core::morton2d_decode_x(addr), core::morton2d_decode_y(addr), pgtLayer),
            extent,
            _subres.levelCount,
            _wrapu,
            _wrapv
        );
    }

    bool destroyAlias(const SViewAliasTextureData& _addr)
    {
        uint32_t sz = computeSquareSz(_addr.origsize_x, _addr.origsize_y);
        sz *= sz;
        const uint32_t addr = core::morton2d_encode(_addr.pgTab_x, _addr.pgTab_y);

        core::address_allocator_traits<pg_tab_addr_alctr_t>::multi_free_addr(m_pageTableLayerAllocators[_addr.pgTab_layer], 1u, &addr, &sz);

        const E_FORMAT format = getFormatInLayer(_addr.pgTab_layer);
        IVTResidentStorage* storage = getStorageForFormatClass(getFormatClass(format));
        if (!storage)
            return false;
        //in case when pgtab layer has no allocations, free it for use by another format
        if (m_pageTableLayerAllocators[_addr.pgTab_layer].get_allocated_size()==0u)
        {
            m_pageTableLayerAllocators[_addr.pgTab_layer].reset();//defragmentation
            updatePrecomputedData(_addr.pgTab_layer, INVALID_SAMPLER_INDEX, core::nan<float>(), EF_UNKNOWN);
            removePageTableLayerForFormat(format, _addr.pgTab_layer);
        }

        return true;
    }

    virtual bool free(const SMasterTextureData& _addr)
    {
        return destroyAlias(reinterpret_cast<const SViewAliasTextureData*>(&_addr)[0]);
    }

    virtual bool commit(const SMasterTextureData& _addr, const image_t* _img, const IImage::SSubresourceRange& _subres, ISampler::E_TEXTURE_CLAMP _uwrap, ISampler::E_TEXTURE_CLAMP _vwrap, ISampler::E_TEXTURE_BORDER_COLOR _borderColor) = 0;

    virtual SViewAliasTextureData createAlias(const SMasterTextureData& _addr, E_FORMAT _viewingFormat, const IImage::SSubresourceRange& _subresRelativeToMaster) = 0;

    //! @returns pointer to reserved space for allocators
    uint8_t* copyVirtualSpaceAllocatorsState(uint32_t _count, pg_tab_addr_alctr_t* _dstArray)
    {
        _count = std::min(_count, m_pageTable->getCreationParameters().arrayLayers);
        const uint32_t bufSz = m_pageTableLayerAllocators[0].get_total_size();
        const uint32_t resSpcPerAlctr = pg_tab_addr_alctr_t::reserved_size(m_pageTableLayerAllocators[0].get_total_size(), m_pageTableLayerAllocators[0]);
        uint8_t* reservedSpc = reinterpret_cast<uint8_t*>( _IRR_ALIGNED_MALLOC(resSpcPerAlctr*_count, _IRR_SIMD_ALIGNMENT) );

        for (uint32_t i = 0u; i < _count; ++i)
            _dstArray[i] = pg_tab_addr_alctr_t(bufSz, m_pageTableLayerAllocators[i], reservedSpc+resSpcPerAlctr);

        return reservedSpc;
    }

    const auto& getViewFormatToLayerMapping() const { return m_viewFormatToLayer; }

    image_view_t* getPageTableView() const
    {
        if (!m_pageTableView)
            m_pageTableView = createPageTableView();
        return m_pageTableView.get();
    }

    image_t* getPageTable() const { return m_pageTable.get(); }
    uint32_t getPageTableExtent_log2() const { return m_pgSzxy_log2; }
    uint32_t getPageExtent() const { return m_pgSzxy; }
    uint32_t getPageExtent_log2() const { return core::findLSB(m_pgSzxy); }
    uint32_t getTilePadding() const { return m_tilePadding; }
    const auto& getResidentStorages() const { return m_storage; }
    typename SamplerArray::range_t getFloatViews() const  { return m_fsamplers.getViews(); }
    typename SamplerArray::range_t getIntViews() const { return m_isamplers.getViews(); }
    typename SamplerArray::range_t getUintViews() const { return m_usamplers.getViews(); }

    inline const auto& getPrecomputedData() const
    {
        return m_precomputed;
    }

    struct reset_update_t {};
    _IRR_STATIC_INLINE_CONSTEXPR reset_update_t reset_update{};
    const auto& getPrecomputedData(reset_update_t)
    {
        m_precomputedWasUpdatedSinceLastQuery = false;
        return m_precomputed;
    }

    static std::string getGLSLExtensionsIncludePath()
    {
        return "irr/builtin/glsl/virtual_texturing/extensions.glsl";
    }
    std::string getGLSLDescriptorsIncludePath() const
    {
        return "irr/builtin/glsl/virtual_texturing/descriptors.glsl";
    }
    std::string getGLSLFunctionsIncludePath() const
    {
        //functions.glsl/pg_sz_log2/tile_padding/pgtab_tex_name/phys_pg_tex_name/get_pgtab_sz_log2_name/get_phys_pg_tex_sz_rcp_name/get_vtex_sz_rcp_name/get_layer2pid/(addr_x_bits/addr_y_bits)...
        std::string s = "irr/builtin/glsl/virtual_texturing/functions.glsl/";
        s += std::to_string(m_pgSzxy_log2) + "/";
        s += std::to_string(m_tilePadding);

        return s;
    }

protected:
    template <typename DSlayout_t>
    std::pair<uint32_t,uint32_t> getDSlayoutBindings_internal(typename DSlayout_t::SBinding* _outBindings, core::smart_refctd_ptr<sampler_t>* _outSamplers, uint32_t _pgtBinding = 0u, uint32_t _fsamplersBinding = 1u, uint32_t _isamplersBinding = 2u, uint32_t _usamplersBinding = 3u) const
    {
        const uint32_t bindingCount = 1u+(getFloatViews().size()?1u:0u)+(getIntViews().size()?1u:0u)+(getUintViews().size()?1u:0u);
        const uint32_t samplerCount = 1u+std::max(getFloatViews().size(), std::max(getIntViews().size(), getUintViews().size()));
        if (!_outBindings || !_outSamplers)
            return std::make_pair(bindingCount, samplerCount);

        auto* bindings = _outBindings;
        auto* samplers = _outSamplers;

        samplers[0] = getPageTableSampler();
        std::fill(samplers+1, samplers+samplerCount, getPhysicalPageSampler());

        auto fillBinding = [](auto& bnd, uint32_t _binding, uint32_t _count, core::smart_refctd_ptr<sampler_t>* _samplers) {
            bnd.binding = _binding;
            bnd.count = _count;
            bnd.stageFlags = asset::ISpecializedShader::ESS_ALL;
            bnd.type = asset::EDT_COMBINED_IMAGE_SAMPLER;
            bnd.samplers = _samplers;
        };

        fillBinding(bindings[0], _pgtBinding, 1u, samplers);

        uint32_t i = 1u;
        if (getFloatViews().size())
        {
            fillBinding(bindings[i], _fsamplersBinding, getFloatViews().size(), samplers+1);
            ++i;
        }
        if (getIntViews().size())
        {
            fillBinding(bindings[i], _isamplersBinding, getIntViews().size(), samplers+1);
            ++i;
        }
        if (getUintViews().size())
        {
            fillBinding(bindings[i], _usamplersBinding, getUintViews().size(), samplers+1);
        }

        return std::make_pair(bindingCount, samplerCount);
    }

    template <typename DS_t>
    std::pair<uint32_t,uint32_t> getDescriptorSetWrites_internal(typename DS_t::SWriteDescriptorSet* _outWrites, typename DS_t::SDescriptorInfo* _outInfo, DS_t* _dstSet, uint32_t _pgtBinding = 0u, uint32_t _fsamplersBinding = 1u, uint32_t _isamplersBinding = 2u, uint32_t _usamplersBinding = 3u) const
    {
        const uint32_t writeCount = 1u+(getFloatViews().size()?1u:0u)+(getIntViews().size()?1u:0u)+(getUintViews().size()?1u:0u);
        const uint32_t infoCount = 1u + getFloatViews().size() + getIntViews().size() + getUintViews().size();

        if (!_outWrites || !_outInfo)
            return std::make_pair(writeCount, infoCount);

        auto* writes = _outWrites;
        auto* info = _outInfo;

        writes[0].binding = _pgtBinding;
        writes[0].arrayElement = 0u;
        writes[0].count = 1u;
        writes[0].descriptorType = EDT_COMBINED_IMAGE_SAMPLER;
        writes[0].dstSet = _dstSet;
        writes[0].info = info;
        info[0].desc = core::smart_refctd_ptr<image_view_t>(getPageTableView());
        info[0].image.imageLayout = EIL_UNDEFINED;
        info[0].image.sampler = nullptr; //samplers are left for user to specify at will

        uint32_t i = 1u, j = 1u;
        if (getFloatViews().size())
        {
            writes[i].binding = _fsamplersBinding;
            writes[i].arrayElement = 0u;
            writes[i].count = getFloatViews().size();
            writes[i].descriptorType = EDT_COMBINED_IMAGE_SAMPLER;
            writes[i].dstSet = _dstSet;
            writes[i].info = info+j;
            for (uint32_t j0 = j; (j-j0)<writes[i].count; ++j)
            {
                info[j].desc = getFloatViews().begin()[j-j0];
                info[j].image.imageLayout = EIL_UNDEFINED;
                info[j].image.sampler = nullptr;
            }
            ++i;
        }
        if (getIntViews().size())
        {
            writes[i].binding = _isamplersBinding;
            writes[i].arrayElement = 0u;
            writes[i].count = getIntViews().size();
            writes[i].descriptorType = EDT_COMBINED_IMAGE_SAMPLER;
            writes[i].dstSet = _dstSet;
            writes[i].info = info+j;
            for (uint32_t j0 = j; (j-j0)<writes[i].count; ++j)
            {
                info[j].desc = getIntViews().begin()[j-j0];
                info[j].image.imageLayout = EIL_UNDEFINED;
                info[j].image.sampler = nullptr;
            }
            ++i;
        }
        if (getUintViews().size())
        {
            writes[i].binding = _usamplersBinding;
            writes[i].arrayElement = 0u;
            writes[i].count = getUintViews().size();
            writes[i].descriptorType = EDT_COMBINED_IMAGE_SAMPLER;
            writes[i].dstSet = _dstSet;
            writes[i].info = info+j;
            for (uint32_t j0 = j; (j-j0)<writes[i].count; ++j)
            {
                info[j].desc = getUintViews().begin()[j-j0];
                info[j].image.imageLayout = EIL_UNDEFINED;
                info[j].image.sampler = nullptr;
            }
            ++i;
        }

        return std::make_pair(writeCount, infoCount);
    }
};

template <typename image_view_t, typename sampler_t>
bool IVirtualTexture<image_view_t, sampler_t>::SMiptailPacker::computeMiptailOffsets(IVirtualTexture<image_view_t, sampler_t>::SMiptailPacker::rect* res, int log2SIZE, int padding)
{
    if (log2SIZE<7 || log2SIZE>10)
        return false;
    
    int SIZE = 0x1u<<log2SIZE;
	if ((SIZE>>1)+(SIZE>>2) + padding * 4 > SIZE)
		return false;
	

	int x1 = 0, y1 = 0;
	res[0].x = 0;
	res[0].mx = (SIZE >> 1) + padding * 2 -1;
	res[0].y = 0;
	res[0].my = (SIZE >> 1) + padding * 2 -1;
	y1 = res[0].my + 1;
	x1 = res[0].mx + 1;

	bool ofx1 = false, ofx2 = false;
	int i = 1;
	while (i < log2SIZE)
	{

		int s = (SIZE >> (i + 1)) + padding * 2;
		int x, y;
		if (i % 2 == 0) {	//on right
			if (i == 2)
			{
				x = x1;
				y = 0;
			}
			else
			{
				if (res[i - 2].my + s > y1)
				{
					if (res[i - 6].mx + s > SIZE || ofx1)
					{
						x = res[i - 4].mx + 1;
						y = res[i - 4].y;
						ofx1 = true;
					}
					else {

					x = res[i - 6].mx + 1;
					y = res[i - 6].y;
					}
				}
				else
				{
					y = res[i - 2].my + 1;
					x = x1;
				}
			}
		}
		else //those placed below the first square and going right are ~x2 larger than those above
		{
			if (i == 1)
			{
				x = 0;
				y = y1;
			}
			else {
				if (res[i - 2].mx + s > SIZE)
				{
					if (res[i - 6].my + s > SIZE ||ofx2)
					{
						x = res[i - 4].x;
						y = res[i - 4].my + 1;
						ofx2 = true;
					}
					else {
						x = res[i - 6].x;
						y = res[i - 6].my + 1;
					}
				}
				else 
				{
				    x = res[i - 2].mx + 1;
				    y = y1;
				}
			}
		}
		res[i].x = x;
		res[i].y = y;
		res[i].mx = x + s -1;
		res[i].my = y + s -1 ;

		i++;
	}
    return true;
}

}}

#endif