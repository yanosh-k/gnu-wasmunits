#include "emscripten.h"
#include "units.h"

EMSCRIPTEN_KEEPALIVE
int convert_unit(char *youHave, char *youWant) {
	int argc = strlen(youWant) ? 5 : 4;
	char *argv[argc];
	
	argv[0] = "units";
	argv[1] = "--strict";
	argv[2] = "--one-line";
	argv[3] = youHave;
	
	if (strlen(youWant)) {
		argv[4] = youWant;
	}
	
	return unitsHandler(argc, argv);
}