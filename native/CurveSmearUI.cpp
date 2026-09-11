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
static Plot plotFor(const PF_EventExtra* e){const auto& f=e->effect_win.current_frame;return {float(f.left+18),float(f.top+12),float(std::max<A_long>(80,f.right-f.left-36)),float(std::max<A_long>(60,f.bottom-f.top-38))};}
static bool inside(float x,float y,const Plot& p){return x>=p.x&&x<=p.x+p.w&&y>=p.y&&y<=p.y+p.h;}
static std::basic_string<DRAWBOT_UTF16Char> utf16(const wchar_t* w){std::basic_string<DRAWBOT_UTF16Char> s;while(*w)s.push_back(static_cast<DRAWBOT_UTF16Char>(*w++));s.push_back(0);return s;}
static void invalidate(PF_InData* in,PF_EventExtra* e){UISuite<PFAppSuite6> app(in,kPFAppSuite,kPFAppSuiteVersion6);PF_Rect r=e->effect_win.current_frame;uiCheck(app->PF_InvalidateRect(e->contextH,&r));}
static void markChanged(PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){params[PROFILE]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;e->evt_out_flags|=PF_EO_HANDLED_EVENT|PF_EO_UPDATE_NOW;}
static PF_Err drawProfile(PF_InData* in,PF_OutData*,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;
 auto data=readHandle(in,params[PROFILE]->u.arb_d.value);data.smooth=params[PROFILE_SMOOTH]->u.bd.value?1u:0u;auto plot=plotFor(e);
 UISuite<PF_EffectCustomUISuite2> custom(in,kPFEffectCustomUISuite,kPFEffectCustomUISuiteVersion2);DRAWBOT_DrawRef draw=nullptr;uiCheck(custom->PF_GetDrawingReference(e->contextH,&draw));
 UISuite<DRAWBOT_DrawbotSuite1> drawSuite(in,kDRAWBOT_DrawSuite,kDRAWBOT_DrawSuite_Version1);UISuite<DRAWBOT_SupplierSuite1> supplierSuite(in,kDRAWBOT_SupplierSuite,kDRAWBOT_SupplierSuite_Version1);UISuite<DRAWBOT_SurfaceSuite2> surfaceSuite(in,kDRAWBOT_SurfaceSuite,kDRAWBOT_SurfaceSuite_Version2);UISuite<DRAWBOT_PathSuite1> pathSuite(in,kDRAWBOT_PathSuite,kDRAWBOT_PathSuite_Version1);
 DRAWBOT_SupplierRef supplier=nullptr;DRAWBOT_SurfaceRef surface=nullptr;uiCheck(drawSuite->GetSupplier(draw,&supplier));uiCheck(drawSuite->GetSurface(draw,&surface));
 DRAWBOT_ColorRGBA bg{.055f,.07f,.08f,1},grid{.22f,.25f,.27f,1},line{.74f,.95f,.52f,1},left{.96f,.66f,.35f,1},right{.48f,.72f,1,1},text{.72f,.75f,.76f,1};DRAWBOT_RectF32 rect{plot.x,plot.y,plot.w,plot.h};uiCheck(surfaceSuite->PaintRect(surface,&bg,&rect));
 auto stroke=[&](const DRAWBOT_ColorRGBA& color,float width,auto build){DRAWBOT_PathRef path=nullptr;DRAWBOT_PenRef pen=nullptr;uiCheck(supplierSuite->NewPath(supplier,&path));try{build(path);uiCheck(supplierSuite->NewPen(supplier,&color,width,&pen));uiCheck(surfaceSuite->StrokePath(surface,pen,path));}catch(...){if(pen)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(pen));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));throw;}if(pen)supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(pen));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));};
 stroke(grid,1,[&](DRAWBOT_PathRef path){for(int i=0;i<=4;i++){float x=plot.x+plot.w*i/4,y=plot.y+plot.h*i/4;pathSuite->MoveTo(path,x,plot.y);pathSuite->LineTo(path,x,plot.y+plot.h);pathSuite->MoveTo(path,plot.x,y);pathSuite->LineTo(path,plot.x+plot.w,y);}});
 stroke(line,2,[&](DRAWBOT_PathRef path){for(int i=0;i<=128;i++){double u=i/128.,v=smear::profileValue(data,u);float x=plot.x+plot.w*float(u),y=plot.y+plot.h*float(1-v);if(i)pathSuite->LineTo(path,x,y);else pathSuite->MoveTo(path,x,y);}});
 auto dot=[&](float x,float y,const DRAWBOT_ColorRGBA& color,float radius){DRAWBOT_PathRef path=nullptr;DRAWBOT_BrushRef brush=nullptr;uiCheck(supplierSuite->NewPath(supplier,&path));DRAWBOT_PointF32 c{x,y};pathSuite->AddArc(path,&c,radius,0.f,360.f);pathSuite->Close(path);uiCheck(supplierSuite->NewBrush(supplier,&color,&brush));uiCheck(surfaceSuite->FillPath(surface,brush,path,kDRAWBOT_FillType_Default));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(path));};
 for(std::uint32_t i=0;i<data.count;i++){auto& p=data.points[i];dot(plot.x+p.x*plot.w,plot.y+(1-p.y)*plot.h,i==0?left:i+1==data.count?right:line,i==data.selected?6.f:4.f);}
 DRAWBOT_Boolean supports=false;supplierSuite->SupportsText(supplier,&supports);if(supports){float size=11;supplierSuite->GetDefaultFontSize(supplier,&size);DRAWBOT_FontRef font=nullptr;DRAWBOT_BrushRef brush=nullptr;supplierSuite->NewDefaultFont(supplier,size,&font);supplierSuite->NewBrush(supplier,&text,&brush);auto drawText=[&](const wchar_t* value,float x,float y,DRAWBOT_TextAlignment align){auto s=utf16(value);DRAWBOT_PointF32 o{x,y};surfaceSuite->DrawString(surface,brush,font,s.data(),&o,align,kDRAWBOT_TextTruncation_None,0);};drawText(L"L",plot.x,plot.y+plot.h+18,kDRAWBOT_TextAlignment_Left);drawText(L"0%",plot.x-4,plot.y+plot.h,kDRAWBOT_TextAlignment_Right);drawText(L"100%",plot.x-4,plot.y+10,kDRAWBOT_TextAlignment_Right);drawText(L"R",plot.x+plot.w,plot.y+plot.h+18,kDRAWBOT_TextAlignment_Right);supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(brush));supplierSuite->ReleaseObject(reinterpret_cast<DRAWBOT_ObjectRef>(font));}
 e->evt_out_flags|=PF_EO_HANDLED_EVENT;return PF_Err_NONE;
}
static PF_Err clickProfile(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;auto plot=plotFor(e);float x=static_cast<float>(e->u.do_click.screen_point.h),y=static_cast<float>(e->u.do_click.screen_point.v);if(!inside(x,y,plot))return PF_Err_NONE;
 auto data=readHandle(in,params[PROFILE]->u.arb_d.value);int hit=-1;for(std::uint32_t i=0;i<data.count;i++)if(std::hypot(plot.x+data.points[i].x*plot.w-x,plot.y+(1-data.points[i].y)*plot.h-y)<10){hit=int(i);break;}
 bool changed=false;if(hit<0&&data.count<smear::PROFILE_MAX_POINTS){float u=(x-plot.x)/plot.w,v=1-(y-plot.y)/plot.h;std::uint32_t at=1;while(at<data.count&&data.points[at].x<u)at++;for(std::uint32_t i=data.count;i>at;i--)data.points[i]=data.points[i-1];data.points[at]={u,std::clamp(v,0.f,1.f)};data.count++;hit=at;changed=true;}
 if(hit>=0){data.selected=hit;writeHandle(in,params[PROFILE]->u.arb_d.value,data);e->u.do_click.continue_refcon[0]=hit+1;e->u.do_click.send_drag=TRUE;if(changed)markChanged(out,params,e);else e->evt_out_flags|=PF_EO_HANDLED_EVENT|PF_EO_UPDATE_NOW;invalidate(in,e);}return PF_Err_NONE;
}
static PF_Err dragProfile(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 if(e->effect_win.area!=PF_EA_CONTROL||e->effect_win.index!=PROFILE)return PF_Err_NONE;int index=int(e->u.do_click.continue_refcon[0])-1;auto data=readHandle(in,params[PROFILE]->u.arb_d.value);if(index<0||index>=int(data.count))return PF_Err_NONE;
 auto plot=plotFor(e);float u=(e->u.do_click.screen_point.h-plot.x)/plot.w,v=1-(e->u.do_click.screen_point.v-plot.y)/plot.h;data.points[index].y=std::clamp(v,0.f,1.f);if(index>0&&index+1<int(data.count))data.points[index].x=std::clamp(u,data.points[index-1].x+.01f,data.points[index+1].x-.01f);data.selected=index;writeHandle(in,params[PROFILE]->u.arb_d.value,data);markChanged(out,params,e);invalidate(in,e);return PF_Err_NONE;
}
PF_Err HandleProfileEvent(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_EventExtra* e){
 switch(e->e_type){case PF_Event_DRAW:return drawProfile(in,out,params,e);case PF_Event_DO_CLICK:return clickProfile(in,out,params,e);case PF_Event_DRAG:return dragProfile(in,out,params,e);case PF_Event_ADJUST_CURSOR:if(e->effect_win.area==PF_EA_CONTROL&&e->effect_win.index==PROFILE){e->u.adjust_cursor.set_cursor=PF_Cursor_CROSSHAIRS;e->evt_out_flags|=PF_EO_HANDLED_EVENT;}break;default:break;}return PF_Err_NONE;
}
PF_Err HandleProfileButton(PF_InData* in,PF_OutData* out,PF_ParamDef* params[],PF_UserChangedParamExtra* e){
 if(e->param_index==PROFILE_SMOOTH){out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;return PF_Err_NONE;}
 if(e->param_index!=PROFILE_RESET&&e->param_index!=PROFILE_FLIP&&e->param_index!=PROFILE_DELETE)return PF_Err_NONE;
 auto p=readHandle(in,params[PROFILE]->u.arb_d.value);if(e->param_index==PROFILE_RESET)smear::resetProfile(p);else if(e->param_index==PROFILE_FLIP)smear::flipProfile(p);else if(p.selected>0&&p.selected+1<p.count){for(std::uint32_t i=p.selected;i+1<p.count;i++)p.points[i]=p.points[i+1];p.count--;p.selected=std::min(p.selected,p.count-1);}
 writeHandle(in,params[PROFILE]->u.arb_d.value,p);params[PROFILE]->uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;out->out_flags|=PF_OutFlag_FORCE_RERENDER|PF_OutFlag_REFRESH_UI;return PF_Err_NONE;
}
