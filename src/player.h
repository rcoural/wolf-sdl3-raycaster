#pragma once
#include <SDL3/SDL.h>
#include <span>

class Game;

class Player {
public:
    Player() = default;

    void handleInput(std::span<const bool> keys, const Game& game);
    void handleMouseMotion(float xrel) noexcept;

    [[nodiscard]] float getX() const noexcept { return x; }
    [[nodiscard]] float getY() const noexcept { return y; }
    [[nodiscard]] float getAngle() const noexcept { return angle; }

private:
    float x = 300.f;
    float y = 400.f;
    float angle = 0.f;
    float moveSpeed = 2.f;
    float rotSpeed = 3.f;
};
