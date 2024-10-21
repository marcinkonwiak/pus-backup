#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        char input[100];
        printf("Int: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }

        // remove \n
        input[strcspn(input, "\n")] = '\0';

        char *endptr;
        long feet = strtol(input, &endptr, 10);

        // Check if conversion was successful
        if (*endptr != '\0') {
            printf("Invalid input. Please enter an integer.\n");
            continue;
        }

        if (feet % 2 == 0) {
            printf("Even\n");
        } else {
            printf("Odd\n");
        }
    }
}

