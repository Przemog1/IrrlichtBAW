#ifndef __C_CUDA_HANDLER_H__
#define __C_CUDA_HANDLER_H__

#include "irr/macros.h"
#include "IReadFile.h"
#include "irr/system/system.h"


#ifdef _IRR_COMPILE_WITH_CUDA_

#include "cuda.h"
#include "nvrtc.h"
#if CUDA_VERSION < 9000
	#error "Need CUDA 9.0 SDK or higher."
#endif

#ifdef _IRR_COMPILE_WITH_OPENGL_
	#include "cudaGL.h"
    #include "../source/Irrlicht/COpenGLDriver.h"
    #include "../source/Irrlicht/COpenGLTexture.h"
#endif // _IRR_COMPILE_WITH_OPENGL_

// useful includes in the future
//#include "cudaEGL.h"
//#include "cudaVDPAU.h"

#include "os.h"

namespace irr
{
namespace cuda
{

#define _IRR_DEFAULT_NVRTC_OPTIONS "--std=c++14",virtualCUDAArchitecture,"-dc","-use_fast_math"


class CCUDAHandler
{
    public:
		using LibLoader = system::DefaultFuncPtrLoader;

		IRR_SYSTEM_DECLARE_DYNAMIC_FUNCTION_CALLER_CLASS(CUDA, LibLoader
			,cuCtxCreate_v2
			,cuDevicePrimaryCtxRetain
			,cuDevicePrimaryCtxRelease
			,cuDevicePrimaryCtxSetFlags
			,cuDevicePrimaryCtxGetState
			,cuCtxDestroy_v2
			,cuCtxEnablePeerAccess
			,cuCtxGetApiVersion
			,cuCtxGetCurrent
			,cuCtxGetDevice
			,cuCtxGetSharedMemConfig
			,cuCtxPopCurrent_v2
			,cuCtxPushCurrent_v2
			,cuCtxSetCacheConfig
			,cuCtxSetCurrent
			,cuCtxSetSharedMemConfig
			,cuCtxSynchronize
			,cuDeviceComputeCapability
			,cuDeviceCanAccessPeer
			,cuDeviceGetCount
			,cuDeviceGet
			,cuDeviceGetAttribute
			,cuDeviceGetLuid
			,cuDeviceGetUuid
			,cuDeviceTotalMem_v2
			,cuDeviceGetName
			,cuDriverGetVersion
			,cuEventCreate
			,cuEventDestroy_v2
			,cuEventElapsedTime
			,cuEventQuery
			,cuEventRecord
			,cuEventSynchronize
			,cuFuncGetAttribute
			,cuFuncSetCacheConfig
			,cuGetErrorName
			,cuGetErrorString
			,cuGraphicsGLRegisterBuffer
			,cuGraphicsGLRegisterImage
			,cuGraphicsMapResources
			,cuGraphicsResourceGetMappedPointer_v2
			,cuGraphicsResourceGetMappedMipmappedArray
			,cuGraphicsSubResourceGetMappedArray
			,cuGraphicsUnmapResources
			,cuGraphicsUnregisterResource
			,cuInit
			,cuLaunchKernel
			,cuMemAlloc_v2
			,cuMemcpyDtoD_v2
			,cuMemcpyDtoH_v2
			,cuMemcpyHtoD_v2
			,cuMemcpyDtoDAsync_v2
			,cuMemcpyDtoHAsync_v2
			,cuMemcpyHtoDAsync_v2
			,cuMemGetAddressRange_v2
			,cuMemFree_v2
			,cuMemFreeHost
			,cuMemGetInfo_v2
			,cuMemHostAlloc
			,cuMemHostRegister_v2
			,cuMemHostUnregister
			,cuMemsetD32_v2
			,cuMemsetD32Async
			,cuMemsetD8_v2
			,cuMemsetD8Async
			,cuModuleGetFunction
			,cuModuleGetGlobal_v2
			,cuModuleLoadDataEx
			,cuModuleLoadFatBinary
			,cuModuleUnload
			,cuOccupancyMaxActiveBlocksPerMultiprocessor
			,cuPointerGetAttribute
			,cuStreamAddCallback
			,cuStreamCreate
			,cuStreamDestroy_v2
			,cuStreamQuery
			,cuStreamSynchronize
			,cuStreamWaitEvent
			,cuGLGetDevices_v2
		);
		static CUDA cuda;

		struct Device
		{
			Device() {}
			Device(int ordinal);
			~Device()
			{
			}

			CUdevice handle = -1;
			char name[122] = {};
			char luid = -1;
			unsigned int deviceNodeMask = 0;
			CUuuid uuid = {};
			size_t vram_size = 0ull;
			int attributes[CU_DEVICE_ATTRIBUTE_MAX] = {};
		};

		IRR_SYSTEM_DECLARE_DYNAMIC_FUNCTION_CALLER_CLASS(NVRTC, LibLoader,
			nvrtcGetErrorString,
			nvrtcVersion,
			nvrtcAddNameExpression,
			nvrtcCompileProgram,
			nvrtcCreateProgram,
			nvrtcDestroyProgram,
			nvrtcGetLoweredName,
			nvrtcGetPTX,
			nvrtcGetPTXSize,
			nvrtcGetProgramLog,
			nvrtcGetProgramLogSize
		);
		static NVRTC nvrtc;
		/*
		IRR_SYSTEM_DECLARE_DYNAMIC_FUNCTION_CALLER_CLASS(NVRTC_BUILTINS, LibLoader
		);
		static NVRTC_BUILTINS nvrtc_builtins;
		*/
	protected:
        CCUDAHandler() = default;

		_IRR_STATIC_INLINE int CudaVersion = 0;
		_IRR_STATIC_INLINE int DeviceCount = 0;
		static core::vector<Device> devices;

		static core::vector<const char*> headers;
		static core::vector<const char*> headerNames;

		_IRR_STATIC_INLINE_CONSTEXPR const char* virtualCUDAArchitectures[] = {	"-arch=compute_30",
																				"-arch=compute_32",
																				"-arch=compute_35",
																				"-arch=compute_37",
																				"-arch=compute_50",
																				"-arch=compute_52",
																				"-arch=compute_53",
																				"-arch=compute_60",
																				"-arch=compute_61",
																				"-arch=compute_62",
																				"-arch=compute_70",
																				"-arch=compute_72",
																				"-arch=compute_75",
																				"-arch=compute_80"};
		_IRR_STATIC_INLINE const char* virtualCUDAArchitecture = nullptr;

	public:
		static CUresult init();
		static void deinit();

		static const char* getCommonVirtualCUDAArchitecture() {return virtualCUDAArchitecture;}

		static bool defaultHandleResult(CUresult result);

		static CUresult getDefaultGLDevices(uint32_t* foundCount, CUdevice* pCudaDevices, uint32_t cudaDeviceCount)
		{
			return cuda.pcuGLGetDevices_v2(foundCount,pCudaDevices,cudaDeviceCount,CU_GL_DEVICE_LIST_ALL);
		}

		template<typename ObjType>
		struct GraphicsAPIObjLink
		{
			core::smart_refctd_ptr<ObjType> obj;
			CUgraphicsResource cudaHandle;

			GraphicsAPIObjLink() : obj(nullptr), cudaHandle(nullptr) {}

			GraphicsAPIObjLink(const GraphicsAPIObjLink& other) = delete;
			GraphicsAPIObjLink& operator=(const GraphicsAPIObjLink& other) = delete;

			~GraphicsAPIObjLink()
			{
				if (obj)
					CCUDAHandler::cuda.pcuGraphicsUnregisterResource(cudaHandle);
			}
		};
		static CUresult registerBuffer(GraphicsAPIObjLink<video::IGPUBuffer>* link, uint32_t flags=CU_GRAPHICS_REGISTER_FLAGS_NONE)
		{
			assert(link->obj);
			auto glbuf = static_cast<video::COpenGLBuffer*>(link->obj.get());
			auto retval = cuda.pcuGraphicsGLRegisterBuffer(&link->cudaHandle,glbuf->getOpenGLName(),flags);
			if (retval!=CUDA_SUCCESS)
				link->obj = nullptr;
			return retval;
		}
		static CUresult registerImage(GraphicsAPIObjLink<video::ITexture>* link, uint32_t flags=CU_GRAPHICS_REGISTER_FLAGS_NONE)
		{
			assert(link->obj);
			
			auto format = link->obj->getColorFormat();
			if (asset::isBlockCompressionFormat(format) || asset::isDepthOrStencilFormat(format) || asset::isScaledFormat(format) || asset::isPlanarFormat(format))
				return CUDA_ERROR_INVALID_IMAGE;

			auto glimg = static_cast<video::COpenGLFilterableTexture*>(link->obj.get());
			GLenum target = glimg->getOpenGLTextureType();
			switch (target)
			{
				case GL_TEXTURE_2D:
				case GL_TEXTURE_2D_ARRAY:
				case GL_TEXTURE_CUBE_MAP:
				case GL_TEXTURE_3D:
					break;
				default:
					return CUDA_ERROR_INVALID_IMAGE;
					break;
			}
			auto retval = cuda.pcuGraphicsGLRegisterImage(&link->cudaHandle,glimg->getOpenGLName(),target,flags);
			if (retval != CUDA_SUCCESS)
				link->obj = nullptr;
			return retval;
		}

		

		static bool defaultHandleResult(nvrtcResult result)
		{
			switch (result)
			{
				case NVRTC_SUCCESS:
					return true;
					break;
				default:
					if (nvrtc.pnvrtcGetErrorString)
						printf("%s\n",nvrtc.pnvrtcGetErrorString(result));
					else
						printf(R"===(CudaHandler: `pnvrtcGetErrorString` is nullptr, the nvrtc library probably not found on the system.\n)===");
					break;
			}
			_IRR_DEBUG_BREAK_IF(true);
			return false;
		}

		//
		static const auto& getCUDASTDHeaders() { return headers; }

		//
		static const auto& getCUDASTDHeaderNames() { return headerNames; }

		static nvrtcResult createProgram(	nvrtcProgram* prog, const char* source, const char* name,
											const char* const* headersBegin=nullptr, const char* const* headersEnd=nullptr,
											const char* const* includeNamesBegin=nullptr, const char* const* includeNamesEnd=nullptr)
		{
			auto headerCount = std::distance(headersBegin, headersEnd);
			if (headerCount)
			{
				if (std::distance(includeNamesBegin,includeNamesEnd)!=headerCount)
					return NVRTC_ERROR_INVALID_INPUT;
				return nvrtc.pnvrtcCreateProgram(prog, source, name, headerCount, headersBegin, includeNamesBegin);
			}
			else
				return nvrtc.pnvrtcCreateProgram(prog, source, name, 0u, nullptr, nullptr);
		}

		template<typename HeaderFileIt>
		static nvrtcResult createProgram(	nvrtcProgram* prog, irr::io::IReadFile* main,
											const HeaderFileIt includesBegin, const HeaderFileIt includesEnd)
		{
			int numHeaders = std::distance(includesBegin,includesEnd);
			core::vector<const char*> headers(numHeaders);
			core::vector<const char*> includeNames(numHeaders);
			size_t sourceSize = main->getSize();
			size_t sourceIt = sourceSize;
			sourceSize++;
			for (auto it=includesBegin; it!=includesEnd; it++)
			{
				sourceSize += it->getSize()+1u;
				includeNames.emplace_back(it->getFileName().c_str());
			}
			core::vector<char> sources(sourceSize);
			main->read(sources.data(),sourceIt);
			sources[sourceIt++] = 0;
			for (auto it=includesBegin; it!=includesEnd; it++)
			{
				auto oldpos = it->getPos();
				it->seek(0ull);

				auto ptr = sources.data()+sourceIt;
				headers.push_back(ptr);
				auto filesize = it->getSize();
				it->read(ptr,filesize);
				sourceIt += filesize;
				sources[sourceIt++] = 0;

				it->seek(oldpos);
			}
			return nvrtc.pnvrtcCreateProgram(prog, sources.data(), main->getFileName().c_str(), numHeaders, headers.data(), includeNames.data());
		}

		static nvrtcResult compileProgram(nvrtcProgram prog, const std::initializer_list<const char*>& options={_IRR_DEFAULT_NVRTC_OPTIONS})
		{
			return nvrtc.pnvrtcCompileProgram(prog, options.size(), options.begin());
		}

		static nvrtcResult compileProgram(nvrtcProgram prog, const std::vector<const char*>& options)
		{
			return nvrtc.pnvrtcCompileProgram(prog, options.size(), options.data());
		}

		//
		static nvrtcResult getProgramLog(nvrtcProgram prog, std::string& log);

		//
		static nvrtcResult getPTX(nvrtcProgram prog, std::string& ptx);

		//
		template<typename OptionsT = const std::initializer_list<const char*>&>
		static nvrtcResult compileDirectlyToPTX(std::string& ptx, irr::io::IReadFile* main,
			const char* const* headersBegin = nullptr, const char* const* headersEnd = nullptr,
			const char* const* includeNamesBegin = nullptr, const char* const* includeNamesEnd = nullptr,
			OptionsT options = { _IRR_DEFAULT_NVRTC_OPTIONS },
			std::string* log = nullptr)
		{
			nvrtcProgram program = nullptr;
			nvrtcResult result = NVRTC_ERROR_PROGRAM_CREATION_FAILURE;
			auto cleanup = core::makeRAIIExiter([&program, &result]() -> void {
				if (result != NVRTC_SUCCESS && program)
					cuda::CCUDAHandler::nvrtc.pnvrtcDestroyProgram(&program);
				});
			
			char* data = new char[main->getSize()+1ull];
			main->read(data, main->getSize());
			data[main->getSize()] = 0;
			result = cuda::CCUDAHandler::createProgram(&program, data, main->getFileName().c_str(), headersBegin, headersEnd, includeNamesBegin, includeNamesEnd);
			delete[] data;

			if (result != NVRTC_SUCCESS)
				return result;

			return result = compileDirectlyToPTX_helper<OptionsT>(ptx, program, std::forward<OptionsT>(options), log);
		}

		template<typename CompileArgsT, typename OptionsT=const std::initializer_list<const char*>&>
		static nvrtcResult compileDirectlyToPTX(std::string& ptx, irr::io::IReadFile* main,
												CompileArgsT includesBegin, CompileArgsT includesEnd,
												OptionsT options={_IRR_DEFAULT_NVRTC_OPTIONS},
												std::string* log=nullptr)
		{
			nvrtcProgram program = nullptr;
			nvrtcResult result = NVRTC_ERROR_PROGRAM_CREATION_FAILURE;
			auto cleanup = core::makeRAIIExiter([&program,&result]() -> void {
				if (result!=NVRTC_SUCCESS && program)
					cuda::CCUDAHandler::nvrtc.pnvrtcDestroyProgram(&program);
			});
			result = cuda::CCUDAHandler::createProgram(&program, main, includesBegin, includesEnd);
			if (result!=NVRTC_SUCCESS)
				return result;

			return result = compileDirectlyToPTX_helper<OptionsT>(ptx,program,std::forward<OptionsT>(options),log);
		}


	protected:
		template<typename OptionsT = const std::initializer_list<const char*>&>
		static nvrtcResult compileDirectlyToPTX_helper(std::string& ptx, nvrtcProgram program, OptionsT options, std::string* log=nullptr)
		{
			nvrtcResult result = cuda::CCUDAHandler::compileProgram(program,options);
			if (log)
				cuda::CCUDAHandler::getProgramLog(program, *log);
			if (result!=NVRTC_SUCCESS)
				return result;

			return cuda::CCUDAHandler::getPTX(program, ptx);
		}
};

}
}

#endif // _IRR_COMPILE_WITH_CUDA_

#endif