#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace smear {
struct Point { double x=0,y=0; };
struct Segment { Point p; double dx,dy,len,s; };
struct Settings { double amount=250,radius=65,feather=.55,streak=.65,frequency=17,original=0,seed=0; bool reverse=false,preview=false; };
struct Mapping { Point source; double influence=0; };
class Curve {
public:
 std::vector<Segment> segments;
 double length=0,left=0,top=0,right=0,bottom=0;
 void add(Point a,Point b) {
  double dx=b.x-a.x,dy=b.y-a.y,len=std::hypot(dx,dy);if(len<1e-7)return;
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
  Mapping result{p,0};
  if(segments.empty()||c.radius<=0||p.x<left-c.radius||p.x>right+c.radius||p.y<top-c.radius||p.y>bottom+c.radius)return result;
  double best=c.radius*c.radius,s=0,n=0;bool found=false;
  for(const auto& g:segments){
   double rx=p.x-g.p.x,ry=p.y-g.p.y,t=std::clamp((rx*g.dx+ry*g.dy)/(g.len*g.len),0.,1.);
   double ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;
   if(d<best){best=d;s=g.s+t*g.len;n=(g.dx*ry-g.dy*rx)/g.len;found=true;}
  }
  if(!found)return result;
  double m=c.feather<=0?1.:std::clamp((c.radius-std::sqrt(best))/(c.radius*c.feather),0.,1.);
  if(c.feather>0)m=m*m*(3-2*m);
  result.influence=m;if(c.amount<=0||c.original>=1)return result;
  double v=n/std::max(20.,c.radius)*c.frequency+c.seed*1.61803398875;
  double noise=.5+.24*std::sin(v*1.17+1.4)+.16*std::sin(v*2.71+.3)+.10*std::sin(v*5.39+2.1);
  double travel=c.reverse?length-s:s;
  double shift=travel*c.amount/(length+c.amount)*m*(1-c.streak*.85*noise);
  Point a=frame(s,n),b=frame(s+(c.reverse?shift:-shift),n);
  result.source={p.x+b.x-a.x,p.y+b.y-a.y};return result;
 }
};
}
