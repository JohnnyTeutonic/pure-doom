#ifndef CUDA_TEST_MAP_H
#define CUDA_TEST_MAP_H

#include "CudaUtils.h"
#include "BSPTree.h"
#include <vector>

namespace PureDoom {

// Creates a simple test map directly in CUDA-friendly format for debugging
// Returns a CudaBSPTree with all the necessary data initialized on the device
CudaBSPTree createSimpleTestMap();

// Creates a test map from the provided sectors
// This allows for an arbitrary number of sectors to be used
CudaBSPTree createTestMapFromSectors(const std::vector<Sector>& sectors);

// Frees resources allocated for the test map
void freeTestMap(CudaBSPTree& testMap);

} // namespace PureDoom

#endif // CUDA_TEST_MAP_H 