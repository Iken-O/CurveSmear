#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace smear {
constexpr std::uint32_t PROFILE_SCHEMA=0x43535031; // CSP1
constexpr int PROFILE_MAX_POINTS=16;
struct ProfilePoint { float x=0,y=1; };
struct ProfileData {
 std::uint32_t schema=PROFILE_SCHEMA,count=2,smooth=1,selected=0;
 std::array<ProfilePoint,PROFILE_MAX_POINTS> points{};
};
inline ProfileData defaultProfile(){ProfileData p;p.points[0]={0,1};p.points[1]={1,1};return p;}
inline void sanitizeProfile(ProfileData& p){
 if(p.schema!=PROFILE_SCHEMA)p=defaultProfile();
 p.count=std::clamp<std::uint32_t>(p.count,2,PROFILE_MAX_POINTS);
 std::sort(p.points.begin(),p.points.begin()+p.count,[](const auto& a,const auto& b){return a.x<b.x;});
 p.points[0].x=0;p.points[p.count-1].x=1;
 for(std::uint32_t i=0;i<p.count;i++){
  p.points[i].y=std::clamp(p.points[i].y,0.f,1.f);
  if(i>0&&i+1<p.count)p.points[i].x=std::clamp(p.points[i].x,p.points[i-1].x+.001f,1.f-(p.count-1-i)*.001f);
 }
 p.smooth=p.smooth?1u:0u;p.selected=std::min(p.selected,p.count-1);
}
inline double profileValue(const ProfileData& input,double u){
 const ProfileData& p=input;u=std::clamp(u,0.,1.);
 std::uint32_t j=0;while(j+2<p.count&&u>p.points[j+1].x)j++;
 const auto& a=p.points[j];const auto& b=p.points[j+1];double h=b.x-a.x,t=(u-a.x)/h;
 if(!p.smooth)return a.y+(b.y-a.y)*t;
 std::array<double,PROFILE_MAX_POINTS> d{},m{};
 for(std::uint32_t i=0;i+1<p.count;i++)d[i]=(p.points[i+1].y-p.points[i].y)/(p.points[i+1].x-p.points[i].x);
 m[0]=d[0];m[p.count-1]=d[p.count-2];
 for(std::uint32_t i=1;i+1<p.count;i++){
  double left=p.points[i].x-p.points[i-1].x,right=p.points[i+1].x-p.points[i].x,aSlope=d[i-1],bSlope=d[i];
  m[i]=aSlope*bSlope<=0?0:(3*(left+right))/((2*right+left)/aSlope+(right+2*left)/bSlope);
 }
 double t2=t*t,t3=t2*t,v=(2*t3-3*t2+1)*a.y+(t3-2*t2+t)*h*m[j]+(-2*t3+3*t2)*b.y+(t3-t2)*h*m[j+1];
 return std::clamp(v,0.,1.);
}
inline void resetProfile(ProfileData& p){p=defaultProfile();}
inline void flipProfile(ProfileData& p){
 sanitizeProfile(p);for(std::uint32_t i=0;i<p.count;i++)p.points[i].x=1-p.points[i].x;
 std::reverse(p.points.begin(),p.points.begin()+p.count);p.selected=p.count-1-p.selected;sanitizeProfile(p);
}
}
