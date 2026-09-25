#pragma once

#if defined(OPENDRIFT_BOARD_MATRIX)

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>


class MatrixStatus
{
public:
    enum class State : uint8_t
    {
        Boot,
        Calibrating,
        Ready,
        NoSignal,
        Error
    };

    void begin();
    void setRotation(uint8_t rotation);
    void setState(State state);
    void update(
        bool receiverSignal,
        bool wifiEnabled,
        bool blackboxReady
    );

private:
    static constexpr uint8_t DATA_PIN = 14;
    static constexpr uint8_t PIXEL_COUNT = 64;
    static constexpr uint8_t BRIGHTNESS = 10;

    Adafruit_NeoPixel pixels = Adafruit_NeoPixel(
        PIXEL_COUNT,
        DATA_PIN,
        NEO_RGB + NEO_KHZ800
    );

    State state = State::Boot;
    bool wifi = false;
    bool blackbox = false;
    bool started = false;
    bool dirty = true;
    // Fresh Matrix builds are mounted with the status artwork rotated 90
    // degrees counter-clockwise from the raw LED wiring orientation.
    uint8_t rotation = 3;

    void render();
    void clear();
    void setPixel(int x, int y, uint32_t color);
    void drawCheck(uint32_t color);
    void drawCross(uint32_t color);
    void drawHourglass(uint32_t color);
};

#endif
