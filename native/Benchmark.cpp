// Standalone timings of the unchanged production renderer. No AE or disk I/O in timed regions.
// Diagnostic passes materialize intermediates; their times are NOT an additive profile of renderPixels.
#include "CurveSmear.cpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

using Clock=std::chrono::steady_clock;
constexpr int W=1920,H=1080;
static volatile double observed=0;
static PF_Err noAbort(PF_ProgPtr){return PF_Err_NONE;}
template<class P> struct Image {
 std::vector<P> pixels;PF_EffectWorld world{};
 Image():pixels(W*H){world.width=W;world.height=H;world.rowbytes=W*sizeof(P);world.data=reinterpret_cast<PF_PixelPtr>(pixels.data());}
};
struct Stats {double first,median,p95;};
template<class F> static Stats measure(F fn,int repeats){
 auto once=[&](){auto start=Clock::now();fn();return std::chrono::duration<double,std::milli>(Clock::now()-start).count();};
 double first=once();std::vector<double> samples;for(int i=0;i<repeats;i++)samples.push_back(once());
 std::sort(samples.begin(),samples.end());return {first,samples[samples.size()/2],samples[static_cast<size_t>(std::ceil(samples.size()*.95))-1]};
}
static smear::Point curvePoint(int kind,double t){
 constexpr double pi=3.14159265358979323846;
 if(kind==0)return {700+500*t,540-100*std::sin(pi*t)};
 if(kind==1)return {200+1500*t,600-380*std::sin(pi*t)};
 if(kind==2)return {200+1500*t,540+300*std::sin(2*pi*t)};
 return {960+650*std::sin(2*pi*t),540+350*std::sin(4*pi*t)};
}
static smear::Curve makeCurve(int kind){
 // Dense analytic polyline resampled at approximately 3 px, like production's target spacing.
 // This measures local preparation only, not AE PathQuery/PathData callbacks.
 constexpr int dense=8192;std::vector<smear::Point> points;std::vector<double> lengths;
 points.push_back(curvePoint(kind,0));lengths.push_back(0);
 for(int i=1;i<=dense;i++){auto p=curvePoint(kind,double(i)/dense);auto prev=points.back();lengths.push_back(lengths.back()+std::hypot(p.x-prev.x,p.y-prev.y));points.push_back(p);}
 int count=std::clamp(static_cast<int>(std::ceil(lengths.back()/3)),16,2048);smear::Curve curve;auto prev=points[0];size_t at=1;
 for(int i=1;i<=count;i++){double distance=lengths.back()*i/count;while(at+1<lengths.size()&&lengths[at]<distance)at++;double t=(distance-lengths[at-1])/(lengths[at]-lengths[at-1]);smear::Point p{points[at-1].x+(points[at].x-points[at-1].x)*t,points[at-1].y+(points[at].y-points[at-1].y)*t};curve.add(prev,p);prev=p;}
 return curve;
}
struct Closest {double best=0,s=0,n=0;bool found=false;};
// Diagnostic copy of the search portion of Curve::map. End-to-end timings call production code.
static Closest search(const smear::Curve& curve,smear::Point p,const smear::Settings& c){
 Closest r;
 if(curve.segments.empty()||c.radius<=0||p.x<curve.left-c.radius||p.x>curve.right+c.radius||p.y<curve.top-c.radius||p.y>curve.bottom+c.radius)return r;
 r.best=c.radius*c.radius;
 for(const auto& g:curve.segments){double rx=p.x-g.p.x,ry=p.y-g.p.y,t=std::clamp((rx*g.dx+ry*g.dy)/(g.len*g.len),0.,1.);double ex=rx-t*g.dx,ey=ry-t*g.dy,d=ex*ex+ey*ey;if(d<r.best){r.best=d;r.s=g.s+t*g.len;r.n=(g.dx*ry-g.dy*rx)/g.len;r.found=true;}}
 return r;
}
static smear::Mapping finish(const smear::Curve& curve,smear::Point p,const smear::Settings& c,const Closest& r){
 smear::Mapping result{p,0,.5};double s=r.s,n=r.n;
 if(!r.found||c.flat&&(s<=1e-3||s>=curve.length-1e-3))return result;
 double m=c.feather<=0?1.:std::clamp((c.radius-std::sqrt(r.best))/(c.radius*c.feather),0.,1.);if(c.feather>0)m=m*m*(3-2*m);
 result.influence=m;result.profile_position=std::clamp((1-n/c.radius)/2,0.,1.);if(c.amount<=0||c.original>=1)return result;
 double v=n/std::max(20.,c.radius)*c.frequency+c.seed*1.61803398875;
 double noise=.5+.24*std::sin(v*1.17+1.4)+.16*std::sin(v*2.71+.3)+.10*std::sin(v*5.39+2.1);
 double travel=c.reverse?curve.length-s:s;
 double shift=travel*c.amount/(curve.length+c.amount)*m*(1-c.streak*.85*noise)*smear::profileGain(c,result.profile_position);
 auto a=curve.frame(s,n),b=curve.frame(s+(c.reverse?shift:-shift),n);result.source={p.x+b.x-a.x,p.y+b.y-a.y};return result;
}
struct Case {const char* name;int kind;double radius;bool nearest,matte;int bpc;bool zero=false;};
static void emit(std::ostream& file,const Case& c,const char* phase,size_t segments,size_t bbox,size_t affected,Stats s,int repeats){
 file<<c.name<<','<<c.bpc<<','<<c.radius<<','<<(c.nearest?"Nearest":"Linear")<<','<<c.matte<<','<<segments<<','<<bbox<<','<<affected<<','<<phase<<','<<repeats<<','<<s.first<<','<<s.median<<','<<s.p95<<'\n';file.flush();
}
template<class P> static void run(const Case& c,int repeats,std::ostream& file){
 Image<P> input,matte,output,replayed;using Channel=decltype(P{}.alpha);double max=std::is_floating_point_v<Channel>?1.:sizeof(Channel)==1?255.:32768.;
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){auto i=y*W+x;auto& p=input.pixels[i];double a=(x%137<100)?1.:.5;p={channel<Channel>(max*a),channel<Channel>(max*a*(x%256)/255.),channel<Channel>(max*a*(y%256)/255.),channel<Channel>(max*a*.25)};double m=(x/120)%2?1.:0;matte.pixels[i]={channel<Channel>(max),channel<Channel>(max*m),channel<Channel>(max*m),channel<Channel>(max*m)};}
 Data d;d.curve=makeCurve(c.kind);d.settings.radius=c.radius;d.settings.amount=c.zero?0:250;d.settings.nearest=c.nearest;d.settings.flat=true;smear::prepareProfile(d.settings);
 PF_InData in{};in.downsample_x={1,1};in.downsample_y={1,1};in.inter.abort=noAbort;const auto* mw=c.matte?&matte.world:nullptr;
 auto total=measure([&](){if(renderPixels<P>(&in,&input.world,&output.world,d,true,mw))throw std::runtime_error("render failed");observed=output.pixels[W*H/2].red;},repeats);
 size_t bbox=0,affected=0;for(int y=0;y<H;y++)for(int x=0;x<W;x++)if(x>=d.curve.left-c.radius&&x<=d.curve.right+c.radius&&y>=d.curve.top-c.radius&&y<=d.curve.bottom+c.radius)bbox++;
 if(c.zero){emit(file,c,"production_render",d.curve.segments.size(),bbox,0,total,repeats);std::cout<<c.name<<": "<<total.median<<" ms\n"<<std::flush;return;}
 std::vector<Closest> closest(W*H);std::vector<smear::Mapping> mappings(W*H);
 auto searchTime=measure([&](){for(int y=0;y<H;y++)for(int x=0;x<W;x++)closest[y*W+x]=search(d.curve,{double(x),double(y)},d.settings);observed=closest[W*H/2].best;},repeats);
 auto finishTime=measure([&](){for(int y=0;y<H;y++)for(int x=0;x<W;x++)mappings[y*W+x]=finish(d.curve,{double(x),double(y)},d.settings,closest[y*W+x]);observed=mappings[W*H/2].source.x;},repeats);
 for(const auto& m:mappings)if(m.influence>0)affected++;
 // Validate the diagnostic split against the current production mapping at EVERY pixel.
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){auto a=d.curve.map({double(x),double(y)},d.settings),b=mappings[y*W+x];if(a.source.x!=b.source.x||a.source.y!=b.source.y||a.influence!=b.influence||a.profile_position!=b.profile_position)throw std::runtime_error("Diagnostic mapping differs from production");}
 auto sampleTime=measure([&](){for(int y=0;y<H;y++)for(int x=0;x<W;x++){int i=y*W+x;auto orig=input.pixels[i];replayed.pixels[i]=orig;const auto& m=mappings[i];if(m.influence<=0)continue;if(m.source.x!=x||m.source.y!=y){double coverage=matteCoverage<P>(mw,m.source.x,m.source.y,c.nearest,false),keep=1-(1-d.settings.original)*coverage;replayed.pixels[i]=c.nearest?nearest<P>(&input.world,m.source.x,m.source.y,orig,keep):bilinear<P>(&input.world,m.source.x,m.source.y,orig,keep);}}observed=replayed.pixels[W*H/2].red;},repeats);
 if(std::memcmp(output.pixels.data(),replayed.pixels.data(),W*H*sizeof(P)))throw std::runtime_error("Diagnostic pixels differ from production");
 auto preparation=measure([&](){auto curve=makeCurve(c.kind);auto settings=d.settings;smear::prepareProfile(settings);observed=curve.length+settings.profile_lut[128];},repeats);
 emit(file,c,"production_render",d.curve.segments.size(),bbox,affected,total,repeats);
 emit(file,c,"diagnostic_search",d.curve.segments.size(),bbox,affected,searchTime,repeats);
 emit(file,c,"diagnostic_finish",d.curve.segments.size(),bbox,affected,finishTime,repeats);
 emit(file,c,"diagnostic_sample",d.curve.segments.size(),bbox,affected,sampleTime,repeats);
 emit(file,c,"synthetic_prepare",d.curve.segments.size(),bbox,affected,preparation,repeats);
 std::cout<<c.name<<": total="<<total.median<<" search="<<searchTime.median<<" finish="<<finishTime.median<<" sample="<<sampleTime.median<<" ms; exact diagnostic equality PASS\n"<<std::flush;
}
int main(int argc,char** argv){
 try{if(argc>3){int cpu=std::atoi(argv[3]);if(cpu<0||cpu>=64||!SetThreadAffinityMask(GetCurrentThread(),DWORD_PTR(1)<<cpu))throw std::runtime_error("CPU affinity failed");std::cout<<"Benchmark thread pinned to logical CPU "<<cpu<<'\n';}
 int repeats=argc>2?std::max(3,std::atoi(argv[2])):9;std::ofstream file(argc>1?argv[1]:"benchmark.csv");if(!file)throw std::runtime_error("Cannot create CSV");file<<std::setprecision(9)<<"case,bpc,radius,sampling,matte,segments,bbox_pixels,influenced_pixels,phase,repeats,first_ms,median_ms,p95_ms\n";
 const Case cases[]={
 {"zero_8",0,65,true,false,8,true},
 {"short_8_nearest",0,65,true,false,8},
 {"short_8_linear",0,65,false,false,8},
 {"short_8_nearest_matte",0,65,true,true,8},
 {"short_8_linear_matte",0,65,false,true,8},
 {"short_16_nearest_matte",0,65,true,true,16},
 {"short_32_nearest_matte",0,65,true,true,32},
 {"short_32_linear_matte",0,65,false,true,32},
 {"short_wide_8",0,250,true,false,8},
 {"long_8",1,65,true,false,8},
 {"long_wide_8",1,250,true,false,8},
 {"s_curve_8",2,65,true,false,8},
 {"crossing_8",3,65,true,false,8}};
 for(const auto& c:cases){if(argc>4&&std::string(argv[4]).find(std::string(",")+c.name+",")==std::string::npos)continue;if(c.bpc==8)run<PF_Pixel8>(c,repeats,file);else if(c.bpc==16)run<PF_Pixel16>(c,repeats,file);else run<PF_PixelFloat>(c,repeats,file);}return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
