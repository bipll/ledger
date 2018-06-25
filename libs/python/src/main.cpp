#include "python/fetch_pybind.hpp"
#include "python/memory/py_range.hpp"
#include "python/memory/py_array.hpp"
#include "python/memory/py_shared_array.hpp"
#include "python/memory/py_shape_less_array.hpp"
//#include "python/memory/py_ndarray.hpp"
#include "python/memory/py_rectangular_array.hpp"

#include "python/math/py_exp.hpp"
#include "python/math/py_bignumber.hpp"
#include "python/math/py_log.hpp"
#include "python/math/linalg/py_matrix.hpp"
#include "python/math/spline/py_linear.hpp"
#include "python/math/distance/py_pearson.hpp"
#include "python/math/distance/py_eisen.hpp"
#include "python/math/distance/py_manhattan.hpp"
#include "python/math/distance/py_euclidean.hpp"
#include "python/math/distance/py_jaccard.hpp"
#include "python/math/distance/py_hamming.hpp"
#include "python/math/distance/py_chebyshev.hpp"
#include "python/math/distance/py_braycurtis.hpp"
#include "python/math/distance/py_distance_matrix.hpp"
#include "python/math/distance/py_pairwise_distance.hpp"

#include "python/math/correlation/py_pearson.hpp"
#include "python/math/correlation/py_eisen.hpp"
#include "python/math/correlation/py_jaccard.hpp"

#include "python/math/statistics/py_mean.hpp"
#include "python/math/statistics/py_geometric_mean.hpp"
#include "python/math/statistics/py_variance.hpp"
#include "python/math/statistics/py_standard_deviation.hpp"

#include "python/byte_array/py_referenced_byte_array.hpp"
#include "python/byte_array/py_encoders.hpp"
#include "python/byte_array/py_const_byte_array.hpp"
#include "python/byte_array/py_basic_byte_array.hpp"
#include "python/byte_array/py_consumers.hpp"
#include "python/byte_array/py_decoders.hpp"
#include "python/byte_array/tokenizer/py_token.hpp"
#include "python/byte_array/tokenizer/py_tokenizer.hpp"
#include "python/byte_array/details/py_encode_decode.hpp"

#include "python/random/py_bitmask.hpp"
#include "python/random/py_lfg.hpp"
#include "python/random/py_bitgenerator.hpp"
#include "python/random/py_lcg.hpp"

PYBIND11_MODULE(fetch, module) {
  namespace py = pybind11;

// Namespaces

  py::module ns_fetch_random = module.def_submodule("random");
  py::module ns_fetch_vectorize = module.def_submodule("vectorize");
  py::module ns_fetch_image = module.def_submodule("image");
  py::module ns_fetch_image_colors = ns_fetch_image.def_submodule("colors");
  py::module ns_fetch_math = module.def_submodule("math");
  py::module ns_fetch_math_correlation = ns_fetch_math.def_submodule("correlation");    
  py::module ns_fetch_math_distance = ns_fetch_math.def_submodule("distance");
  py::module ns_fetch_math_statistics = ns_fetch_math.def_submodule("statistics");    
  py::module ns_fetch_math_spline = ns_fetch_math.def_submodule("spline");
  py::module ns_fetch_memory = module.def_submodule("memory");
  py::module ns_fetch_byte_array = module.def_submodule("byte_array");
  py::module ns_fetch_math_linalg = ns_fetch_math.def_submodule("linalg");


  fetch::memory::BuildArray<int8_t>("ArrayInt8", ns_fetch_memory);      
  fetch::memory::BuildArray<int16_t>("ArrayInt16", ns_fetch_memory);    
  fetch::memory::BuildArray<int32_t>("ArrayInt32", ns_fetch_memory);
  fetch::memory::BuildArray<int64_t>("ArrayInt64", ns_fetch_memory);

  fetch::memory::BuildArray<uint8_t>("ArrayUInt8", ns_fetch_memory);      
  fetch::memory::BuildArray<uint16_t>("ArrayUInt16", ns_fetch_memory);    
  fetch::memory::BuildArray<uint32_t>("ArrayUInt32", ns_fetch_memory);
  fetch::memory::BuildArray<uint64_t>("ArrayUInt64", ns_fetch_memory);
  
  fetch::memory::BuildArray<float>("ArrayFloat", ns_fetch_memory);
  fetch::memory::BuildArray<double>("ArrayDouble", ns_fetch_memory);  


  fetch::memory::BuildSharedArray<int8_t>("SharedArrayInt8", ns_fetch_memory);      
  fetch::memory::BuildSharedArray<int16_t>("SharedArrayInt16", ns_fetch_memory);    
  fetch::memory::BuildSharedArray<int32_t>("SharedArrayInt32", ns_fetch_memory);
  fetch::memory::BuildSharedArray<int64_t>("SharedArrayInt64", ns_fetch_memory);

  fetch::memory::BuildSharedArray<uint8_t>("SharedArrayUInt8", ns_fetch_memory);      
  fetch::memory::BuildSharedArray<uint16_t>("SharedArrayUInt16", ns_fetch_memory);    
  fetch::memory::BuildSharedArray<uint32_t>("SharedArrayUInt32", ns_fetch_memory);
  fetch::memory::BuildSharedArray<uint64_t>("SharedArrayUInt64", ns_fetch_memory);
  
  fetch::memory::BuildSharedArray<float>("SharedArrayFloat", ns_fetch_memory);
  fetch::memory::BuildSharedArray<double>("SharedArrayDouble", ns_fetch_memory);  


  fetch::memory::BuildRange("Range", ns_fetch_memory);  
  /*
  fetch::math::BuildShapeLessArray<int8_t>("ShapeLessArrayInt8", ns_fetch_memory);      
  fetch::math::BuildShapeLessArray<int16_t>("ShapeLessArrayInt16", ns_fetch_memory);    
  fetch::math::BuildShapeLessArray<int32_t>("ShapeLessArrayInt32", ns_fetch_memory);
  fetch::math::BuildShapeLessArray<int64_t>("ShapeLessArrayInt64", ns_fetch_memory);

  fetch::math::BuildShapeLessArray<uint8_t>("ShapeLessArrayUInt8", ns_fetch_memory);      
  fetch::math::BuildShapeLessArray<uint16_t>("ShapeLessArrayUInt16", ns_fetch_memory);    
  fetch::math::BuildShapeLessArray<uint32_t>("ShapeLessArrayUInt32", ns_fetch_memory);
  fetch::math::BuildShapeLessArray<uint64_t>("ShapeLessArrayUInt64", ns_fetch_memory);
  */  
  fetch::math::BuildShapeLessArray<float>("ShapeLessArrayFloat", ns_fetch_memory);
  fetch::math::BuildShapeLessArray<double>("ShapeLessArrayDouble", ns_fetch_memory);  



  /*
  fetch::math::BuildRectangularArray<int8_t>("RectangularArrayInt8", ns_fetch_memory);      
  fetch::math::BuildRectangularArray<int16_t>("RectangularArrayInt16", ns_fetch_memory);    
  fetch::math::BuildRectangularArray<int32_t>("RectangularArrayInt32", ns_fetch_memory);
  fetch::math::BuildRectangularArray<int64_t>("RectangularArrayInt64", ns_fetch_memory);

  fetch::math::BuildRectangularArray<uint8_t>("RectangularArrayUInt8", ns_fetch_memory);      
  fetch::math::BuildRectangularArray<uint16_t>("RectangularArrayUInt16", ns_fetch_memory);    
  fetch::math::BuildRectangularArray<uint32_t>("RectangularArrayUInt32", ns_fetch_memory);
  fetch::math::BuildRectangularArray<uint64_t>("RectangularArrayUInt64", ns_fetch_memory);
  */  
  fetch::math::BuildRectangularArray<float>("RectangularArrayFloat", ns_fetch_memory);
  fetch::math::BuildRectangularArray<double>("RectangularArrayDouble", ns_fetch_memory);  

  

//  fetch::math::BuildExp< 0, 60801, false>("Exp0", ns_fetch_math);  
//  fetch::math::BuildLog(ns_fetch_math);

//  fetch::math::linalg::BuildMatrix<int8_t>("MatrixInt8", ns_fetch_math_linalg);      
//  fetch::math::linalg::BuildMatrix<int16_t>("MatrixInt16", ns_fetch_math_linalg);    
//  fetch::math::linalg::BuildMatrix<int32_t>("MatrixInt32", ns_fetch_math_linalg);
  //  fetch::math::linalg::BuildMatrix<int64_t>("MatrixInt64", ns_fetch_math_linalg);

  //  fetch::math::linalg::BuildMatrix<uint8_t>("MatrixUInt8", ns_fetch_math_linalg);      
  //  fetch::math::linalg::BuildMatrix<uint16_t>("MatrixUInt16", ns_fetch_math_linalg);    
  //  fetch::math::linalg::BuildMatrix<uint32_t>("MatrixUInt32", ns_fetch_math_linalg);
  //  fetch::math::linalg::BuildMatrix<uint64_t>("MatrixUInt64", ns_fetch_math_linalg);
  
  fetch::math::linalg::BuildMatrix<float>("MatrixFloat", ns_fetch_math_linalg);
  fetch::math::linalg::BuildMatrix<double>("MatrixDouble", ns_fetch_math_linalg);  

//  fetch::math::BuildSpline(ns_fetch_math_spline);
//  fetch::image::colors::BuildAbstractColor<uint32_t, 8, 3>("ColorRGB8",ns_fetch_image_colors);
//  fetch::image::colors::BuildAbstractColor<uint32_t, 8, 4>("ColorRGBA8",ns_fetch_image_colors); 

//  fetch::image::BuildImageType< fetch::image::colors::RGB8 >("ImageRGB8", ns_fetch_image);
//  fetch::image::BuildImageType< fetch::image::colors::RGBA8 >("ImageRGBA8", ns_fetch_image);  




  ///////////
  // Comparisons

  fetch::math::correlation::BuildPearsonCorrelation("Pearson", ns_fetch_math_correlation);  
  fetch::math::correlation::BuildEisenCorrelation("Eisen", ns_fetch_math_correlation);  
  fetch::math::correlation::BuildJaccardCorrelation("Jaccard", ns_fetch_math_correlation);
  fetch::math::correlation::BuildGeneralisedJaccardCorrelation("GeneralisedJaccard", ns_fetch_math_correlation);  
  
  fetch::math::distance::BuildPearsonDistance("Pearson", ns_fetch_math_distance);
  fetch::math::distance::BuildEisenDistance("Eisen", ns_fetch_math_distance);
  fetch::math::distance::BuildEisenDistance("Cosine", ns_fetch_math_distance);  
  fetch::math::distance::BuildManhattanDistance("Manhattan", ns_fetch_math_distance);
  fetch::math::distance::BuildEuclideanDistance("Euclidean", ns_fetch_math_distance);  
  fetch::math::distance::BuildJaccardDistance("Jaccard", ns_fetch_math_distance); 
  fetch::math::distance::BuildGeneralisedJaccardDistance("GeneralisedJaccard", ns_fetch_math_distance);
  
  fetch::math::distance::BuildHammingDistance("Hamming", ns_fetch_math_distance);     
  fetch::math::distance::BuildChebyshevDistance("Chebyshev", ns_fetch_math_distance);
  fetch::math::distance::BuildBraycurtisDistance("Braycurtis", ns_fetch_math_distance);

  fetch::math::distance::BuildDistanceMatrixDistance("DistanceMatrix", ns_fetch_math_distance);
  fetch::math::distance::BuildPairWiseDistanceDistance("PairWiseDistance", ns_fetch_math_distance);  


  ////////////
  // Statistics
  fetch::math::statistics::BuildMeanStatistics("Mean", ns_fetch_math_statistics);     
  fetch::math::statistics::BuildGeometricMeanStatistics("GeometricMean", ns_fetch_math_statistics);
  fetch::math::statistics::BuildVarianceStatistics("Variance", ns_fetch_math_statistics);
  fetch::math::statistics::BuildStandardDeviationStatistics("StandardDeviation", ns_fetch_math_statistics);  
  
  
  fetch::byte_array::BuildBasicByteArray(ns_fetch_byte_array);
  fetch::byte_array::BuildByteArray(ns_fetch_byte_array);
  fetch::byte_array::BuildConstByteArray(ns_fetch_byte_array);
  
  fetch::random::BuildLaggedFibonacciGenerator<418, 1279 > ( "LaggedFibonacciGenerator",ns_fetch_random);
  fetch::random::BuildLinearCongruentialGenerator(ns_fetch_random);
  fetch::random::BuildBitMask<uint64_t, 12, true >("BitMask", ns_fetch_random);

  fetch::random::BuildBitGenerator< fetch::random::LaggedFibonacciGenerator<>, 12, true >("BitGenerator",ns_fetch_random);

}
