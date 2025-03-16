#ifndef CUDA_TEST_MAP_H
#define CUDA_TEST_MAP_H

#include "CudaUtils.h"

namespace PureDoom {

// Creates a simple test map directly in CUDA-friendly format for debugging
// Returns a CudaBSPTree with all the necessary data initialized on the device
CudaBSPTree createSimpleTestMap();

// Frees resources allocated for the test map
void freeTestMap(CudaBSPTree& testMap);

} // namespace PureDoom

#endif // CUDA_TEST_MAP_H 