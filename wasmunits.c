#include "emscripten.h"
#include "units.h"

EMSCRIPTEN_KEEPALIVE
int convert_unit(char *youHave, char *youWant) {
	int argc = strlen(youWant) ? 4 : 3;
	char *argv[argc];
	
	argv[0] = "units";
	argv[1] = "--terse";
	argv[2] = youHave;
	
	if (strlen(youWant)) {
		argv[3] = youWant;
	}
	
	return unitsHandler(argc, argv);
}