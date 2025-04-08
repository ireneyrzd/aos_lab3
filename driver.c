#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Driver: Launching test programs sequentially...\n");
    system("./program1");
    system("./program2");
    system("./program3");
    system("./program4");
    system("./program5");
    return 0;
}
