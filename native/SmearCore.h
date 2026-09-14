#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cmath>
#include <limits>
#include <vector>
#include "ProfileData.h"

namespace smear {
struct Point { double x=0,y=0; };
struct Segment { Point p; double dx,dy,len,s; };
struct Settings { double amount=250,radius=65,feather=.55,streak=.65,frequency=17,original=0,seed=0; bool reverse=false,preview=false,flat=false,nearest=false,matte_alpha=false; ProfileData profile=defaultProfile(); std::array<float,257> profile_lut=[](){std::array<float,257> a{};a.fill(1);return a;}(); };
struct Mapping { Point source; double influence=0,profile_position=.5; };
inline void prepareProfile(Settings& c){sanitizeProfile(c.profile);for(int i=0;i<=256;i++)c.profile_lut[i]=static_cast<float>(profileValue(c.profile,i/256.));}
inline double profileGain(const Settings& c,double u){double v=std::clamp(u,0.,1.)*256;int i=static_cast<int>(v);double f=v-i;return c.profile_lut[i]*(1-f)+c.profile_lut[std::min(256,i+1)]*f;}
class Curve {
 static constexpr double CELL_SIZE=32.;
 static constexpr std::uint64_t MAX_CELLS=1000000;
 static constexpr std::uint64_t MAX_REFERENCES=8000000;
 struct SpatialIndex {
  double radius=0,origin_x=0,origin_y=0;
  std::uint32_t columns=0,rows=0;
  std::vector<std::uint32_t> offsets,indices;
  bool ready=false;
 } index;
 template<class F> void forCandidate(Point p,const Settings& c,F&& fn) const {
  if(index.ready&&index.radius==c.radius){
   auto column=static_cast<long long>(std::floor((p.x-index.origin_x)/CELL_SIZE));
   auto row=static_cast<long long>(std::floor((p.y-index.origin_y)/CELL_SIZE));
   if(column<0||row<0||column>=index.columns||row>=index.rows)return;
   auto cell=static_cast<size_t>(row)*index.columns+static_cast<size_t>(column);
   for(auto at=index.offsets[cell];at<index.offsets[cell+1];at++)fn(segments[index.indices[at]]);
  }else for(const auto& segment:segments)fn(segment);
 }
public:
 std::vector<Segment> segments;
 double length=0,left=0,top=0,right=0,bottom=0;
 void clearSpatialIndex(){index=SpatialIndex{};}
 bool spatialIndexReady()const{return index.ready;}
 size_t spatialCellCount()const{return index.ready?static_cast<size_t>(index.columns)*index.rows:0;}
 size_t spatialCandidateReferences()const{return index.ready?index.indices.size():0;}
 void prepareSpatialIndex(double radius){
  clearSpatialIndex();
  if(segments.empty()||radius<=0||!std::isfinite(radius)||segments.size()>std::numeric_limits<std::uint32_t>::max())return;
  double min_x=left-radius,min_y=top-radius,max_x=right+radius,max_y=bottom+radius;
  if(!std::isfinite(min_x)||!std::isfinite(min_y)||!std::isfinite(max_x)||!std::isfinite(max_y))return;
  double origin_x=std::floor(min_x/CELL_SIZE)*CELL_SIZE,origin_y=std::floor(min_y/CELL_SIZE)*CELL_SIZE;
  double columns_d=std::floor((max_x-origin_x)/CELL_SIZE)+1,rows_d=std::floor((max_y-origin_y)/CELL_SIZE)+1;
  if(columns_d<1||rows_d<1||columns_d>std::numeric_limits<std::uint32_t>::max()||rows_d>std::numeric_limits<std::uint32_t>::max())return;
  auto columns=static_cast<std::uint32_t>(columns_d),rows=static_cast<std::uint32_t>(rows_d);
  std::uint64_t cell_count=static_cast<std::uint64_t>(columns)*rows;
  if(cell_count>MAX_CELLS)return;
  std::vector<std::uint32_t> counts(static_cast<size_t>(cell_count),0);
  std::uint64_t references=0;
  auto range=[&](const Segment& g){
   long long x0=static_cast<long long>(std::floor((std::min(g.p.x,g.p.x+g.dx)-radius-origin_x)/CELL_SIZE));
   long long x1=static_cast<long long>(std::floor((std::max(g.p.x,g.p.x+g.dx)+radius-origin_x)/CELL_SIZE));
   long long y0=static_cast<long long>(std::floor((std::min(g.p.y,g.p.y+g.dy)-radius-origin_y)/CELL_SIZE));
   long long y1=static_cast<long long>(std::floor((std::max(g.p.y,g.p.y+g.dy)+radius-origin_y)/CELL_SIZE));
   x0=std::clamp<long long>(x0,0,columns-1);x1=std::clamp<long long>(x1,0,columns-1);
   y0=std::clamp<long long>(y0,0,rows-1);y1=std::clamp<long long>(y1,0,rows-1);
   return std::array<long long,4>{x0,x1,y0,y1};
  };
  for(const auto& g:segments){
   auto r=range(g);references+=static_cast<std::uint64_t>(r[1]-r[0]+1)*(r[3]-r[2]+1);
   if(references>MAX_REFERENCES)return;
   for(auto y=r[2];y<=r[3];y++)for(auto x=r[0];x<=r[1];x++){
    auto cell=static_cast<size_t>(y)*columns+static_cast<size_t>(x);
    if(counts[cell]==std::numeric_limits<std::uint32_t>::max())return;
    counts[cell]++;
   }
  }
  SpatialIndex built;built.radius=radius;built.origin_x=origin_x;built.origin_y=origin_y;built.columns=columns;built.rows=rows;
  built.offsets.resize(static_cast<size_t>(cell_count)+1);
  for(size_t cell=0;cell<counts.size();cell++)built.offsets[cell+1]=built.offsets[cell]+counts[cell];
  built.indices.resize(static_cast<size_t>(references));auto cursors=built.offsets;
  for(std::uint32_t segment=0;segment<segments.size();segment++){
   auto r=range(segments[segment]);
   for(auto y=r[2];y<=r[3];y++)for(auto x=r[0];x<=r[1];x++){
    auto cell=static_cast<size_t>(y)*columns+static_cast<size_t>(x);built.indices[cursors[cell]++]=segment;
   }
  }
  built.ready=true;index=std::move(built);
 }
 void add(Point a,Point b) {
  double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<1e-7)return;
  if(index.ready)clearSpatialIndex();
  if(segments.empty()){left=right=a.x;top=bottom=a.y;}
  left=std::min({left,a.x,b.x});right=std::max({right,a.x,b.x});top=std::min({top,a.y,b.y});bottom=std::max({bottom,a.y,b.y});
  segments.push_back({a,dx,dy,len,length});length+=len;
 }
 Point frame(double s,double n) const {
  auto it=std::lower_bound(segments.begin(),segments.end(),s,[](const Segment& g,double v){return g.s+g.len<v;});
  if(it==segments.end())it=segments.end()-1;
  const auto& g=*it;double t=(s-g.s)/g.len;
  return {g.p.x+t*g.dx-n*g.dy/g.len,g.p.y+t*g.dy+n*g.dx/g.len};
 }
 Mapping map(Point p,const Settings& c) const {
  Mapping result{p,0,.5};
  if(segments.empty()||c.radius<=0||p.x<left-c.radius||p.x>right+c.radius||p.y<top-c.radius||p.y>bottom+c.radius)return result;
  double best=c.radius*c.radius,s=0,n=0;bool found=false;
  forCandidate(p,c,[&](const Segment& g){
   double rx=p.x-g.p.x,ry=p.y-g.p.y,t=std::clamp((rx*g.dx+ry*g.dy)/(g.len*g.len),0.,1.);
   double ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;
   if(d<best){best=d;s=g.s+t*g.len;n=(g.dx*ry-g.dy*rx)/g.len;found=true;}
  });
  if(!found||c.flat&&(s<=1e-3||s>=length-1e-3))return result;
  double m=c.feather<=0?1.:std::clamp((c.radius-std::sqrt(best))/(c.radius*c.feather),0.,1.);
  if(c.feather>0)m=m*m*(3-2*m);
  result.influence=m;result.profile_position=std::clamp((1-n/c.radius)/2,0.,1.);if(c.amount<=0||c.original>=1)return result;
  double v=n/std::max(20.,c.radius)*c.frequency+c.seed*1.61803398875;
  double noise=.5+.24*std::sin(v*1.17+1.4)+.16*std::sin(v*2.71+.3)+.10*std::sin(v*5.39+2.1);
  double travel=c.reverse?length-s:s;
  double shift=travel*c.amount/(length+c.amount)*m*(1-c.streak*.85*noise)*profileGain(c,result.profile_position);
  Point a=frame(s,n),b=frame(s+(c.reverse?shift:-shift),n);
  result.source={p.x+b.x-a.x,p.y+b.y-a.y};return result;
 }
};
}
