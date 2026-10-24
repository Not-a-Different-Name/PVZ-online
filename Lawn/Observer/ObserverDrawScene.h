#pragma once
#include "ObserverState.h"
#include "ObserverPrediction.h"
#include "graphics/Image.h"
#include "graphics/Graphics.h"
#include "graphics/TriVertex.h"
#include "misc/SexyMatrix.h"
#include <array>
#include <map>
#include <memory>
#include <vector>
#include <string>
#include <tuple>
class LawnApp;class Board;
namespace Observer {
struct AssetKey {uint8_t kind=0;uint32_t id=0;uint16_t item=0,filters=0;bool operator<(const AssetKey&o)const{return std::tie(kind,id,item,filters)<std::tie(o.kind,o.id,o.item,o.filters);}};
enum class DrawOp:uint8_t {Fill,Rect,Line,LineAA,Blt,BltF,Rotate,Stretch,Matrix,Triangles,Mirror,StretchMirror,Text,AnimationTrack};
struct DrawCommand {
 DrawOp op=DrawOp::Fill;uint16_t asset=0,motion=0;uint32_t color=0;uint8_t mode=0,flags=0;
 std::array<int,4> src{},dst{},clip{0,0,800,600};std::array<double,8> values{};
 std::vector<Sexy::TriVertex> vertices;std::string text;
};
struct DrawFrame {Identity identity;uint64_t sequence=0;uint8_t flags=0;uint16_t wave=0,total=0;std::vector<AssetKey> assets;std::vector<DrawCommand> commands;std::vector<Motion> motions;std::vector<VisualAnimation> animations;};
class DrawAssets {
public:
 explicit DrawAssets(LawnApp* app);~DrawAssets();
 bool Identify(Sexy::Image*,AssetKey&);Sexy::Image* Resolve(const AssetKey&);void Refresh();
private:struct Impl;std::unique_ptr<Impl> impl;
};
class DrawScene {
public:
 explicit DrawScene(LawnApp*);~DrawScene();bool Validate(const DrawFrame&)const;std::size_t Bytes(const DrawFrame&)const;
 bool Prepare(const DrawFrame&);void Draw(Sexy::Graphics*);void Clear();std::size_t Size()const{return current.commands.size();}
 void SetPredictionAge(double seconds){predictionAge=seconds;}
 uint8_t Flags()const{return current.flags;}uint16_t Wave()const{return current.wave;}uint16_t Total()const{return current.total;}
private:DrawAssets assets;DrawFrame current;std::vector<Sexy::Image*> images;std::vector<std::unique_ptr<Reanimation>> animations;double predictionAge=0;
};
// Recording is a draw target, never a game entity or a second Board.
class DrawRecorder final:public Sexy::Image {
public:
 DrawRecorder(LawnApp*,DrawFrame&);bool Good()const{return valid;}void Fail(){valid=false;}
 bool Predicting()const{return (frame.flags&4)!=0;}
 int AnimationStart(Reanimation*);bool AnimationTrack(Sexy::Graphics*,Reanimation*,int,int);
 void RenderItemMotion(Board*,const RenderItem&);
 void Text(const std::string&,int,int,int,bool,const Sexy::Color&,const Sexy::Rect&);
 void FillRect(const Sexy::Rect&,const Sexy::Color&,int)override;void DrawRect(const Sexy::Rect&,const Sexy::Color&,int)override;
 void ClearRect(const Sexy::Rect&)override;void DrawLine(double,double,double,double,const Sexy::Color&,int)override;void DrawLineAA(double,double,double,double,const Sexy::Color&,int)override;
 bool PolyFill3D(const Sexy::Point*,int,const Sexy::Rect*,const Sexy::Color&,int,int,int)override;
 void FillScanLines(Sexy::Span*,int,const Sexy::Color&,int)override;
#ifdef __ANDROID__
 void FillScanLinesWithCoverage(Sexy::Span*,int,const Sexy::Color&,int,const uint8_t*,int,int,int,int)override;
 void Blt(Sexy::Image*,int,int,const Sexy::Rect&,const Sexy::Color&,int,bool=true)override;
 void BltMirror(Sexy::Image*,int,int,const Sexy::Rect&,const Sexy::Color&,int,bool=true)override;
#else
 void FillScanLinesWithCoverage(Sexy::Span*,int,const Sexy::Color&,int,const BYTE*,int,int,int,int)override;
 void Blt(Sexy::Image*,int,int,const Sexy::Rect&,const Sexy::Color&,int)override;
 void BltMirror(Sexy::Image*,int,int,const Sexy::Rect&,const Sexy::Color&,int)override;
#endif
 void BltF(Sexy::Image*,float,float,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Color&,int)override;
 void BltRotated(Sexy::Image*,float,float,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Color&,int,double,float,float)override;
 void StretchBlt(Sexy::Image*,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Color&,int,bool)override;
 void StretchBltMirror(Sexy::Image*,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Rect&,const Sexy::Color&,int,bool)override;
 void BltMatrix(Sexy::Image*,float,float,const Sexy::SexyMatrix3&,const Sexy::Rect&,const Sexy::Color&,int,const Sexy::Rect&,bool)override;
 void BltTrianglesTex(Sexy::Image*,const Sexy::TriVertex[][3],int,const Sexy::Rect&,const Sexy::Color&,int,float,float,bool)override;
private:DrawAssets assets;DrawFrame& frame;bool valid=true;uint16_t motion=0;std::map<AssetKey,uint16_t> ids;
 uint16_t Asset(Sexy::Image*);
 DrawCommand Base(DrawOp,const Sexy::Color&,int);void Add(DrawCommand,Sexy::Image* =nullptr);
};
bool CaptureDrawFrame(Board*,DrawFrame&);
bool EncodeDrawFrame(const DrawFrame&,std::vector<uint8_t>&);bool DecodeDrawFrame(const uint8_t*,size_t,DrawFrame&);
}
