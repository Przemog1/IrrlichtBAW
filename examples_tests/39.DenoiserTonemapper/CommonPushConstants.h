#ifdef __cplusplus
	#define uint uint32_t
	#define mat3 irr::core::matrix3x4SIMD
#endif
struct CommonPushConstants
{
	uint inImageTexelOffset[3];
	uint inImageTexelPitch[3];
	uint outImageOffset[3];
	uint imageWidth;

	mat3 normalMatrix;
};
#ifdef __cplusplus
	#undef uint uint32_t
	#undef mat3 irr::core::matrix3x4SIMD
#endif