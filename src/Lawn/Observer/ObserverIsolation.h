#pragma once
namespace ObserverIsolation {
inline thread_local unsigned depth=0;
inline bool Active(){return depth!=0;}
struct Fence {Fence(){++depth;}~Fence(){--depth;}Fence(const Fence&)=delete;};
}
