#ifndef DIGITALPORTCONTROLLER_HPP
#define DIGITALPORTCONTROLLER_HPP

#include "mbed.h"

#include "../AbstractController.hpp"

class DigitalPortController : public AbstractController {

public:
	enum PortMode {
		OUTPUT,
		INPUT,
		PWM
	};

	enum DigitalValue {
		LOW,
		HIGH
	};

	class DigitalPortChangeListener {
	public:
		virtual void onDigitalPortValueChange() = 0;
	};

	DigitalPortController(int id, PinName pinName);

	void setMode(PortMode mode);
	void setValue(DigitalValue value);
	void setPwmDutyCycle(float dutyCycle);

	DigitalValue getDigitalReading();
	void addChangeListener(DigitalPortController::DigitalPortChangeListener *listener);
	void addIntervalListener(DigitalPortController::DigitalPortChangeListener *listener, int intervalMs = 1000);

private:
	int id;
	PinName pinName;
	DigitalOut digitalOut;
};

#endif
