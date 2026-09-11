#define PF_DEEP_COLOR_AWARE 1
#define NOMINMAX
#include "AEConfig.h"
#include "AE_EffectVers.h"
#include <Windows.h>
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_Macros.h"
#include "AE_PluginData.h"
#include "Param_Utils.h"
#include "SmearCore.h"
#include "CurveSmearUI.h"
#include <array>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

constexpr A_long FLAGS=PF_OutFlag_DEEP_COLOR_AWARE|PF_OutFlag_CUSTOM_UI;
constexpr A_long FLAGS2=PF_OutFlag2_SUPPORTS_SMART_RENDER|PF_OutFlag2_FLOAT_COLOR_AWARE|PF_OutFlag2_I_MIX_GUID_DEPENDENCIES|PF_OutFlag2_SUPPORTS_THREADED_RENDERING;
constexpr A_u_long VERSION=PF_VERSION(0,1,3,PF_Stage_DEVELOP,4);
static void check(PF_Err e){if(e)throw e;}
static A_char* paramName(PF_ParamDef& d){
#if PF_PLUG_IN_SUBVERS >= 29
 return d.PF_DEF_NAME;
#else
 return d.name;
#endif
}
template<class T> class Suite {
 SPBasicSuite* basic; const char* name; A_long version;
public:
 const T* p=nullptr;
 Suite(PF_InData* in,const char* n,A_long v):basic(in->pica_basicP),name(n),version(v){check(basic->AcquireSuite(n,v,reinterpret_cast<const void**>(&p)));}
 ~Suite(){if(p)basic->ReleaseSuite(name,version);}
 const T* operator->()const{return p;}
 Suite(const Suite&)=delete;
};
struct Data { smear::Curve curve; smear::Settings settings; };
class Params {
 PF_InData* in;int acquired=0;
public:
 std::array<PF_ParamDef,COUNT> p{};
 explicit Params(PF_InData* i):in(i){try{for(int k=1;k<=LAST_RENDER_PARAM;k++){check(PF_CHECKOUT_PARAM(in,k,in->current_time,in->time_step,in->time_scale,&p[k]));acquired=k;}}catch(...){release();throw;}}
 void release(){for(int k=1;k<=acquired;k++)PF_CHECKIN_PARAM(in,&p[k]);acquired=0;}
 ~Params(){release();}
};
static void loadPath(PF_InData* in,PF_OutData* out,PF_PathID id,smear::Curve& curve){
 if(!id)return;
 Suite<PF_PathQuerySuite1> query(in,kPFPathQuerySuite,kPFPathQuerySuiteVersion1);
 Suite<PF_PathDataSuite1> data(in,kPFPathDataSuite,kPFPathDataSuiteVersion1);
 PF_PathOutlinePtr path=nullptr;check(query->PF_CheckoutPath(in->effect_ref,id,in->current_time,in->time_step,in->time_scale,&path));
 if(!path)return;
 try {
  PF_Boolean open=false;check(data->PF_PathIsOpen(in->effect_ref,path,&open));
  if(!open){std::strcpy(out->return_msg,"CurveSmear: select an open mask path (Mask Mode: None). Closed paths pass through unchanged.");}
  else {
   A_long count=0;check(data->PF_PathNumSegments(in->effect_ref,path,&count));
   for(A_long j=0;j<count;j++){
    PF_PathSegPrepPtr prep=nullptr;
    check(data->PF_PathPrepareSegLength(in->effect_ref,path,j,100,&prep));
    try{
     PF_FpLong len=0;check(data->PF_PathGetSegLength(in->effect_ref,path,j,&prep,&len));
     // Uniform arc-length samples: bounded chord length at full layer resolution.
     int samples=std::clamp(static_cast<int>(std::ceil(len/3.)),16,2048);
     smear::Point prev;check(data->PF_PathEvalSegLength(in->effect_ref,path,&prep,j,0,&prev.x,&prev.y));
     for(int k=1;k<=samples;k++){smear::Point p;check(data->PF_PathEvalSegLength(in->effect_ref,path,&prep,j,len*k/samples,&p.x,&p.y));curve.add(prev,p);prev=p;}
    }catch(...){data->PF_PathCleanupSegLength(in->effect_ref,path,j,&prep);throw;}
    check(data->PF_PathCleanupSegLength(in->effect_ref,path,j,&prep));
   }
  }
 }catch(...){query->PF_CheckinPath(in->effect_ref,id,FALSE,path);throw;}
 check(query->PF_CheckinPath(in->effect_ref,id,FALSE,path));
}
static Data readData(PF_InData* in,PF_OutData* out,PF_ParamDef* p[]){
 Data d;auto& s=d.settings;
 s.amount=std::max(0.,p[AMOUNT]->u.fs_d.value);s.radius=std::max(0.,p[RADIUS]->u.fs_d.value);
 s.feather=std::clamp(p[FEATHER]->u.fs_d.value/100,0.,1.);s.streak=std::clamp(p[STREAK]->u.fs_d.value/100,0.,1.);
 s.frequency=std::max(1.,p[FREQUENCY]->u.fs_d.value);s.original=std::clamp(p[ORIGINAL]->u.fs_d.value/100,0.,1.);
 s.reverse=p[REVERSE]->u.bd.value!=0;s.seed=p[SEED]->u.sd.value;s.preview=p[PREVIEW]->u.bd.value!=0;
 s.flat=p[END_CAP]->u.pd.value==2;s.nearest=p[SAMPLING]->u.pd.value==2;
 if(auto h=p[PROFILE]->u.arb_d.value){Suite<PF_HandleSuite1> handles(in,kPFHandleSuite,kPFHandleSuiteVersion1);auto profile=static_cast<const smear::ProfileData*>(handles->host_lock_handle(h));if(!profile)throw PF_Err_OUT_OF_MEMORY;s.profile=*profile;handles->host_unlock_handle(h);smear::sanitizeProfile(s.profile);}
 s.profile.smooth=p[PROFILE_SMOOTH]->u.bd.value?1u:0u;
 smear::prepareProfile(s);
 loadPath(in,out,p[PATH]->u.path_d.path_id,d.curve);return d;
}
static PF_Err setup(PF_InData* in_data,PF_OutData* out_data){
 PF_ParamDef def{};def.param_type=PF_Param_PATH;def.uu.id=PATH;std::strcpy(paramName(def),"Flow Path (open mask)");def.u.path_d.dephault=0;
 check(PF_ADD_PARAM(in_data,-1,&def));
 PF_ADD_FLOAT_SLIDERX("Smear Amount",0,10000,0,1000,250,1,0,0,AMOUNT);
 PF_ADD_FLOAT_SLIDERX("Radius",0,5000,0,500,65,1,0,0,RADIUS);
 PF_ADD_FLOAT_SLIDERX("Edge Feather",0,100,0,100,55,1,PF_ValueDisplayFlag_PERCENT,0,FEATHER);
 PF_ADD_FLOAT_SLIDERX("Streak Strength",0,100,0,100,65,1,PF_ValueDisplayFlag_PERCENT,0,STREAK);
 PF_ADD_FLOAT_SLIDERX("Streak Frequency",1,100,1,35,17,1,0,0,FREQUENCY);
 PF_ADD_FLOAT_SLIDERX("Keep Original",0,100,0,100,0,1,PF_ValueDisplayFlag_PERCENT,0,ORIGINAL);
 PF_ADD_CHECKBOXX("Reverse Flow",FALSE,0,REVERSE);
 AEFX_CLR_STRUCT(def);PF_ADD_SLIDER("Seed",0,100000,0,1000,0,SEED);
 PF_ADD_CHECKBOXX("Show Influence (rendered)",FALSE,0,PREVIEW);
 // Existing projects keep the legacy Round/Linear behavior; new instances default to Flat/Nearest.
 AEFX_CLR_STRUCT(def);def.param_type=PF_Param_POPUP;std::strcpy(paramName(def),"End Caps");def.u.pd.num_choices=2;def.u.pd.value=1;def.u.pd.dephault=2;def.u.pd.u.namesptr="Round|Flat";def.flags=PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;def.uu.id=END_CAP;check(PF_ADD_PARAM(in_data,-1,&def));
 AEFX_CLR_STRUCT(def);def.param_type=PF_Param_POPUP;std::strcpy(paramName(def),"Sampling");def.u.pd.num_choices=2;def.u.pd.value=1;def.u.pd.dephault=2;def.u.pd.u.namesptr="Linear|Nearest";def.flags=PF_ParamFlag_USE_VALUE_FOR_OLD_PROJECTS;def.uu.id=SAMPLING;check(PF_ADD_PARAM(in_data,-1,&def));
 PF_ArbitraryH profileDefault=nullptr;check(CreateDefaultProfile(in_data,&profileDefault));AEFX_CLR_STRUCT(def);
 PF_ADD_ARBITRARY2("Width Profile",PROFILE_UI_WIDTH,PROFILE_UI_HEIGHT,PF_ParamFlag_CANNOT_TIME_VARY,PF_PUI_CONTROL|PF_PUI_DONT_ERASE_CONTROL,profileDefault,PROFILE,PROFILE_REFCON);
 PF_ADD_CHECKBOXX("Smooth Profile",TRUE,PF_ParamFlag_CANNOT_TIME_VARY|PF_ParamFlag_SUPERVISE,PROFILE_SMOOTH);
 PF_ADD_BUTTON("Profile", "Reset", PF_PUI_NONE, PF_ParamFlag_SUPERVISE, PROFILE_RESET);
 PF_ADD_BUTTON("Profile", "Swap L/R", PF_PUI_NONE, PF_ParamFlag_SUPERVISE, PROFILE_FLIP);
 PF_ADD_BUTTON("Profile", "Delete Selected Point", PF_PUI_NONE, PF_ParamFlag_SUPERVISE, PROFILE_DELETE);
 PF_CustomUIInfo ui{};ui.events=PF_CustomEFlag_EFFECT;ui.comp_ui_alignment=ui.layer_ui_alignment=ui.preview_ui_alignment=PF_UIAlignment_NONE;check(in_data->inter.register_ui(in_data->effect_ref,&ui));
 out_data->num_params=COUNT;return PF_Err_NONE;
}
static void deleteData(void* p){delete static_cast<Data*>(p);}
static PF_Err preRender(PF_InData* in,PF_OutData* out,PF_PreRenderExtra* extra){
 Params params(in);std::array<PF_ParamDef*,COUNT> p{};for(int k=1;k<=LAST_RENDER_PARAM;k++)p[k]=&params.p[k];
 auto d=std::make_unique<Data>(readData(in,out,p.data()));
 // Path geometry participates in the render cache, including animation of None masks.
 // I_MIX_GUID_DEPENDENCIES requires at least one mix call on every pre-render,
 // including the initial state before a mask has been selected.
 struct PathDependencyHeader { A_u_long schema; A_u_long segment_count; };
 const PathDependencyHeader dependency{1,static_cast<A_u_long>(d->curve.segments.size())};
 if(!extra->cb->GuidMixInPtr)return PF_Err_BAD_CALLBACK_PARAM;
 check(extra->cb->GuidMixInPtr(in->effect_ref,sizeof(dependency),&dependency));
 if(!d->curve.segments.empty())check(extra->cb->GuidMixInPtr(in->effect_ref,static_cast<A_u_long>(d->curve.segments.size()*sizeof(smear::Segment)),d->curve.segments.data()));
 PF_RenderRequest req=extra->input->output_request;
 const A_long width=static_cast<A_long>(std::ceil(in->width*double(in->downsample_x.num)/in->downsample_x.den));
 const A_long height=static_cast<A_long>(std::ceil(in->height*double(in->downsample_y.num)/in->downsample_y.den));
 req.rect={0,0,width,height};req.channel_mask=PF_ChannelMask_ARGB;req.preserve_rgb_of_zero_alpha=TRUE;
 PF_CheckoutResult result{};check(extra->cb->checkout_layer(in->effect_ref,0,0,&req,in->current_time,in->time_step,in->time_scale,&result));
 extra->output->max_result_rect={0,0,width,height};
 const auto& requested=extra->input->output_request.rect;
 extra->output->result_rect={std::max<A_long>(0,requested.left),std::max<A_long>(0,requested.top),std::min(width,requested.right),std::min(height,requested.bottom)};
 if(extra->output->result_rect.right<extra->output->result_rect.left||extra->output->result_rect.bottom<extra->output->result_rect.top)extra->output->result_rect={0,0,0,0};
 extra->output->pre_render_data=d.release();extra->output->delete_pre_render_data_func=deleteData;return PF_Err_NONE;
}
template<class Pixel> static Pixel get(const PF_EffectWorld* w,int x,int y){
 if(!w||!w->data||x<0||y<0||x>=w->width||y>=w->height)return {};
 return reinterpret_cast<const Pixel*>(reinterpret_cast<const char*>(w->data)+y*w->rowbytes)[x];
}
template<class Channel> static Channel channel(double v){
 if constexpr(std::is_floating_point_v<Channel>)return static_cast<Channel>(v);
 else return static_cast<Channel>(std::clamp(std::round(v),0.,sizeof(Channel)==1?255.:32768.));
}
template<class Pixel> static Pixel bilinear(const PF_EffectWorld* w,double x,double y,Pixel original,double mix){
 double a=0,r=0,g=0,b=0;
 if(std::isfinite(x)&&std::isfinite(y)&&x>-2&&y>-2&&x<w->width+1&&y<w->height+1){
  int ix=static_cast<int>(std::floor(x)),iy=static_cast<int>(std::floor(y));double fx=x-ix,fy=y-iy;
  // AE worlds already contain premultiplied RGB; interpolate once, including alpha.
  for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++){Pixel p=get<Pixel>(w,ix+xx,iy+yy);double weight=(xx?fx:1-fx)*(yy?fy:1-fy);a+=p.alpha*weight;r+=p.red*weight;g+=p.green*weight;b+=p.blue*weight;}
 }
 Pixel result;using C=decltype(result.alpha);
 result.alpha=channel<C>(a*(1-mix)+original.alpha*mix);result.red=channel<C>(r*(1-mix)+original.red*mix);
 result.green=channel<C>(g*(1-mix)+original.green*mix);result.blue=channel<C>(b*(1-mix)+original.blue*mix);return result;
}
template<class Pixel> static Pixel nearest(const PF_EffectWorld* w,double x,double y,Pixel original,double mix){
 Pixel p=get<Pixel>(w,static_cast<int>(std::round(x)),static_cast<int>(std::round(y))),result;using C=decltype(result.alpha);
 result.alpha=channel<C>(p.alpha*(1-mix)+original.alpha*mix);result.red=channel<C>(p.red*(1-mix)+original.red*mix);result.green=channel<C>(p.green*(1-mix)+original.green*mix);result.blue=channel<C>(p.blue*(1-mix)+original.blue*mix);return result;
}
template<class Pixel> static PF_Err renderPixels(PF_InData* in,const PF_EffectWorld* input,PF_EffectWorld* output,const Data& data,bool smart){
 const double sx=double(in->downsample_x.num)/in->downsample_x.den,sy=double(in->downsample_y.num)/in->downsample_y.den;
 int ix=smart?input->origin_x:0,iy=smart?input->origin_y:0,ox=smart?output->origin_x:0,oy=smart?output->origin_y:0;
 const auto& c=data.settings;
 for(int y=0;y<output->height;y++){
  if((y&31)==0){PF_Err e=PF_ABORT(in);if(e)return e;}
  auto row=reinterpret_cast<Pixel*>(reinterpret_cast<char*>(output->data)+y*output->rowbytes);
  for(int x=0;x<output->width;x++){
   Pixel original=get<Pixel>(input,x+ox-ix,y+oy-iy);row[x]=original;
   if(!c.preview&&(c.amount<=0||c.original>=1||c.radius<=0))continue;
   smear::Point pos{(x+ox)/sx,(y+oy)/sy};auto mapped=data.curve.map(pos,c);
   if(mapped.influence<=0)continue;
   if(c.preview){using C=decltype(row[x].alpha);double max=std::is_floating_point_v<C>?1.:sizeof(C)==1?255.:32768.,u=mapped.profile_position,m=mapped.influence*.35,rr=.96*(1-u)+.48*u,gg=.66*(1-u)+.72*u,bb=.35*(1-u)+u;row[x].red=channel<C>(original.red*(1-m)+max*rr*m);row[x].green=channel<C>(original.green*(1-m)+max*gg*m);row[x].blue=channel<C>(original.blue*(1-m)+max*bb*m);row[x].alpha=channel<C>(original.alpha*(1-m)+max*m);}
   else if(mapped.source.x!=pos.x||mapped.source.y!=pos.y)row[x]=c.nearest?nearest<Pixel>(input,mapped.source.x*sx-ix,mapped.source.y*sy-iy,original,c.original):bilinear<Pixel>(input,mapped.source.x*sx-ix,mapped.source.y*sy-iy,original,c.original);
  }
 }return PF_Err_NONE;
}
static PF_Err render(PF_InData* in,PF_EffectWorld* input,PF_EffectWorld* output,const Data& d,bool smart){
 Suite<PF_WorldSuite2> world(in,kPFWorldSuite,kPFWorldSuiteVersion2);PF_PixelFormat format;check(world->PF_GetPixelFormat(output,&format));
 switch(format){case PF_PixelFormat_ARGB32:return renderPixels<PF_Pixel8>(in,input,output,d,smart);case PF_PixelFormat_ARGB64:return renderPixels<PF_Pixel16>(in,input,output,d,smart);case PF_PixelFormat_ARGB128:return renderPixels<PF_PixelFloat>(in,input,output,d,smart);default:return PF_Err_BAD_CALLBACK_PARAM;}
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr ptr,PF_PluginDataCB2 callback,SPBasicSuite*,const char*,const char*){
 PF_Err result=PF_Err_NONE;
 PF_REGISTER_EFFECT_EXT2(ptr,callback,"CurveSmear","Siosi CurveSmear","CurveSmear",AE_RESERVED_INFO,"EffectMain","");
 return result;
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd,PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_LayerDef* output,void* extra){
 try {
  switch(cmd){
   case PF_Cmd_ABOUT:std::strcpy(out->return_msg,"CurveSmear 0.1.3\rLocal curve-driven smear with Flat/Round caps and an editable width profile.");break;
   case PF_Cmd_GLOBAL_SETUP:out->my_version=VERSION;out->out_flags=FLAGS;out->out_flags2=FLAGS2;break;
   case PF_Cmd_PARAMS_SETUP:return setup(in,out);
   case PF_Cmd_ARBITRARY_CALLBACK:return HandleArbitrary(in,out,static_cast<PF_ArbParamsExtra*>(extra));
   case PF_Cmd_EVENT:return HandleProfileEvent(in,out,params,static_cast<PF_EventExtra*>(extra));
   case PF_Cmd_USER_CHANGED_PARAM:return HandleProfileButton(in,out,params,static_cast<PF_UserChangedParamExtra*>(extra));
   case PF_Cmd_SMART_PRE_RENDER:return preRender(in,out,static_cast<PF_PreRenderExtra*>(extra));
   case PF_Cmd_SMART_RENDER:{auto e=static_cast<PF_SmartRenderExtra*>(extra);PF_EffectWorld *input=nullptr,*dest=nullptr;check(e->cb->checkout_layer_pixels(in->effect_ref,0,&input));check(e->cb->checkout_output(in->effect_ref,&dest));auto d=static_cast<const Data*>(e->input->pre_render_data);if(!d||!input||!dest)return PF_Err_BAD_CALLBACK_PARAM;return render(in,input,dest,*d,true);}
   case PF_Cmd_RENDER:{auto d=readData(in,out,params);return render(in,&params[0]->u.ld,output,d,false);}
   default:break;
  }
 }catch(PF_Err e){return e;}catch(const std::bad_alloc&){return PF_Err_OUT_OF_MEMORY;}catch(...){return PF_Err_INTERNAL_STRUCT_DAMAGED;}
 return PF_Err_NONE;
}
