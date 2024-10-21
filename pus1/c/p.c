#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        char input[100];
        printf("Num: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }

        // remove \n
        input[strcspn(input, "\n")] = '\0';

        char *endptr;
        const double number = strtod(input, &endptr);

        // Check if conversion was successful
        if (*endptr != '\0') {
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        // Check if feet is less than 0, equal to 0, or greater than 0
        if (number < 0) {
            printf("The number is less than 0.\n");
        } else if (number == 0) {
            printf("The number is equal to 0.\n");
        } else {
            printf("The number is greater than 0.\n");
        }
    }
}
