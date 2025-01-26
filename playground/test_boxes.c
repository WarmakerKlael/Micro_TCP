#include <stdio.h>

int main() {
    // Box-drawing characters
    const char top_left[] = "┌";    // U+250C
    const char *top_right = "┐";   // U+2510
    const char *bottom_left = "└"; // U+2514
    const char *bottom_right = "┘"; // U+2518
    const char *horizontal = "─";  // U+2500
    const char *vertical = "│";    // U+2502
    const char *horizontal_Double = "═";    // U+2502

    // Print a box using Unicode characters
    printf("%s%s%s%s%s\n", top_left, horizontal, horizontal, horizontal, top_right);
    printf("%s   %s\n", vertical, vertical);
    printf("%s   %s\n", vertical, vertical);
    printf("%s%s%s%s%s\n", bottom_left, horizontal, horizontal, horizontal, bottom_right);

    return 0;
}
