#include "mbed.h"
#include "rtos.h"

#include "Application.hpp"
#include "Config.hpp"
#include "Debug.hpp"
#include "CommandManager.hpp"
#include "EthernetManager.hpp"

Application::Application(Config *config) :
	config(config)
{}

void Application::run() {
	setup();

	while (true) {
		loop();
	}
}

void Application::setup() {
	setupSerial();
	setupDebug();
	setupCommandManager();
	setupEthernetManager();
}

void Application::loop() {
	printf("> main loop\n");

	Thread::wait(1000);
}

void Application::setupDebug() {
	debug = new Debug(
		config->debugBreatheLedPin,
		config->debugCommandReceivedLedPin
	);
}

void Application::setupCommandManager() {
	commandManager = new CommandManager();
}

void Application::setupEthernetManager() {
	ethernetManager = new EthernetManager();

	ethernetManager->initialize();
}

void Application::setupSerial() {
	serial = new Serial(config->serialTxPin, config->serialRxPin);
	serial->baud(config->serialBaudRate);

	serial->attach(this, &Application::handleSerialRx, Serial::RxIrq);
	serial->attach(this, &Application::handleSerialTx, Serial::TxIrq);
}

void Application::handleSerialRx() {
	// serialRxNotifierThread.signal_set(SIGNAL_SERIAL_RX);

	char receivedChar = serial->getc();

	if (receivedChar == '\n') {
		if (commandManager != NULL) {
			commandManager->handleCommand(commandBuffer);
		} else {
			printf("> command manager is not yet available to handle '%s'", commandBuffer.c_str());
		}

		commandBuffer = "";

		debug->handleCommandReceived();
	} else {
		commandBuffer += receivedChar;
	}
}

void Application::handleSerialTx() {
	//serialTxNotifierThread.signal_set(SIGNAL_SERIAL_TX);
}
