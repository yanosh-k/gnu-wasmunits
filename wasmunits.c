#include "emscripten.h"
#include "units.h"

EMSCRIPTEN_KEEPALIVE
char* convert_unit(char *youHave, char *youWant) {
	int argc = 4;
	char *argv[] = {"units", "--terse", youHave, youWant};
	
	return unitsHandler(argc, argv);
}
