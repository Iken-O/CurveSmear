#include "CurveSmear.cpp"
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>

static PF_Err noAbort(PF_ProgPtr){return PF_Err_NONE;}
template<class P> struct Image {
 int w,h,stride; std::vector<P> pixels; PF_EffectWorld world{};
 Image(int width,int height,int padding=3):w(width),h(height),stride(width+padding),pixels(stride*height){world.width=w;world.height=h;world.rowbytes=stride*sizeof(P);world.data=reinterpret_cast<PF_PixelPtr>(pixels.data());}
 P& at(int x,int y){return pixels[y*stride+x];}
};
static Data testData(){
 Data d;smear::Point prev{365,220};
 for(int k=1;k<=100;k++){double t=k/100.,u=1-t;smear::Point p{u*u*u*365+3*u*u*t*460+3*u*t*t*540+t*t*t*700,u*u*u*220+3*u*u*t*190+3*u*t*t*350+t*t*t*310};d.curve.add(prev,p);prev=p;}
 return d;
}
template<class P> static void invariants(){
 Image<P> input(800,520),output(800,520);using C=decltype(P{}.alpha);
 const double max=std::is_floating_point_v<C>?1.:sizeof(C)==1?255.:32768.;
 for(int y=0;y<input.h;y++)for(int x=0;x<input.w;x++){auto& p=input.at(x,y);p.alpha=channel<C>(max);p.red=channel<C>(max*x/800);p.green=channel<C>(max*y/520);p.blue=channel<C>(max*.3);}
 PF_InData in{};in.downsample_x={1,1};in.downsample_y={1,1};in.inter.abort=noAbort;
 auto d=testData();assert(renderPixels<P>(&in,&input.world,&output.world,d,true)==0);
 int changed=0;for(int y=0;y<input.h;y++)for(int x=0;x<input.w;x++){bool equal=std::memcmp(&input.at(x,y),&output.at(x,y),sizeof(P))==0;auto m=d.curve.map({double(x),double(y)},d.settings);if(!m.influence)assert(equal);if(!equal)changed++;}assert(changed>1000);
 d.settings.amount=0;renderPixels<P>(&in,&input.world,&output.world,d,true);
 for(int y=0;y<input.h;y++)for(int x=0;x<input.w;x++)assert(std::memcmp(&input.at(x,y),&output.at(x,y),sizeof(P))==0);
 d.settings.amount=250;d.settings.original=1;renderPixels<P>(&in,&input.world,&output.world,d,true);
 for(int y=0;y<input.h;y++)for(int x=0;x<input.w;x++)assert(std::memcmp(&input.at(x,y),&output.at(x,y),sizeof(P))==0);
 d.settings.original=0;renderPixels<P>(&in,&input.world,&output.world,d,true);
 Image<P> tile(140,130);tile.world.origin_x=390;tile.world.origin_y=200;renderPixels<P>(&in,&input.world,&tile.world,d,true);
 for(int y=0;y<tile.h;y++)for(int x=0;x<tile.w;x++)assert(std::memcmp(&tile.at(x,y),&output.at(x+390,y+200),sizeof(P))==0);
 // Source checkout with non-zero origin and padding must address the same pixels.
 Image<P> cropped(700,450);cropped.world.origin_x=100;cropped.world.origin_y=50;
 for(int y=0;y<cropped.h;y++)for(int x=0;x<cropped.w;x++)cropped.at(x,y)=input.at(x+100,y+50);
 renderPixels<P>(&in,&cropped.world,&tile.world,d,true);
 for(int y=0;y<tile.h;y++)for(int x=0;x<tile.w;x++)assert(std::memcmp(&tile.at(x,y),&output.at(x+390,y+200),sizeof(P))==0);
 // Half-size previews use full-resolution path coordinates and radius.
 Image<P> half(400,260),halfOut(400,260);for(int y=0;y<260;y++)for(int x=0;x<400;x++)half.at(x,y)=input.at(x*2,y*2);
 in.downsample_x={1,2};in.downsample_y={1,2};renderPixels<P>(&in,&half.world,&halfOut.world,d,true);
 for(int y=0;y<260;y++)for(int x=0;x<400;x++){if(!d.curve.map({double(x*2),double(y*2)},d.settings).influence)assert(std::memcmp(&half.at(x,y),&halfOut.at(x,y),sizeof(P))==0);}
 std::cout<<"PASS "<<sizeof(P)*8<<"-bit pixel: outside radius, zero, original, tiled output, cropped input, downsample\n";
}
int main(int argc,char** argv){
 invariants<PF_Pixel8>();invariants<PF_Pixel16>();invariants<PF_PixelFloat>();
 Image<PF_PixelFloat> hdr(2,2);hdr.at(0,0)={1,4,-.5f,2};hdr.at(1,0)={0,0,0,0};
 auto sampleFloat=bilinear<PF_PixelFloat>(&hdr.world,.5,0,{},0);assert(sampleFloat.alpha==.5f&&sampleFloat.red==2&&sampleFloat.green==-.25f&&sampleFloat.blue==1);
 Image<PF_Pixel16> deep(2,2);deep.at(0,0)={32768,32768,0,0};auto q=bilinear<PF_Pixel16>(&deep.world,.5,0,{},0);assert(q.alpha==16384&&q.red==16384);
 assert(VERSION==40965);assert(FLAGS==0x02008000);assert(FLAGS2==0x08201400);
 auto d=testData();auto a=d.curve.map({470,235},d.settings);d.settings.reverse=true;auto b=d.curve.map({470,235},d.settings);assert(a.source.x!=b.source.x);
 auto profile=smear::defaultProfile();for(double u:{0.,.25,.5,.75,1.})assert(smear::profileValue(profile,u)==1);
 profile.points[0].y=.01f;assert(std::abs(smear::profileValue(profile,0)-.01)<1e-6);assert(std::abs(smear::profileValue(profile,.5)-.505)<1e-6);assert(smear::profileValue(profile,1)==1);
 smear::flipProfile(profile);assert(profile.points[0].y==1&&std::abs(profile.points[1].y-.01f)<1e-6);smear::resetProfile(profile);assert(profile.count==2&&profile.points[0].y==1&&profile.points[1].y==1);
 smear::Curve straight;straight.add({0,100},{100,100});smear::Settings tapered;tapered.amount=80;tapered.radius=20;tapered.feather=0;tapered.streak=0;tapered.profile.points[0].y=0;tapered.profile.points[1].y=1;smear::prepareProfile(tapered);
 auto weak=straight.map({80,110},tapered),strong=straight.map({80,90},tapered);assert(std::abs(strong.source.x-80)>2.5*std::abs(weak.source.x-80));
 tapered.profile.points[0].y=tapered.profile.points[1].y=0;smear::prepareProfile(tapered);auto stopped=straight.map({80,90},tapered);assert(stopped.source.x==80&&stopped.source.y==90);
 auto cap=testData();auto roundCap=cap.curve.map({350,225},cap.settings);assert(roundCap.influence>0);cap.settings.flat=true;assert(cap.curve.map({350,225},cap.settings).influence==0);
 Image<PF_Pixel8> binary(2,1,0);binary.at(0,0)={255,0,0,0};binary.at(1,0)={255,255,255,255};auto linearMid=bilinear<PF_Pixel8>(&binary.world,.45,0,{},0),nearestMid=nearest<PF_Pixel8>(&binary.world,.45,0,{},0);assert(linearMid.red>0&&linearMid.red<255);assert(nearestMid.red==0);
 assert(matteCoverage<PF_Pixel8>(&binary.world,0,0,true,false,false)==0);assert(std::abs(matteCoverage<PF_Pixel8>(&binary.world,1,0,true,false,false)-1)<1e-9);assert(matteCoverage<PF_Pixel8>(&binary.world,0,0,true,true,false)==1);assert(matteCoverage<PF_Pixel8>(&binary.world,0,0,true,false,true)==1);
 Image<PF_Pixel8> matteSource(120,50,0),blackMatte(120,50,0),whiteMatte(120,50,0),matteOut(120,50,0);for(int y=0;y<50;y++)for(int x=0;x<120;x++){matteSource.at(x,y)={255,static_cast<A_u_char>(x*2),0,0};blackMatte.at(x,y)={255,0,0,0};whiteMatte.at(x,y)={255,255,255,255};}
 Data matteData;matteData.curve.add({10,25},{110,25});matteData.settings.amount=80;matteData.settings.radius=12;matteData.settings.feather=0;matteData.settings.streak=0;matteData.settings.nearest=true;PF_InData matteIn{};matteIn.downsample_x={1,1};matteIn.downsample_y={1,1};matteIn.inter.abort=noAbort;
 renderPixels<PF_Pixel8>(&matteIn,&matteSource.world,&matteOut.world,matteData,true,&blackMatte.world);for(int y=0;y<50;y++)for(int x=0;x<120;x++)assert(std::memcmp(&matteSource.at(x,y),&matteOut.at(x,y),sizeof(PF_Pixel8))==0);
 renderPixels<PF_Pixel8>(&matteIn,&matteSource.world,&matteOut.world,matteData,true,&whiteMatte.world);int matteChanged=0;for(int y=0;y<50;y++)for(int x=0;x<120;x++)if(std::memcmp(&matteSource.at(x,y),&matteOut.at(x,y),sizeof(PF_Pixel8))!=0)matteChanged++;assert(matteChanged>100);
 std::cout<<"PASS HDR, premultiplied alpha, AE 16bpc, PiPL flags/version, reverse, Flat cap, width profile mapping/reset/flip, nearest sampling, source matte\n";
 if(argc==3){
  Image<PF_Pixel8> source(800,520),dest(800,520);std::ifstream file(argv[1],std::ios::binary);std::vector<unsigned char> rgba(800*520*4);file.read(reinterpret_cast<char*>(rgba.data()),rgba.size());assert(file.gcount()==static_cast<std::streamsize>(rgba.size()));
  for(int y=0;y<520;y++)for(int x=0;x<800;x++){int i=(y*800+x)*4;double alpha=rgba[i+3]/255.;source.at(x,y)={rgba[i+3],channel<A_u_char>(rgba[i]*alpha),channel<A_u_char>(rgba[i+1]*alpha),channel<A_u_char>(rgba[i+2]*alpha)};}
  PF_InData in{};in.downsample_x={1,1};in.downsample_y={1,1};in.inter.abort=noAbort;auto data=testData();auto start=std::chrono::steady_clock::now();renderPixels<PF_Pixel8>(&in,&source.world,&dest.world,data,true);
  std::cout<<"800x520 kernel: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<" ms\n";
  for(int y=0;y<520;y++)for(int x=0;x<800;x++){int i=(y*800+x)*4;auto p=dest.at(x,y);double scale=p.alpha?255./p.alpha:0;rgba[i]=channel<A_u_char>(p.red*scale);rgba[i+1]=channel<A_u_char>(p.green*scale);rgba[i+2]=channel<A_u_char>(p.blue*scale);rgba[i+3]=p.alpha;}
  std::ofstream out(argv[2],std::ios::binary);out.write(reinterpret_cast<char*>(rgba.data()),rgba.size());
 }
}
