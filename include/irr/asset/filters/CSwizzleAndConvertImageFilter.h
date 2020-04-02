// Copyright (C) 2020- Mateusz 'DevSH' Kielan
// This file is part of the "IrrlichtBAW" engine.
// For conditions of distribution and use, see copyright notice in irrlicht.h

#ifndef __IRR_C_SWIZZLE_AND_CONVERT_IMAGE_FILTER_H_INCLUDED__
#define __IRR_C_SWIZZLE_AND_CONVERT_IMAGE_FILTER_H_INCLUDED__

#include "irr/core/core.h"

#include <type_traits>

#include "irr/asset/ICPUImageView.h"
#include "irr/asset/filters/CConvertFormatImageFilter.h"

namespace irr
{
namespace asset
{

// do a per-pixel recombination of image channels while converting
class CSwizzleAndConvertImageFilter : public CConvertFormatImageFilter
{
	public:
		virtual ~CSwizzleAndConvertImageFilter() {}

		using state_type = CMatchedSizeInOutImageFilterCommon::state_type;

		static inline bool execute(state_type* state)
		{			
			auto perOutputRegion = [](const CommonExecuteData& commonExecuteData, CBasicImageFilterCommon::clip_region_functor_t& clip) -> bool
			{
				enum FORMAT_META_TYPE
				{
					FMT_FLOAT,
					FMT_INT,
					FMT_UINT
				};
				auto getMetaType = [](E_FORMAT format) -> FORMAT_META_TYPE
				{
					if (!isIntegerFormat(format))
						return FMT_FLOAT;
					else if (isSignedFormat(format))
						return FMT_INT;
					else
						return FMT_UINT;
				};
				const FORMAT_META_TYPE inMetaType = getMetaType(commonExecuteData.inFormat);
				const FORMAT_META_TYPE outMetaType = getMetaType(commonExecuteData.outFormat);

				switch (outMetaType)
				{
					case FMT_FLOAT:
					{
						switch (inMetaType)
						{
							case FMT_FLOAT:
							{
								Swizzle<double,double> swizzle(commonExecuteData,clip);
								break;
							}
							case FMT_INT:
							{
								Swizzle<double,int64_t> swizzle(commonExecuteData,clip);
								break;
							}
							default:
							{
								Swizzle<double,uint64_t> swizzle(commonExecuteData,clip);
								break;
							}
						}
						break;
					}
					case FMT_INT:
					{
						switch (inMetaType)
						{
							case FMT_FLOAT:
							{
								Swizzle<int64_t,double> swizzle(commonExecuteData,clip);
								break;
							}
							case FMT_INT:
							{
								Swizzle<int64_t,int64_t> swizzle(commonExecuteData,clip);
								break;
							}
							default:
							{
								Swizzle<int64_t,uint64_t> swizzle(commonExecuteData,clip);
								break;
							}
						}
						break;
					}
					default:
					{
						switch (inMetaType)
						{
							case FMT_FLOAT:
							{
								Swizzle<uint64_t,double> swizzle(commonExecuteData,clip);
								break;
							}
							case FMT_INT:
							{
								Swizzle<uint64_t,int64_t> swizzle(commonExecuteData,clip);
								break;
							}
							default:
							{
								Swizzle<uint64_t,uint64_t> swizzle(commonExecuteData,clip);
								break;
							}
						}
						break;
					}
				}
			};
			return commonExecute(state,perOutputRegion);
		}

	private:
		template<typename InType, typename OutType>
		struct Swizzle
		{
				Swizzle(const CommonExecuteData& _commonExecuteData, CBasicImageFilterCommon::clip_region_functor_t& clip) : commonExecuteData(_commonExecuteData)
				{
					CBasicImageFilterCommon::executePerRegion(commonExecuteData.inImg,*this,commonExecuteData.inRegions.begin(),commonExecuteData.inRegions.end(),clip);
				}

				inline void operator()(uint32_t readBlockArrayOffset, core::vectorSIMDu32 readBlockPos)
				{
					constexpr auto MaxPlanes = 4;
					constexpr auto MaxChannels = 4;

					const void* pixels[MaxPlanes] = { commonExecuteData.inData+readBlockArrayOffset,nullptr,nullptr,nullptr };
					InType inTmp[MaxChannels];
#if 0
					decodePixels(pixels,inTmp,,);

					OutType outTmp[MaxChannels];
					for (auto i=0; i<MaxChannels; i++)
						doSwizzle(outTmp[i],(&swizzle.r)[i]);
					auto localOutPos = readBlockPos+offsetDifference;
					encodePixels(outData+oit->getByteOffset(localOutPos, outByteStrides),outTmp);
#endif
					assert(false);
				}

			private:
				const CommonExecuteData& commonExecuteData;

				static inline void doSwizzle(OutType& out, void* swizzle)
				{
					assert(false);
				}
		};
		/*
				void* pixels[4] = {,nullptr,nullptr,nullptr};
				auto doSwizzle = [&pixels](auto tmp[4]) -> void
				{
					decodePixels(format,pixels,tmp,0u,0u);
					std::decay<decltype(tmp[0])>::type tmp2[4];
					tmp2[0] = tmp[swizzle.r];
					tmp2[1] = tmp[swizzle.g];
					tmp2[2] = tmp[swizzle.b];
					tmp2[3] = tmp[swizzle.a];
					encodePixels(format,pixels[0],tmp2);
				};
		*/
};

} // end namespace asset
} // end namespace irr

#endif