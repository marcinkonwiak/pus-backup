#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    // ReSharper disable once CppDFAEndlessLoop
    while (1) {
        char input[100];
        printf("Feet: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }

        // remove \n
        input[strcspn(input, "\n")] = '\0';

        char *endptr;
        double feet = strtod(input, &endptr);

        // Check if conversion was successful
        if (*endptr != '\0') {
            printf("Invalid input. Please enter a number.\n");
            continue;
        }

        const double meters = feet * 0.3;
        printf("Meters: %.2f\n", meters);
    }
}

