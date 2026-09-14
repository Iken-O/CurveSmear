#include "CurveSmearCUDA.h"
#include <cuda_runtime.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

namespace {
smear::Curve makeCurve(){
 smear::Curve curve;smear::Point previous{365,220};
 for(int k=1;k<=100;k++){double t=k/100.,u=1-t;smear::Point point{u*u*u*365+3*u*u*t*460+3*u*t*t*540+t*t*t*700,u*u*u*220+3*u*u*t*190+3*u*t*t*350+t*t*t*310};curve.add(previous,point);previous=point;}
 curve.prepareSpatialIndex(65);return curve;
}
double batch(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,CUDAImage& output,void* cache,int iterations){
 auto begin=std::chrono::steady_clock::now();for(int i=0;i<iterations;i++)if(!RenderCurveSmearCUDA(curve,settings,source,nullptr,output,1,1,nullptr,cache))return -1;
 return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count()/iterations;
}
double median(std::vector<double> values){std::sort(values.begin(),values.end());return values[values.size()/2];}
bool measure(const char* name,const smear::Curve& curve,smear::Settings settings,const CUDAImage& source,CUDAImage& output){
 constexpr int iterations=50,passes=7;void* cache=CreateCurveSmearCUDACache();if(!cache)return false;
 if(!RenderCurveSmearCUDA(curve,settings,source,nullptr,output,1,1,nullptr,cache)||!RenderCurveSmearCUDA(curve,settings,source,nullptr,output,1,1,nullptr,nullptr)){DestroyCurveSmearCUDACache(cache);return false;}
 std::vector<double> uncached,cached;
 for(int pass=0;pass<passes;pass++){
  double first=pass&1?batch(curve,settings,source,output,cache,iterations):batch(curve,settings,source,output,nullptr,iterations);
  double second=pass&1?batch(curve,settings,source,output,nullptr,iterations):batch(curve,settings,source,output,cache,iterations);
  if(first<0||second<0){DestroyCurveSmearCUDACache(cache);return false;}
  (pass&1?cached:uncached).push_back(first);(pass&1?uncached:cached).push_back(second);
 }
 CUDACacheStats stats{};GetCurveSmearCUDACacheStats(cache,stats);double a=median(uncached),b=median(cached);
 std::printf("%s: uncached %.4f ms, cached %.4f ms, saved %.4f ms (%.1f%%), uploads %llu, hits %llu\n",name,a,b,a-b,100*(a-b)/a,stats.geometry_uploads,stats.cache_hits);DestroyCurveSmearCUDACache(cache);return true;
}
bool verifyConcurrentStreams(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,CUDAImage output){
 void* second_output=nullptr;size_t bytes=static_cast<size_t>(output.pitch)*output.height*sizeof(float4);if(cudaMalloc(&second_output,bytes)!=cudaSuccess)return false;CUDAImage other=output;other.data=second_output;
 cudaStream_t first_stream=nullptr,second_stream=nullptr;if(cudaStreamCreate(&first_stream)!=cudaSuccess||cudaStreamCreate(&second_stream)!=cudaSuccess){cudaStreamDestroy(first_stream);cudaStreamDestroy(second_stream);cudaFree(second_output);return false;}
 void* cache=CreateCurveSmearCUDACache();bool first_ok=true,second_ok=true;
 std::thread first([&]{for(int i=0;i<10;i++)first_ok=RenderCurveSmearCUDA(curve,settings,source,nullptr,output,1,1,reinterpret_cast<void*>(first_stream),cache)&&first_ok;});
 std::thread second([&]{for(int i=0;i<10;i++)second_ok=RenderCurveSmearCUDA(curve,settings,source,nullptr,other,1,1,reinterpret_cast<void*>(second_stream),cache)&&second_ok;});first.join();second.join();
 CUDACacheStats stats{};bool ok=first_ok&&second_ok&&GetCurveSmearCUDACacheStats(cache,stats)&&stats.renders==20&&stats.geometry_uploads==2&&stats.cache_hits==18&&stats.stream_slots==2;
 std::printf("concurrent_streams: %s, slots %u, uploads %llu, hits %llu\n",ok?"PASS":"FAIL",stats.stream_slots,stats.geometry_uploads,stats.cache_hits);
 DestroyCurveSmearCUDACache(cache);cudaStreamDestroy(second_stream);cudaStreamDestroy(first_stream);cudaFree(second_output);return ok;
}
}

int main(){
 constexpr int width=800,height=520;void *source_memory=nullptr,*output_memory=nullptr;size_t bytes=static_cast<size_t>(width)*height*sizeof(float4);
 if(cudaMalloc(&source_memory,bytes)!=cudaSuccess||cudaMalloc(&output_memory,bytes)!=cudaSuccess)return 1;cudaMemset(source_memory,0,bytes);CUDAImage source{source_memory,width,height,width,0,0},output{output_memory,width,height,width,0,0};
 auto curve=makeCurve();smear::Settings active;active.amount=250;active.radius=65;smear::prepareProfile(active);bool ok=measure("active_800x520",curve,active,source,output);auto zero=active;zero.amount=0;ok=measure("zero_800x520",curve,zero,source,output)&&ok;ok=verifyConcurrentStreams(curve,active,source,output)&&ok;
 cudaFree(output_memory);cudaFree(source_memory);return ok?0:2;
}
