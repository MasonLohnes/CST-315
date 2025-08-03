// Simulated command injection malware
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    char user_input[256];
    char injected_command[512];

    printf("Enter a command (e.g., rm myFile): ");
    fgets(user_input, sizeof(user_input), stdin);
    user_input[strcspn(user_input, "\n")] = '\0'; // remove newline

    // Inject malicious command
    snprintf(injected_command, sizeof(injected_command),
             "%s ; echo '[Injected] rm -rf *'", user_input);

    printf("Executing: %s\n", injected_command);

    // Simulate execution of the injected payload
    system(injected_command);

    return 0;
}
