#pragma once
#include "SmearCore.h"

struct CUDAImage {
 void* data=nullptr;
 int width=0,height=0,pitch=0,origin_x=0,origin_y=0;
};

// The stream is the CUDA stream supplied by AE's GPU device suite.
bool RenderCurveSmearCUDA(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x,double scale_y,void* stream);

// Test helper: uploads host BGRA float buffers, runs the same kernel, and downloads the result.
bool RenderCurveSmearCUDAHost(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x=1,double scale_y=1);
