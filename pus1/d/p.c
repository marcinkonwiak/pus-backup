#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    while (1) {
        char input[100];
        double num1, num2;
        char operator;


        // Get first number
        printf("Enter first number: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        char *endptr;
        num1 = strtod(input, &endptr);

        // Check if first number conversion was successful
        if (*endptr != '\0') {
            printf("Invalid input. Please enter a valid number.\n");
            continue;
        }

        // Get operator
        printf("Enter operator (+, -, *, /): ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }
        operator = input[0]; // take the first character

        // Check if the operator is valid
        if (operator != '+' && operator != '-' && operator != '*' && operator != '/') {
            printf("Invalid operator. Please enter one of +, -, *, /.\n");
            continue;
        }

        // Get second number
        printf("Enter second number: ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            printf("Input error. Please try again.\n");
            continue;
        }
        input[strcspn(input, "\n")] = '\0';
        num2 = strtod(input, &endptr);

        // Check if second number conversion was successful
        if (*endptr != '\0') {
            printf("Invalid input. Please enter a valid number.\n");
            continue;
        }

        // Perform the calculation based on the operator
        double result;
        switch (operator) {
            case '+':
                result = num1 + num2;
                break;
            case '-':
                result = num1 - num2;
                break;
            case '*':
                result = num1 * num2;
                break;
            case '/':
                if (num2 == 0) {
                    printf("Error: Division by zero is not allowed.\n");
                    continue;
                }
                result = num1 / num2;
                break;
            default:
                printf("Unknown operator.\n");
                continue;
        }

        // Print the result
        printf("Result: %.2f\n", result);

        FILE *file = fopen("results.txt", "a");
        // Save the expression and result in the file
        fprintf(file, "%.2f %c %.2f = %.2f\n", num1, operator, num2, result);
        fclose(file); // Close the file
    }
}
