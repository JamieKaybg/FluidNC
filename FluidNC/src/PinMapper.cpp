#include "PinMapper.h"

#include "Assert.h"

#include <esp32-hal-gpio.h>  // PULLUP, INPUT, OUTPUT
extern "C" void __pinMode(pinnum_t pin, uint8_t mode);
extern "C" int  __digitalRead(pinnum_t pin);
extern "C" void __digitalWrite(pinnum_t pin, uint8_t val);

// Pin mapping. Pretty straight forward, it's just a thing that stores pins in an array.
//
// The default behavior of a mapped pin is _undefined pin_, so it does nothing.
namespace {
    class Mapping {
        static const int N_PIN_MAPPINGS = 192;

    public:
        static const int GPIO_LIMIT = 64;

        Pin* _mapping[N_PIN_MAPPINGS];

        Mapping() {
            for (int i = 0; i < N_PIN_MAPPINGS; ++i) {
                _mapping[i] = nullptr;
            }
        }

        pinnum_t Claim(Pin* pin) {
            for (pinnum_t i = 0; i < N_PIN_MAPPINGS; ++i) {
                if (_mapping[i] == nullptr) {
                    _mapping[i] = pin;
                    return i + GPIO_LIMIT;
                }
            }
            return 0;
        }

        void Release(pinnum_t idx) { _mapping[idx - GPIO_LIMIT] = nullptr; }

        static Mapping& instance() {
            static Mapping instance;
            return instance;
        }
    };
}

// See header file for more information.

PinMapper::PinMapper() : _mappedId(0) {}

PinMapper::PinMapper(Pin& pin) {
    _mappedId = Mapping::instance().Claim(&pin);

    // If you reach this assertion, you haven't been using the Pin class like you're supposed to.
    Assert(_mappedId != 0, "Cannot claim pin. We've reached the limit of 255 mapped pins.");
}

// To aid return values and assignment
PinMapper::PinMapper(PinMapper&& o) : _mappedId(0) {
    std::swap(_mappedId, o._mappedId);
}

PinMapper& PinMapper::operator=(PinMapper&& o) {
    // Special case for `a=a`. If we release there, things go wrong.
    if (&o != this) {
        if (_mappedId != 0) {
            Mapping::instance().Release(_mappedId);
            _mappedId = 0;
        }
        std::swap(_mappedId, o._mappedId);
    }
    return *this;
}

// Clean up when we get destructed.
PinMapper::~PinMapper() {
    if (_mappedId != 0) {
        Mapping::instance().Release(_mappedId);
    }
}

// Arduino compatibility functions, which basically forward the call to the mapper:
void IRAM_ATTR digitalWrite(pinnum_t pin, uint8_t val) {
    if (pin < Mapping::GPIO_LIMIT) {
        return __digitalWrite(pin, val);
    }
    auto thePin = Mapping::instance()._mapping[pin - Mapping::GPIO_LIMIT];
    if (thePin) {
        thePin->synchronousWrite(val);
    }
}

void IRAM_ATTR pinMode(pinnum_t pin, uint8_t mode) {
    if (pin < Mapping::Mapping::GPIO_LIMIT) {
        __pinMode(pin, mode);
        return;
    }

    auto thePin = Mapping::instance()._mapping[pin - Mapping::GPIO_LIMIT];
    if (!thePin) {
        return;
    }

    Pins::PinAttributes attr = Pins::PinAttributes::None;
    if ((mode & OUTPUT) == OUTPUT) {
        attr = attr | Pins::PinAttributes::Output;
    }
    if ((mode & INPUT) == INPUT) {
        attr = attr | Pins::PinAttributes::Input;
    }
    if ((mode & PULLUP) == PULLUP) {
        attr = attr | Pins::PinAttributes::PullUp;
    }
    if ((mode & PULLDOWN) == PULLDOWN) {
        attr = attr | Pins::PinAttributes::PullDown;
    }

    thePin->setAttr(attr);
}

int IRAM_ATTR digitalRead(pinnum_t pin) {
    if (pin < Mapping::GPIO_LIMIT) {
        return __digitalRead(pin);
    }
    auto thePin = Mapping::instance()._mapping[pin - Mapping::GPIO_LIMIT];
    return (thePin) ? thePin->read() : 0;
}
