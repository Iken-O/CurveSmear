#pragma once
#include "SmearCore.h"

struct CUDAImage {
 void* data=nullptr;
 int width=0,height=0,pitch=0,origin_x=0,origin_y=0;
};

struct CUDACacheStats {
 unsigned long long renders=0,geometry_uploads=0,cache_hits=0,allocations=0;
 unsigned int stream_slots=0;
};

void* CreateCurveSmearCUDACache();
void DestroyCurveSmearCUDACache(void* cache);
bool GetCurveSmearCUDACacheStats(void* cache,CUDACacheStats& stats);

// The stream is the CUDA stream supplied by AE's GPU device suite.
bool RenderCurveSmearCUDA(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x,double scale_y,void* stream,void* cache=nullptr);

// Test helper: uploads host BGRA float buffers, runs the same kernel, and downloads the result.
bool RenderCurveSmearCUDAHost(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x=1,double scale_y=1,void* cache=nullptr);
