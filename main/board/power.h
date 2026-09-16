#pragma once

namespace board {

// Latches the main power rail on. Without this the device drops power as soon
// as the user releases the power button and appears dead or boot-looping.
// Must be the first thing called from app_main().
void power_on_hold();

// Releases the latch. Renders the screen you want the user to keep seeing
// before calling this: the panel holds its image while powered down.
void power_off();

}  // namespace board
