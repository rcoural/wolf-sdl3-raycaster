// Player position, facing angle, and WASD/arrow movement with wall collision.

#include "player.h"
#include "game.h"

#include <cmath>
#include <numbers>

namespace {
constexpr float kDeg2Rad = std::numbers::pi_v<float> / 180.0f;
}

void Player::handleInput(std::span<const bool> keys, const Game& game) {
    const float rad  = angle * kDeg2Rad;
    const float cosA = std::cos(rad);
    const float sinA = std::sin(rad);
    constexpr float r = 10.0f; // wall buffer to keep the player off the surface

    // Forward vector scaled by speed; X and Y axes checked separately so the
    // player can slide along walls instead of getting fully blocked.
    const float dx = cosA * moveSpeed;
    const float dy = sinA * moveSpeed;

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP]) {
        if (!game.isWall(x + dx + cosA * r, y)) x += dx;
        if (!game.isWall(x, y + dy + sinA * r)) y += dy;
    }
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN]) {
        if (!game.isWall(x - dx - cosA * r, y)) x -= dx;
        if (!game.isWall(x, y - dy - sinA * r)) y -= dy;
    }
    if (keys[SDL_SCANCODE_A]) {
        const float sx =  sinA * moveSpeed;
        const float sy = -cosA * moveSpeed;
        if (!game.isWall(x + sx + sinA * r, y)) x += sx;
        if (!game.isWall(x, y + sy - cosA * r)) y += sy;
    }
    if (keys[SDL_SCANCODE_D]) {
        const float sx = -sinA * moveSpeed;
        const float sy =  cosA * moveSpeed;
        if (!game.isWall(x + sx - sinA * r, y)) x += sx;
        if (!game.isWall(x, y + sy + cosA * r)) y += sy;
    }

    if (keys[SDL_SCANCODE_LEFT]) {
        angle -= rotSpeed;
        if (angle < 0.f) angle += 360.f;
    }
    if (keys[SDL_SCANCODE_RIGHT]) {
        angle += rotSpeed;
        if (angle >= 360.f) angle -= 360.f;
    }
}

void Player::handleMouseMotion(float xrel) noexcept {
    angle += xrel * 0.2f;
    if (angle <    0.f) angle += 360.f;
    if (angle >= 360.f) angle -= 360.f;
}
