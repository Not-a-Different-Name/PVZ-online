#pragma once
#include <memory>
#include <vector>
#include <cstdint>
class LawnApp;class Board;
namespace Sexy {class Graphics;}
namespace ObserverWorld {
bool Capture(Board*,std::vector<uint8_t>&);
class World {
 struct Impl;std::unique_ptr<Impl> impl;
public:
 explicit World(LawnApp*);~World();bool Restore(const std::vector<uint8_t>&);
 void Advance(unsigned milliseconds);void Draw(Sexy::Graphics*);Board* Inspect()const;
};
}
