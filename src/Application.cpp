#include "Application.hpp"
#include "Config.hpp"

Application::Application(Config *config) :
	config(config),
	serial(config->serialTxPin, config->serialRxPin)
{}

void Application::run() {
	setup();

	while (true) {
		loop();
	}
}

void Application::setup() {
	setupTimer();
	setupSerial();
	setupCommandHandlers();
	setupDebug();
	setupEthernetManager();
	setupSocketServer();
}

void Application::loop() {
	consumeQueuedCommands();

	// Thread::wait(1000);
}

void Application::setupTimer() {
	timer.start();
}

void Application::setupSerial() {
	serial.baud(config->serialBaudRate);
	serial.attach(this, &Application::handleSerialRx, Serial::RxIrq);

	printf("\n\n### initializing ###\n");
}

void Application::setupCommandHandlers() {
	printf("# setting up command handlers\n");

	registerCommandHandler("memory", this, &Application::handleMemoryCommand);
	registerCommandHandler("sum", this, &Application::handleSumCommand);
	registerCommandHandler("led", this, &Application::handleLedCommand);
}

void Application::setupDebug() {
	printf("# setting up debugging\n");

	debug.setLedMode(LED_BREATHE_INDEX, Debug::LedMode::BREATHE);
}

void Application::setupEthernetManager() {
	printf("# setting up ethernet manager\n");

	debug.setLedMode(LED_ETHERNET_STATUS_INDEX, Debug::LedMode::BLINK_FAST);

	bool isConnected = ethernetManager.initialize();

	if (isConnected) {
		debug.setLedMode(LED_ETHERNET_STATUS_INDEX, Debug::LedMode::BLINK_SLOW);
	} else {
		debug.setLedMode(LED_ETHERNET_STATUS_INDEX, Debug::LedMode::OFF);
	}
}

void Application::setupSocketServer() {
	printf("# setting up socket server\n");

	socketServer.addListener(this);
	socketServer.start(ethernetManager.getEthernetInterface(), config->socketServerPort);
}

// TODO replace this with proper response model
/*
void Application::sendMessage(const char *fmt, ...) {
	va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
	vsprintf(sendBuffer, fmt, args);
	socketServer.sendMessage(sendBuffer);
    va_end(args);
}
*/

template<typename T, typename M>
void Application::registerCommandHandler(std::string name, T *obj, M method) {
	registerCommandHandler(name, Callback<CommandManager::Command::Response(CommandManager::Command*)>(obj, method));
}

void Application::registerCommandHandler(std::string name, Callback<CommandManager::Command::Response(CommandManager::Command*)> func) {
	printf("# registering command handler for '%s'\n", name.c_str());

	commandHandlerMap[name].attach(func);
}

void Application::consumeQueuedCommands() {
	CommandManager::Command *command = commandManager.getNextCommand();

	while (command != NULL) {
		consumeCommand(command);

		command = commandManager.getNextCommand();
	}
}

void Application::consumeCommand(CommandManager::Command *command) {
	CommandHandlerMap::iterator commandIt = commandHandlerMap.find(command->name);

	if (commandIt != commandHandlerMap.end()) {
		printf("# calling command handler for #%d '%s' (source: %d)\n", command->id, command->name.c_str(), command->sourceId);

		for (int i = 0; i < command->argumentCount; i++) {
			printf("#  argument %d: %s\n", i, command->arguments[i].c_str());
		}

		CommandManager::Command::Response response = commandIt->second.call(command);
		std::string responseText = response.getResponseText();

		printf("# command response for %d: %s\n", response.requestId, responseText.c_str());

		switch (command->sourceId) {
			case CommandSource::SOCKET:
				socketServer.sendMessage(response.getResponseText() + "\n");
				printf("> %s\n", responseText.c_str());
				break;

			case CommandSource::SERIAL:
				printf("> %s\n", responseText.c_str());
				break;

			default:
				error("unexpected command source %d\n", command->sourceId);
		}
	} else {
		printf("# command handler for #%d '%s' (source: %d) has not been registered\n", command->id, command->name.c_str(), command->sourceId);

		for (int i = 0; i < command->argumentCount; i++) {
			printf("#  argument %d: %s\n", i, command->arguments[i].c_str());
		}
	}
}

bool Application::validateCommandArgumentCount(CommandManager::Command *command, int expectedArgumentCount) {
	if (command->argumentCount != expectedArgumentCount) {
		return false;
	}

	return true;
}

void Application::onSocketClientConnected(TCPSocketConnection* client) {
	debug.setLedMode(LED_ETHERNET_STATUS_INDEX, Debug::LedMode::ON);
}

void Application::onSocketClientDisconnected(TCPSocketConnection* client) {
	debug.setLedMode(LED_ETHERNET_STATUS_INDEX, Debug::LedMode::BLINK_SLOW);
}

void Application::onSocketCommandReceived(const char *command, int length) {
	commandManager.handleCommand(CommandSource::SOCKET, command, length);

	//consumeQueuedCommands();

	debug.setLedMode(LED_COMMAND_RECEIVED_INDEX, Debug::LedMode::BLINK_ONCE);
}

void Application::handleSerialRx() {
	char receivedChar = serial.getc();

	if (receivedChar == '\n') {
		commandManager.handleCommand(CommandSource::SERIAL, commandBuffer, commandLength);

		commandBuffer[0] = '\0';
		commandLength = 0;

		debug.setLedMode(LED_COMMAND_RECEIVED_INDEX, Debug::LedMode::BLINK_ONCE);

		//consumeQueuedCommands();
	} else {
		if (commandLength > MAX_COMMAND_LENGTH - 1) {
			error("maximum command length is %d characters, stopping at %s\n", MAX_COMMAND_LENGTH, commandBuffer);
		}

		commandBuffer[commandLength++] = receivedChar;
		commandBuffer[commandLength] = '\0';
	}
}

CommandManager::Command::Response Application::handleMemoryCommand(CommandManager::Command *command) {
	if (!validateCommandArgumentCount(command, 0)) {
		return command->createFailureResponse();
	}

	int freeMemoryBytes = Debug::getFreeMemoryBytes();

	return command->createSuccessResponse(/*freeMemoryBytes*/);
}

CommandManager::Command::Response Application::handleSumCommand(CommandManager::Command *command) {
	if (!validateCommandArgumentCount(command, 2)) {
		return command->createFailureResponse();
	}

	int a = command->getInt(0);
	int b = command->getInt(1);
	int sum = a + b;

	return command->createSuccessResponse(/*sum*/);
}

CommandManager::Command::Response Application::handleLedCommand(CommandManager::Command *command) {
	if (!validateCommandArgumentCount(command, 2)) {
		return command->createFailureResponse();
	}

	int ledIndex = command->getInt(0);
	std::string ledModeRequest = command->getString(1);

	if (ledIndex < 0 || ledIndex > 3) {
		return command->createFailureResponse(/*"expected led index between 0 and 3"*/);
	}

	Debug::LedMode ledMode = Debug::LedMode::OFF;

	if (ledModeRequest == "OFF") {
		ledMode = Debug::LedMode::OFF;
	} else if (ledModeRequest == "ON") {
		ledMode = Debug::LedMode::ON;
	} else if (ledModeRequest == "BLINK_SLOW") {
		ledMode = Debug::LedMode::BLINK_SLOW;
	} else if (ledModeRequest == "BLINK_FAST") {
		ledMode = Debug::LedMode::BLINK_FAST;
	} else if (ledModeRequest == "BLINK_ONCE") {
		ledMode = Debug::LedMode::BLINK_ONCE;
	} else if (ledModeRequest == "BREATHE") {
		ledMode = Debug::LedMode::BREATHE;
	} else {
		return command->createFailureResponse(/*"unsupported led mode requested"*/);
	}

	debug.setLedMode(ledIndex, ledMode);

	return command->createSuccessResponse();
}
