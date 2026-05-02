#include "../../kernel/panic.h"
#include "cmd_panic.h"

void cmd_panic() {
    panic("Manually triggered by user");
}