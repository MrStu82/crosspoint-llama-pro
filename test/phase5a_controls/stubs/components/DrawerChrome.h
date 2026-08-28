#pragma once
class GfxRenderer; struct Rect { int x,y,width,height; Rect(int a,int b,int c,int d):x(a),y(b),width(c),height(d){} }; namespace DrawerChrome { enum class Edge{Top,Bottom}; inline void clearRegion(const GfxRenderer&,Rect){} inline bool isOutsideTap(Edge,Rect,int,int){return false;} }
