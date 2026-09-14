#include "CurveSmearCUDA.h"
#include <cuda_runtime.h>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <vector>

namespace {
struct GPUSegment {double px,py,dx,dy,len,s;};
struct GPUSettings {
 double amount,radius,feather,streak,frequency,original,seed;
 double length,left,top,right,bottom,index_radius,index_origin_x,index_origin_y;
 int reverse,preview,flat,nearest,matte_alpha,segment_count,index_ready,index_columns,index_rows;
 float profile[257];
};
struct DPoint {double x,y;};
struct DMapping {DPoint source;double influence,profile_position;};

struct DeviceBuffer {
 void* data=nullptr;size_t capacity=0;
 ~DeviceBuffer(){cudaFree(data);}
 bool ensure(size_t bytes,std::atomic_ullong* allocation_count){
  if(bytes<=capacity)return true;
  void* replacement=nullptr;if(cudaMalloc(&replacement,bytes)!=cudaSuccess)return false;
  cudaFree(data);data=replacement;capacity=bytes;if(allocation_count)allocation_count->fetch_add(1,std::memory_order_relaxed);return true;
 }
};
struct CacheSlot {
 std::mutex mutex;DeviceBuffer segments,offsets,indices;
 std::vector<GPUSegment> host_segments;std::vector<std::uint32_t> host_offsets,host_indices;
 void invalidate(){host_segments.clear();host_offsets.clear();host_indices.clear();}
};
struct CUDAPathCache {
 std::mutex slots_mutex;std::unordered_map<void*,std::shared_ptr<CacheSlot>> slots;
 std::atomic_ullong renders{0},geometry_uploads{0},cache_hits{0},allocations{0};
};
constexpr size_t MAX_STREAM_SLOTS=32;

__device__ double clampd(double value,double low,double high){return value<low?low:value>high?high:value;}
__device__ float4 zero4(){return make_float4(0,0,0,0);}
__device__ float4 readPixel(const float4* image,int pitch,int width,int height,int x,int y){return image&&x>=0&&y>=0&&x<width&&y<height?image[y*pitch+x]:zero4();}
__device__ double profileGain(const GPUSettings& c,double u){double v=clampd(u,0.,1.)*256.;int i=static_cast<int>(v);double f=v-i;return c.profile[i]*(1-f)+c.profile[i<256?i+1:256]*f;}
__device__ DPoint frame(const GPUSegment* segments,int count,double s,double n){
 int first=0,last=count;
 while(first<last){int middle=first+(last-first)/2;if(segments[middle].s+segments[middle].len<s)first=middle+1;else last=middle;}
 int index=first<count?first:count-1;const auto& g=segments[index];double t=(s-g.s)/g.len;
 return {g.px+t*g.dx-n*g.dy/g.len,g.py+t*g.dy+n*g.dx/g.len};
}
__device__ void consider(const GPUSegment& g,DPoint p,double& best,double& s,double& n,bool& found){
 double rx=p.x-g.px,ry=p.y-g.py,t=clampd((rx*g.dx+ry*g.dy)/(g.len*g.len),0.,1.);
 double ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;
 if(d<best){best=d;s=g.s+t*g.len;n=(g.dx*ry-g.dy*rx)/g.len;found=true;}
}
__device__ DMapping mapPoint(DPoint p,const GPUSettings& c,const GPUSegment* segments,const std::uint32_t* offsets,const std::uint32_t* indices){
 DMapping result{p,0,.5};
 if(c.segment_count<=0||c.radius<=0||p.x<c.left-c.radius||p.x>c.right+c.radius||p.y<c.top-c.radius||p.y>c.bottom+c.radius)return result;
 double best=c.radius*c.radius,s=0,n=0;bool found=false;
 if(c.index_ready&&c.index_radius==c.radius){
  long long column=static_cast<long long>(floor((p.x-c.index_origin_x)/32.));
  long long row=static_cast<long long>(floor((p.y-c.index_origin_y)/32.));
  if(column<0||row<0||column>=c.index_columns||row>=c.index_rows)return result;
  size_t cell=static_cast<size_t>(row)*c.index_columns+static_cast<size_t>(column);
  for(std::uint32_t at=offsets[cell];at<offsets[cell+1];at++)consider(segments[indices[at]],p,best,s,n,found);
 }else for(int i=0;i<c.segment_count;i++)consider(segments[i],p,best,s,n,found);
 if(!found||(c.flat&&(s<=1e-3||s>=c.length-1e-3)))return result;
 double m=c.feather<=0?1.:clampd((c.radius-sqrt(best))/(c.radius*c.feather),0.,1.);if(c.feather>0)m=m*m*(3-2*m);
 result.influence=m;result.profile_position=clampd((1-n/c.radius)/2,0.,1.);if(c.amount<=0||c.original>=1)return result;
 double v=n/fmax(20.,c.radius)*c.frequency+c.seed*1.61803398875;
 double noise=.5+.24*sin(v*1.17+1.4)+.16*sin(v*2.71+.3)+.10*sin(v*5.39+2.1);
 double travel=c.reverse?c.length-s:s;
 double shift=travel*c.amount/(c.length+c.amount)*m*(1-c.streak*.85*noise)*profileGain(c,result.profile_position);
 auto a=frame(segments,c.segment_count,s,n),b=frame(segments,c.segment_count,s+(c.reverse?shift:-shift),n);
 result.source={p.x+b.x-a.x,p.y+b.y-a.y};return result;
}
__device__ float4 bilinear(const float4* image,int pitch,int width,int height,double x,double y,float4 original,double mix){
 double a=0,r=0,g=0,b=0;
 if(isfinite(x)&&isfinite(y)&&x>-2&&y>-2&&x<width+1&&y<height+1){
  int ix=static_cast<int>(floor(x)),iy=static_cast<int>(floor(y));double fx=x-ix,fy=y-iy;
  for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++){float4 p=readPixel(image,pitch,width,height,ix+xx,iy+yy);double w=(xx?fx:1-fx)*(yy?fy:1-fy);b+=p.x*w;g+=p.y*w;r+=p.z*w;a+=p.w*w;}
 }
 return make_float4(static_cast<float>(b*(1-mix)+original.x*mix),static_cast<float>(g*(1-mix)+original.y*mix),static_cast<float>(r*(1-mix)+original.z*mix),static_cast<float>(a*(1-mix)+original.w*mix));
}
__device__ float4 nearest(const float4* image,int pitch,int width,int height,double x,double y,float4 original,double mix){
 float4 p=readPixel(image,pitch,width,height,static_cast<int>(round(x)),static_cast<int>(round(y)));
 return make_float4(static_cast<float>(p.x*(1-mix)+original.x*mix),static_cast<float>(p.y*(1-mix)+original.y*mix),static_cast<float>(p.z*(1-mix)+original.z*mix),static_cast<float>(p.w*(1-mix)+original.w*mix));
}
__device__ double matteValue(float4 p,bool alpha){return clampd(alpha?p.w:.2126*p.z+.7152*p.y+.0722*p.x,0.,1.);}
__device__ double matteCoverage(const float4* matte,int pitch,int width,int height,double x,double y,bool nearest_sampling,bool alpha){
 if(!matte)return 1.;double value=0;
 if(nearest_sampling)value=matteValue(readPixel(matte,pitch,width,height,static_cast<int>(round(x)),static_cast<int>(round(y))),alpha);
 else if(isfinite(x)&&isfinite(y)&&x>-2&&y>-2&&x<width+1&&y<height+1){int ix=static_cast<int>(floor(x)),iy=static_cast<int>(floor(y));double fx=x-ix,fy=y-iy;for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++)value+=matteValue(readPixel(matte,pitch,width,height,ix+xx,iy+yy),alpha)*(xx?fx:1-fx)*(yy?fy:1-fy);}
 return clampd(value,0.,1.);
}
__global__ void curveSmearKernel(const float4* source,float4* output,const float4* matte,CUDAImage src,CUDAImage dst,CUDAImage mat,GPUSettings settings,const GPUSegment* segments,const std::uint32_t* offsets,const std::uint32_t* indices,double scale_x,double scale_y){
 int x=blockIdx.x*blockDim.x+threadIdx.x,y=blockIdx.y*blockDim.y+threadIdx.y;if(x>=dst.width||y>=dst.height)return;
 float4 original=readPixel(source,src.pitch,src.width,src.height,x+dst.origin_x-src.origin_x,y+dst.origin_y-src.origin_y);output[y*dst.pitch+x]=original;
 if(!settings.preview&&(settings.amount<=0||settings.original>=1||settings.radius<=0))return;
 DPoint pos{(x+dst.origin_x)/scale_x,(y+dst.origin_y)/scale_y};auto mapped=mapPoint(pos,settings,segments,offsets,indices);if(mapped.influence<=0)return;
 if(settings.preview){double u=mapped.profile_position,m=mapped.influence*.35,rr=.96*(1-u)+.48*u,gg=.66*(1-u)+.72*u,bb=.35*(1-u)+u;output[y*dst.pitch+x]=make_float4(static_cast<float>(original.x*(1-m)+bb*m),static_cast<float>(original.y*(1-m)+gg*m),static_cast<float>(original.z*(1-m)+rr*m),static_cast<float>(original.w*(1-m)+m));}
 else if(mapped.source.x!=pos.x||mapped.source.y!=pos.y){double coverage=matteCoverage(matte,mat.pitch,mat.width,mat.height,mapped.source.x*scale_x-mat.origin_x,mapped.source.y*scale_y-mat.origin_y,settings.nearest,settings.matte_alpha),keep=1-(1-settings.original)*coverage;output[y*dst.pitch+x]=settings.nearest?nearest(source,src.pitch,src.width,src.height,mapped.source.x*scale_x-src.origin_x,mapped.source.y*scale_y-src.origin_y,original,keep):bilinear(source,src.pitch,src.width,src.height,mapped.source.x*scale_x-src.origin_x,mapped.source.y*scale_y-src.origin_y,original,keep);}
}
template<class T> bool sameVector(const std::vector<T>& cached,const T* source,size_t count){return cached.size()==count&&(!count||std::memcmp(cached.data(),source,count*sizeof(T))==0);}
template<class T> bool updateBuffer(DeviceBuffer& device,std::vector<T>& cached,const T* source,size_t count,cudaStream_t stream,std::atomic_ullong* allocation_count,bool& uploaded){
 if(sameVector(cached,source,count))return true;
 const size_t bytes=count*sizeof(T);if(bytes&&!device.ensure(bytes,allocation_count))return false;
 if(bytes&&cudaMemcpyAsync(device.data,source,bytes,cudaMemcpyHostToDevice,stream)!=cudaSuccess)return false;
 if(count)cached.assign(source,source+count);else cached.clear();uploaded=true;return true;
}
}

void* CreateCurveSmearCUDACache(){return new(std::nothrow) CUDAPathCache;}
void DestroyCurveSmearCUDACache(void* cache){delete static_cast<CUDAPathCache*>(cache);}
bool GetCurveSmearCUDACacheStats(void* cache,CUDACacheStats& stats){
 auto* c=static_cast<CUDAPathCache*>(cache);if(!c)return false;
 stats.renders=c->renders.load(std::memory_order_relaxed);stats.geometry_uploads=c->geometry_uploads.load(std::memory_order_relaxed);stats.cache_hits=c->cache_hits.load(std::memory_order_relaxed);stats.allocations=c->allocations.load(std::memory_order_relaxed);
 std::lock_guard<std::mutex> lock(c->slots_mutex);stats.stream_slots=static_cast<unsigned int>(c->slots.size());return true;
}

bool RenderCurveSmearCUDA(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x,double scale_y,void* stream_pointer,void* cache_pointer){
 cudaStream_t stream=static_cast<cudaStream_t>(stream_pointer);auto view=curve.spatialView();auto* cache=static_cast<CUDAPathCache*>(cache_pointer);
 CacheSlot temporary;std::shared_ptr<CacheSlot> shared_slot;CacheSlot* slot=&temporary;
 if(cache){
  cache->renders.fetch_add(1,std::memory_order_relaxed);std::lock_guard<std::mutex> lock(cache->slots_mutex);auto found=cache->slots.find(stream_pointer);
  if(found!=cache->slots.end())shared_slot=found->second;
  else if(cache->slots.size()<MAX_STREAM_SLOTS){shared_slot=std::make_shared<CacheSlot>();cache->slots.emplace(stream_pointer,shared_slot);}
  if(shared_slot)slot=shared_slot.get();
 }
 std::lock_guard<std::mutex> slot_lock(slot->mutex);
 std::vector<GPUSegment> host_segments;host_segments.reserve(curve.segments.size());for(const auto& g:curve.segments)host_segments.push_back({g.p.x,g.p.y,g.dx,g.dy,g.len,g.s});
 bool uploaded=false;auto* allocation_count=cache?&cache->allocations:nullptr;
 if(!updateBuffer(slot->segments,slot->host_segments,host_segments.data(),host_segments.size(),stream,allocation_count,uploaded)||
    (view.ready&&!updateBuffer(slot->offsets,slot->host_offsets,view.offsets,view.offset_count,stream,allocation_count,uploaded))||
    (view.ready&&!updateBuffer(slot->indices,slot->host_indices,view.indices,view.index_count,stream,allocation_count,uploaded))){slot->invalidate();return false;}
 if(cache){if(uploaded)cache->geometry_uploads.fetch_add(1,std::memory_order_relaxed);else cache->cache_hits.fetch_add(1,std::memory_order_relaxed);}
 GPUSettings c{};c.amount=settings.amount;c.radius=settings.radius;c.feather=settings.feather;c.streak=settings.streak;c.frequency=settings.frequency;c.original=settings.original;c.seed=settings.seed;c.length=curve.length;c.left=curve.left;c.top=curve.top;c.right=curve.right;c.bottom=curve.bottom;c.reverse=settings.reverse;c.preview=settings.preview;c.flat=settings.flat;c.nearest=settings.nearest;c.matte_alpha=settings.matte_alpha;c.segment_count=static_cast<int>(curve.segments.size());c.index_ready=view.ready;c.index_radius=view.radius;c.index_origin_x=view.origin_x;c.index_origin_y=view.origin_y;c.index_columns=view.columns;c.index_rows=view.rows;for(int i=0;i<=256;i++)c.profile[i]=settings.profile_lut[i];
 CUDAImage empty{};const CUDAImage& mat=matte?*matte:empty;dim3 block(16,16),grid((output.width+15)/16,(output.height+15)/16);
 curveSmearKernel<<<grid,block,0,stream>>>(static_cast<const float4*>(source.data),static_cast<float4*>(output.data),matte?static_cast<const float4*>(matte->data):nullptr,source,output,mat,c,static_cast<const GPUSegment*>(slot->segments.data),view.ready?static_cast<const std::uint32_t*>(slot->offsets.data):nullptr,view.ready?static_cast<const std::uint32_t*>(slot->indices.data):nullptr,scale_x,scale_y);
 bool ok=cudaPeekAtLastError()==cudaSuccess&&cudaStreamSynchronize(stream)==cudaSuccess;
 if(!ok)slot->invalidate();return ok;
}

bool RenderCurveSmearCUDAHost(const smear::Curve& curve,const smear::Settings& settings,const CUDAImage& source,const CUDAImage* matte,CUDAImage& output,double scale_x,double scale_y,void* cache){
 void *device_source=nullptr,*device_matte=nullptr,*device_output=nullptr;size_t source_bytes=static_cast<size_t>(source.pitch)*source.height*16,output_bytes=static_cast<size_t>(output.pitch)*output.height*16,matte_bytes=matte?static_cast<size_t>(matte->pitch)*matte->height*16:0;
 if(cudaMalloc(&device_source,source_bytes)!=cudaSuccess||cudaMalloc(&device_output,output_bytes)!=cudaSuccess){cudaFree(device_output);cudaFree(device_source);return false;}
 if(cudaMemcpy(device_source,source.data,source_bytes,cudaMemcpyHostToDevice)!=cudaSuccess){cudaFree(device_output);cudaFree(device_source);return false;}
 if(matte&&(cudaMalloc(&device_matte,matte_bytes)!=cudaSuccess||cudaMemcpy(device_matte,matte->data,matte_bytes,cudaMemcpyHostToDevice)!=cudaSuccess)){cudaFree(device_matte);cudaFree(device_output);cudaFree(device_source);return false;}
 CUDAImage gpu_source=source,gpu_output=output,gpu_matte{};gpu_source.data=device_source;gpu_output.data=device_output;if(matte){gpu_matte=*matte;gpu_matte.data=device_matte;}
 bool ok=RenderCurveSmearCUDA(curve,settings,gpu_source,matte?&gpu_matte:nullptr,gpu_output,scale_x,scale_y,nullptr,cache)&&cudaMemcpy(output.data,device_output,output_bytes,cudaMemcpyDeviceToHost)==cudaSuccess;
 cudaFree(device_matte);cudaFree(device_output);cudaFree(device_source);return ok;
}
