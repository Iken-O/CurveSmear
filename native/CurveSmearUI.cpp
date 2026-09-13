#define NOMINMAX
#include "AEConfig.h"
#include <Windows.h>
#include "entry.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_EffectUI.h"
#include "CurveSmearUI.h"
#include "adobesdk/DrawbotSuite.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>

static void uiCheck(PF_Err e){if(e)throw e;}
template<class T> class UISuite {
 SPBasicSuite* basic;const char* name;A_long version;
public:const T* p=nullptr;
 UISuite(PF_InData* in,const char* n,A_long v):basic(in->pica_basicP),name(n),version(v){uiCheck(basic->AcquireSuite(n,v,reinterpret_cast<const void**>(&p)));}
 ~UISuite(){if(p)basic->ReleaseSuite(name,version);}const T* operator->()const{return p;}
};
static PF_Handle newHandle(PF_InData* in,const smear::ProfileData& value){
 UISuite<PF_HandleSuite1> handles(in,kPFHandleSuite,kPFHandleSuiteVersion1);PF_Handle h=handles->host_new_handle(sizeof(value));if(!h)throw PF_Err_OUT_OF_MEMORY;
 auto p=static_cast<smear::ProfileData*>(handles->host_lock_handle(h));if(!p){handles->host_dispose_handle(h);throw PF_Err_OUT_OF_MEMORY;}
 *p=value;handles->host_unlock_handle(h);return h;
}
PF_Err CreateDefaultProfile(PF_InData* in,PF_ArbitraryH* out){*out=newHandle(in,smear::defaultProfile());return PF_Err_NONE;}
static smear::ProfileData readHandle(PF_InData* in,PF_Handle h){
 if(!h)return smear::defaultProfile();UISuite<PF_HandleSuite1> handles(in,kPFHandleSuite,kPFHandleSuiteVersion1);auto p=static_cast<const smear::ProfileData*>(handles->host_lock_handle(h));if(!p)throw PF_Err_OUT_OF_MEMORY;
 auto value=*p;handles->host_unlock_handle(h);smear::sanitizeProfile(value);return value;
}
static void writeHandle(PF_InData* in,PF_Handle h,const smear::ProfileData& value){
 if(!h)throw PF_Err_BAD_CALLBACK_PARAM;UISuite<PF_HandleSuite1> handles(in,kPFHandleSuite,kPFHandleSuiteVersion1);auto p=static_cast<smear::ProfileData*>(handles->host_lock_handle(h));if(!p)throw PF_Err_OUT_OF_MEMORY;
 *p=value;handles->host_unlock_handle(h);
}
PF_Err HandleArbitrary(PF_InData* in,PF_OutData*,PF_ArbParamsExtra* e){
 auto refOk=[&](void* p){return p==PROFILE_REFCON;};
 switch(e->which_function){
  case PF_Arbitrary_NEW_FUNC:if(!refOk(e->u.new_func_params.refconPV))return PF_Err_UNRECOGNIZED_PARAM_TYPE;return CreateDefaultProfile(in,e->u.new_func_params.arbPH);
  case PF_Arbitrary_DISPOSE_FUNC:if(!refOk(e->u.dispose_func_params.refconPV))return PF_Err_UNRECOGNIZED_PARAM_TYPE;if(e->u.dispose_func_params.arbH){UISuite<PF_HandleSuite1> handles(in,kPFHandleSuite,kPFHandleSuiteVersion1);handles->host_dispose_handle(e->u.dispose_func_params.arbH);}return PF_Err_NONE;
  case PF_Arbitrary_COPY_FUNC:{if(!refOk(e->u.copy_func_params.refconPV))return PF_Err_UNRECOGNIZED_PARAM_TYPE;*e->u.copy_func_params.dst_arbPH=newHandle(in,readHandle(in,e->u.copy_func_params.src_arbH));return PF_Err_NONE;}
  case PF_Arbitrary_FLAT_SIZE_FUNC:*e->u.flat_size_func_params.flat_data_sizePLu=sizeof(smear::ProfileData);return PF_Err_NONE;
  case PF_Arbitrary_FLATTEN_FUNC:{if(e->u.flatten_func_params.buf_sizeLu<sizeof(smear::ProfileData))return PF_Err_BAD_CALLBACK_PARAM;auto p=readHandle(in,e->u.flatten_func_params.arbH);std::memcpy(e->u.flatten_func_params.flat_dataPV,&p,sizeof(p));return PF_Err_NONE;}
  case PF_Arbitrary_UNFLATTEN_FUNC:{
   if(e->u.unflatten_func_params.buf_sizeLu!=sizeof(smear::ProfileData))return PF_Err_CANNOT_PARSE_KEYFRAME_TEXT;
   smear::ProfileData p;std::memcpy(&p,e->u.unflatten_func_params.flat_dataPV,sizeof(p));if(p.schema!=smear::PROFILE_SCHEMA)return PF_Err_CANNOT_PARSE_KEYFRAME_TEXT;
   smear::sanitizeProfile(p);*e->u.unflatten_func_params.arbPH=newHandle(in,p);return PF_Err_NONE;
  }
  case PF_Arbitrary_INTERP_FUNC:{auto h=e->u.interp_func_params.tF<.5?e->u.interp_func_params.left_arbH:e->u.interp_func_params.right_arbH;*e->u.interp_func_params.interpPH=newHandle(in,readHandle(in,h));return PF_Err_NONE;}
  case PF_Arbitrary_COMPARE_FUNC:{auto a=readHandle(in,e->u.compare_func_params.a_arbH),b=readHandle(in,e->u.compare_func_params.b_arbH);int c=std::memcmp(&a,&b,sizeof(a));*e->u.compare_func_params.compareP=c<0?PF_ArbCompare_LESS:c>0?PF_ArbCompare_MORE:PF_ArbCompare_EQUAL;return PF_Err_NONE;}
  case PF_Arbitrary_PRINT_SIZE_FUNC:*e->u.print_size_func_params.print_sizePLu=1024;return PF_Err_NONE;
  case PF_Arbitrary_PRINT_FUNC:{
   auto p=readHandle(in,e->u.print_func_params.arbH);std::ostringstream s;s<<"CSP1 "<<p.smooth<<' '<<p.count;for(std::uint32_t i=0;i<p.count;i++)s<<' '<<p.points[i].x<<' '<<p.points[i].y;
   auto str=s.str();if(e->u.print_func_params.print_sizeLu){std::snprintf(e->u.print_func_params.print_bufferPC,e->u.print_func_params.print_sizeLu,"%s",str.c_str());}return PF_Err_NONE;
  }
  case PF_Arbitrary_SCAN_FUNC:{
   std::string text(e->u.scan_func_params.bufPC,e->u.scan_func_params.bytes_to_scanLu);std::istringstream s(text);std::string tag;smear::ProfileData p=smear::defaultProfile();s>>tag>>p.smooth>>p.count;
   if(tag!="CSP1"||p.count<2||p.count>smear::PROFILE_MAX_POINTS)return PF_Err_CANNOT_PARSE_KEYFRAME_TEXT;for(std::uint32_t i=0;i<p.count;i++)if(!(s>>p.points[i].x>>p.points[i].y))return PF_Err_CANNOT_PARSE_KEYFRAME_TEXT;
   smear::sanitizeProfile(p);*e->u.scan_func_params.arbPH=newHandle(in,p);return PF_Err_NONE;
  }
  default:return PF_Err_NONE;
 }
}

struct Plot {float x,y,w,h;};
struct Button {float x,y,w,h;};
static Plot plotFor(const PF_EventExtra* e){const auto& f=e->effect_win.current_frame;float width=float(std::max<A_long>(80,f.right-f.left-10));return {float(f.left+5),float(f.top+5),width,width*.5f};}
static std::array<Button,3> buttonsFor(const PF_EventExtra* e){auto p=plotFor(e);constexpr float gap=4,height=24;float y=p.y+p.h+7,w=(p.w-gap*2)/3;return {{{p.x,y,w,height},{p.x+w+gap,y,w,height},{p.x+(w+gap)*2,y,w,height}}};}
static bool inside(float x,float y,const Plot& p){return x>=p.x&&x<=p.x+p.w&&y>=p.y&&y<=p.y+p.h;}
static bool inside(float x,float y,const Button& b){return x>=b.x&&x<=b.x+b.w&&y>=b.y&&y<=b.y+b.h;}
static std::basic_string<DRAWBOT_UTF16Char> utf16(const wchar_t* w){std::basic_string<DRAWBOT_UTF16Char> s;while(*w)s.push_back(static_cast<DRAWBOT_UTF16Char>(*w++));s.push_back(0);return s;}
static void invalidate(PF_InData* in,PF_EventExtra* e){UISuite<PFAppSuite6> app(in,kPFAppSuite,kPFAppSuiteVersion6);PF_Rect r=e->effect_win.current_frame;uiCheck(app->PF_InvalidateRect(e->contextH,&r));}
static void markChanged(PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){params[PROFILE]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;e->evt_out_flags|=PF_EO_HANDLED_EVENT|PF_EO_UPDATE_NOW;}
static PF_Err drawProfile(PF_InData* in,PF_OutData*,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;
 auto data=readHandle(in,params[PROFILE]->u.arb_d.value);data.smooth=params[PROFILE_SMOOTH]->u.bd.value?1u:0u;auto plot=plotFor(e);
 UISuite<PF_EffectCustomUISuite2> custom(in,kPFEffectCustomUISuite,kPFEffectCustomUISuiteVersion2);DRAWBOT_DrawRef draw=nullptr;uiCheck(custom->PF_GetDrawingReference(e->contextH,&draw));
 UISuite<DRAWBOT_DrawbotSuite1> drawSuite(in,kDRAWBOT_DrawSuite,kDRAWBOT_DrawSuite_Version1);UISuite<DRAWBOT_SupplierSuite1> supplierSuite(in,kDRAWBOT_SupplierSuite,kDRAWBOT_SupplierSuite_Version1);UISuite<DRAWBOT_SurfaceSuite2> surfaceSuite(in,kDRAWBOT_SurfaceSuite,kDRAWBOT_SurfaceSuite_Version2);UISuite<DRAWBOT_PathSuite1> pathSuite(in,kDRAWBOT_PathSuite,kDRAWBOT_PathSuite_Version1);
 DRAWBOT_SupplierRef supplier=nullptr;DRAWBOT_SurfaceRef surface=nullptr;uiCheck(drawSuite->GetSupplier(draw,&supplier));uiCheck(drawSuite->GetSurface(draw,&surface));
 DRAWBOT_ColorRGBA bg{.11f,.11f,.11f,1},grid{.48f,.48f,.48f,1},border{.68f,.68f,.68f,1},line{.76f,.76f,.76f,1},selected{.96f,.96f,.96f,1},buttonFill{.43f,.43f,.43f,1},buttonDisabled{.27f,.27f,.27f,1},buttonText{.88f,.88f,.88f,1},disabledText{.52f,.52f,.52f,1};
 DRAWBOT_RectF32 rect{plot.x,plot.y,plot.w,plot.h};uiCheck(surfaceSuite->PaintRect(surface,&bg,&rect));
 auto stroke=[&](const DRAWBOT_ColorRGBA& color,float width,auto build){DRAWBOT_PathRef path=nullptr;DRAWBOT_PenRef pen=nullptr;uiCheck(supplierSuite->NewPath(supplier,&path));try{build(path);uiCheck(supplierSuite->NewPen(supplier,&color,width,&pen));uiCheck(surfaceSuite->StrokePath(surface,pen,path));}catch(...){if(pen)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(pen));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));throw;}if(pen)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(pen));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));};
 stroke(grid,.75f,[&](DRAWBOT_PathRef path){for(int i=1;i<4;i++){float x=plot.x+plot.w*i/4;for(float q=0;q<plot.h;q+=5){pathSuite->MoveTo(path,x,plot.y+q);pathSuite->LineTo(path,x,std::min(plot.y+q+2,plot.y+plot.h));}}float y=plot.y+plot.h*.5f;for(float q=0;q<plot.w;q+=5){pathSuite->MoveTo(path,plot.x+q,y);pathSuite->LineTo(path,std::min(plot.x+q+2,plot.x+plot.w),y);}});
 stroke(border,1,[&](DRAWBOT_PathRef path){pathSuite->MoveTo(path,plot.x,plot.y);pathSuite->LineTo(path,plot.x+plot.w,plot.y);pathSuite->LineTo(path,plot.x+plot.w,plot.y+plot.h);pathSuite->LineTo(path,plot.x,plot.y+plot.h);pathSuite->Close(path);});
 stroke(line,1.5f,[&](DRAWBOT_PathRef path){for(int i=0;i<=128;i++){double u=i/128.,v=smear::profileValue(data,u);float x=plot.x+plot.w*float(u),y=plot.y+plot.h*float(1-v);if(i)pathSuite->LineTo(path,x,y);else pathSuite->MoveTo(path,x,y);}});
 for(std::uint32_t i=0;i<data.count;i++){auto& p=data.points[i];float size=i==data.selected?7.f:5.f;DRAWBOT_RectF32 point{plot.x+p.x*plot.w-size*.5f,plot.y+(1-p.y)*plot.h-size*.5f,size,size};uiCheck(surfaceSuite->PaintRect(surface,i==data.selected?&selected:&line,&point));}
 auto buttons=buttonsFor(e);auto paintPill=[&](const Button& b,const DRAWBOT_ColorRGBA& color){float radius=b.h*.5f;DRAWBOT_RectF32 middle{b.x+radius,b.y,b.w-2*radius,b.h};uiCheck(surfaceSuite->PaintRect(surface,&color,&middle));DRAWBOT_PathRef path=nullptr;DRAWBOT_BrushRef brush=nullptr;uiCheck(supplierSuite->NewPath(supplier,&path));try{DRAWBOT_PointF32 left{b.x+radius,b.y+radius},right{b.x+b.w-radius,b.y+radius};pathSuite->AddArc(path,&left,radius,0,360);pathSuite->Close(path);pathSuite->AddArc(path,&right,radius,0,360);pathSuite->Close(path);uiCheck(supplierSuite->NewBrush(supplier,&color,&brush));uiCheck(surfaceSuite->FillPath(surface,brush,path,kDRAWBOT_FillType_Default));}catch(...){if(brush)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));throw;}if(brush)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));};
 bool canDelete=data.selected>0&&data.selected+1<data.count;for(int i=0;i<3;i++)paintPill(buttons[i],i==2&&!canDelete?buttonDisabled:buttonFill);
 DRAWBOT_Boolean supports=false;supplierSuite->SupportsText(supplier,&supports);if(supports){float size=10;supplierSuite->GetDefaultFontSize(supplier,&size);size=std::min(size,10.f);DRAWBOT_FontRef font=nullptr;DRAWBOT_BrushRef normalBrush=nullptr,disabledBrush=nullptr;uiCheck(supplierSuite->NewDefaultFont(supplier,size,&font));uiCheck(supplierSuite->NewBrush(supplier,&buttonText,&normalBrush));uiCheck(supplierSuite->NewBrush(supplier,&disabledText,&disabledBrush));const wchar_t* labels[]={L"Reset",L"Swap L/R",L"Delete"};for(int i=0;i<3;i++){auto text=utf16(labels[i]);DRAWBOT_PointF32 origin{buttons[i].x+buttons[i].w*.5f,buttons[i].y+buttons[i].h*.68f};surfaceSuite->DrawString(surface,i==2&&!canDelete?disabledBrush:normalBrush,font,text.data(),&origin,kDRAWBOT_TextAlignment_Center,kDRAWBOT_TextTruncation_End,buttons[i].w-8);}supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(disabledBrush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(normalBrush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(font));}
 e->evt_out_flags|=PF_EO_HANDLED_EVENT;return PF_Err_NONE;
}
static PF_Err clickProfile(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;auto plot=plotFor(e);float x=static_cast<float>(e->u.do_click.screen_point.h),y=static_cast<float>(e->u.do_click.screen_point.v);auto buttons=buttonsFor(e);
 for(int action=0;action<3;action++)if(inside(x,y,buttons[action])){auto data=readHandle(in,params[PROFILE]->u.arb_d.value);bool changed=true;if(action==0)smear::resetProfile(data);else if(action==1)smear::flipProfile(data);else if(data.selected>0&&data.selected+1<data.count){for(std::uint32_t i=data.selected;i+1<data.count;i++)data.points[i]=data.points[i+1];data.count--;data.selected=std::min(data.selected,data.count-1);}else changed=false;if(changed){writeHandle(in,params[PROFILE]->u.arb_d.value,data);markChanged(out,params,e);invalidate(in,e);}else e->evt_out_flags|=PF_EO_HANDLED_EVENT;return PF_Err_NONE;}
 if(!inside(x,y,plot))return PF_Err_NONE;
 auto data=readHandle(in,params[PROFILE]->u.arb_d.value);int hit=-1;for(std::uint32_t i=0;i<data.count;i++)if(std::hypot(plot.x+data.points[i].x*plot.w-x,plot.y+(1-data.points[i].y)*plot.h-y)<10){hit=int(i);break;}
 bool changed=false;if(hit<0&&data.count<smear::PROFILE_MAX_POINTS){float u=(x-plot.x)/plot.w,v=1-(y-plot.y)/plot.h;std::uint32_t at=1;while(at<data.count&&data.points[at].x<u)at++;for(std::uint32_t i=data.count;i>at;i--)data.points[i]=data.points[i-1];data.points[at]={u,std::clamp(v,0.f,1.f)};data.count++;hit=at;changed=true;}
 if(hit>=0){data.selected=hit;writeHandle(in,params[PROFILE]->u.arb_d.value,data);e->u.do_click.continue_refcon[0]=hit+1;e->u.do_click.send_drag=TRUE;if(changed)markChanged(out,params,e);else e->evt_out_flags|=PF_EO_HANDLED_EVENT|PF_EO_UPDATE_NOW;invalidate(in,e);}return PF_Err_NONE;
}
static PF_Err dragProfile(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;int index=int(e->u.do_click.continue_refcon[0])-1;auto data=readHandle(in,params[PROFILE]->u.arb_d.value);if(index<0||index>=int(data.count))return PF_Err_NONE;
 auto plot=plotFor(e);float u=(e->u.do_click.screen_point.h-plot.x)/plot.w,v=1-(e->u.do_click.screen_point.v-plot.y)/plot.h;data.points[index].y=std::clamp(v,0.f,1.f);if(index>0&&index+1<int(data.count))data.points[index].x=std::clamp(u,data.points[index-1].x+.01f,data.points[index+1].x-.01f);data.selected=index;writeHandle(in,params[PROFILE]->u.arb_d.value,data);markChanged(out,params,e);invalidate(in,e);return PF_Err_NONE;
}
PF_Err HandleProfileEvent(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 switch(e->e_type){case PF_Event_DRAW:return drawProfile(in,out,params,e);case PF_Event_DO_CLICK:return clickProfile(in,out,params,e);case PF_Event_DRAG:return dragProfile(in,out,params,e);case PF_Event_ADJUST_CURSOR:if(e->effect_win.area==PF_EA_CONTROL&&e->effect_win.index==PROFILE){float x=static_cast<float>(e->u.adjust_cursor.screen_point.h),y=static_cast<float>(e->u.adjust_cursor.screen_point.v);auto buttons=buttonsFor(e);bool overButton=false;for(const auto& button:buttons)overButton|=inside(x,y,button);e->u.adjust_cursor.set_cursor=overButton?PF_Cursor_FINGER_POINTER:PF_Cursor_CROSSHAIRS;e->evt_out_flags|=PF_EO_HANDLED_EVENT;}break;default:break;}return PF_Err_NONE;
}
PF_Err HandleProfileButton(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_UserChangedParamExtra* e){
 if(e->param_index==PROFILE_SMOOTH){out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;return PF_Err_NONE;}
 if(e->param_index!=PROFILE_RESET&&e->param_index!=PROFILE_FLIP&&e->param_index!=PROFILE_DELETE)return PF_Err_NONE;
 auto p=readHandle(in,params[PROFILE]->u.arb_d.value);if(e->param_index==PROFILE_RESET)smear::resetProfile(p);else if(e->param_index==PROFILE_FLIP)smear::flipProfile(p);else if(p.selected>0&&p.selected+1<p.count){for(std::uint32_t i=p.selected;i+1<p.count;i++)p.points[i]=p.points[i+1];p.count--;p.selected=std::min(p.selected,p.count-1);}
 writeHandle(in,params[PROFILE]->u.arb_d.value,p);params[PROFILE]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;return PF_Err_NONE;
}
