#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <stdlib.h>
#include <fcntl.h>  // For open() flags
#include <unistd.h> // For dup2(), close()
#include <stdbool.h>

// ====================================
//    Global Variables & #defines
// ====================================

#define BUF_SIZE 100
#define LINUX_PWD_MAX 4096
#define ECHO_MAX_WORDS 20

char *g_val = NULL;

// ==================================================================================================
// linked list built for locaal variables
// ==================================================================================================
struct loc_var_node
{
    char *keywords;
    char *value;
    struct loc_var_node *next;
};

struct loc_var_node *head = NULL;
struct loc_var_node *current = NULL;

// ====================================
//     Display Local Variables
// ====================================
void print_env(void)
{
    struct loc_var_node *ptr = head;

    while (ptr != NULL)
    {
        printf("%s=%s\n", ptr->keywords, ptr->value);
        ptr = ptr->next;
    }
}

// ====================================
//     Get Local Variable Value
// ====================================
void get_env(char *key)
{
    struct loc_var_node *ptr = head;

    while (ptr != NULL)
    {
        if (strcmp(ptr->keywords, key) == 0)
        {
            free(g_val);
            g_val = (char *)malloc(strlen(ptr->value) + 1);
            strcpy(g_val, ptr->value);
            return;
        }
        ptr = ptr->next;
    }
}

// ====================================
//      Set Local Variable
// ====================================
void set_loc_env(char *keyword, char *value)
{
    struct loc_var_node *link = (struct loc_var_node *)malloc(sizeof(struct loc_var_node));

    link->keywords = (char *)malloc(strlen(keyword) + 1);
    link->value = (char *)malloc(strlen(value) + 1);
    strcpy(link->keywords, keyword);
    strcpy(link->value, value);
    link->next = NULL;

    if (head == NULL)
    {
        head = link;
        return;
    }

    current = head;
    while (current->next != NULL)
    {
        current = current->next;
    }
    current->next = link;
}

// ====================================
//    Redirection Helpers
// ====================================

// Backup original file descriptors
static int saved_stdin = -1;
static int saved_stdout = -1;
static int saved_stderr = -1;

// ====================================
//      Main Shell Function
// ====================================
int main()
{
    // Input buffer for storing user commands (limited to BUF_SIZE characters)
    char input_buf[BUF_SIZE];

    // Buffer for storing current working directory path (max length LINUX_PWD_MAX)
    char CWD_buf[LINUX_PWD_MAX];

    // Pointer used for tokenizing (splitting) the input string into separate arguments
    char *token;

    // Counter for tracking how many words/arguments were entered in the command
    int echo_words_counter = 0;

    // Array to store parsed command arguments (max ECHO_MAX_WORDS arguments)
    // Example: For "ls -l /home", passed_args would be ["ls", "-l", "/home"]
    char *passed_args[ECHO_MAX_WORDS];

    // Array to store arguments for external commands (one less than passed_args)
    // Used when preparing arguments for execvp() system call
    char *ext_passed_args[ECHO_MAX_WORDS - 1];

    // Counter for tracking '=' signs in local variable assignments
    // Example: In "x=10", equal_counter would be 1
    int equal_counter;

    // Delimiter string used for splitting variable assignments (always "=")
    char *delimeter = "=";

    // Pointer to store the variable name when parsing assignments
    // Example: In "x=10", var_keyword would point to "x"
    char *var_keyword;

    // Pointer to store the variable value when parsing assignments
    // Example: In "x=10", var_value would point to "10"
    char *var_value;

    // Counter for tracking '=' signs in export command arguments
    // Used to validate proper environment variable syntax
    int expo_equal_counter;

    /*************************************************************************************************/
    while (1)
    {
        // ----------------------------------
        //  Prompt and Input
        // ----------------------------------

        // Reset argument counter for each new command
        echo_words_counter = 0; // Counts number of space-separated words in current command

        // Reset equals counters for variable assignment checking
        equal_counter = 0;      // Tracks '=' signs for local variable assignments (like x=10)
        expo_equal_counter = 0; // Tracks '=' signs for exported variables (like export x=10)

        // Display the custom shell prompt with colors:
        // - \033[1;35m sets the color to bright magenta for "My Own Shell:"
        // - \033[1;36m sets the color to bright cyan for the current directory
        // - \033[1;33m sets the color to bright yellow for the ">>" prompt
        // - \033[0m resets the color back to default after the prompt
        printf("\033[1;35mMy Own Shell:\033[1;36m %s \033[1;33m>> \033[0m", getcwd(CWD_buf, sizeof(CWD_buf) - 1));

        // Read user input (max BUF_SIZE chars)
        fgets(input_buf, BUF_SIZE, stdin);

        // Remove trailing newline by replacing with null terminator
        input_buf[strlen(input_buf) - 1] = '\0';

        // Skip empty input (just pressed Enter)
        if (strlen(input_buf) == 0)
            continue;

        // Parse redirection operators
        char *input_file = NULL;
        char *output_file = NULL;
        char *error_file = NULL;

        // ----------------------------------
        //          Parse Input
        // ----------------------------------
        // Clear external args array (set all to NULL)
        for (int j = 0; j < sizeof(ext_passed_args) / sizeof(ext_passed_args[0]); j++)
        {
            ext_passed_args[j] = NULL;
        }

        // Ensure string is null-terminated (remove \n if present)
        input_buf[strcspn(input_buf, "\n")] = '\0';

        // Tokenize input by spaces (first token)
        token = strtok(input_buf, " ");

        // Process all tokens
        while (token != NULL)
        {
            // Store token in both argument arrays
            passed_args[echo_words_counter] = token;
            ext_passed_args[echo_words_counter] = token;

            // Get next token
            token = strtok(NULL, " ");
            echo_words_counter++;
        }

        // NULL-terminate external args array (for execvp)
        ext_passed_args[echo_words_counter] = NULL;

        // ----------------------------------
        //     Local Variable Assignment
        // ----------------------------------
        bool valid_assignment = true;
        int equal_signs = 0;

        // Check first argument for proper variable assignment syntax
        for (char *ch = passed_args[0]; *ch != '\0'; ch++)
        {
            if (*ch == '=')
            {
                equal_signs++;
            }
            // Invalid if contains spaces or multiple '='
            if (*ch == ' ' || equal_signs > 1)
            {
                valid_assignment = false;
                break;
            }
        }

        if (valid_assignment && equal_signs == 1) // Valid var=value format found
        {
            // Extract the variable name (left side of '=')
            var_keyword = strtok(passed_args[0], delimeter);
            if (var_keyword == NULL) // No name before '='
            {
                fprintf(stderr, "Error: Missing variable name\n");
                continue; // Skip to next command
            }

            // Extract the variable value (right side of '=')
            var_value = strtok(NULL, delimeter);
            if (var_value == NULL) // No value after '='
            {
                fprintf(stderr, "Error: Missing variable value\n");
                continue; // Skip to next command
            }

            // Store the variable in local environment
            set_loc_env(var_keyword, var_value);
            continue; // Skip rest of processing after assignment
        }

        // ----------------------------------
        //   Built-in Commands:
        // ----------------------------------

        /* echo */
        if (strcmp(passed_args[0], "echo") == 0)
        {
            char *echo_args[ECHO_MAX_WORDS]; // Array to store processed arguments
            int echo_arg_count = 0;          // Counter for processed arguments

            // Backup original file descriptors
            int saved_stdout = dup(STDOUT_FILENO);
            int saved_stderr = dup(STDERR_FILENO);

            /* Parse command arguments looking for:
             * 1. Redirection operators (<, >, 2>)
             * 2. Environment variables ($VAR)
             * 3. Regular arguments
             */
            for (int i = 1; i < echo_words_counter; i++)
            {
                // Check for input redirection (though echo doesn't use input)
                if (strcmp(passed_args[i], "<") == 0 && i + 1 < echo_words_counter)
                {
                    input_file = passed_args[i + 1]; // Store input filename
                    i++;                             // Skip the filename in next iteration
                }
                // Check for output redirection
                else if (strcmp(passed_args[i], ">") == 0 && i + 1 < echo_words_counter)
                {
                    output_file = passed_args[i + 1]; // Store output filename
                    i++;                              // Skip the filename
                }
                // Check for error redirection
                else if (strcmp(passed_args[i], "2>") == 0 && i + 1 < echo_words_counter)
                {
                    error_file = passed_args[i + 1]; // Store error filename
                    i++;                             // Skip the filename
                }
                // Handle regular arguments and variables
                else
                {
                    // Check for environment variables ($ prefix)
                    if (passed_args[i][0] == '$')
                    {
                        get_env(passed_args[i] + 1);         // Get variable value (skip $)
                        echo_args[echo_arg_count++] = g_val; // Store the value
                    }
                    else
                    {
                        echo_args[echo_arg_count++] = passed_args[i]; // Store regular arg
                    }
                }
            }
            echo_args[echo_arg_count] = NULL; // NULL-terminate the argument array

            /* Apply output redirection if specified */
            if (output_file)
            {
                int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDOUT_FILENO); // Redirect stdout to file
                close(fd);
            }

            /* Apply error redirection if specified */
            if (error_file)
            {
                int fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                dup2(fd, STDERR_FILENO); // Redirect stderr to file
                close(fd);
            }

            /* Print all processed arguments */
            for (int i = 0; i < echo_arg_count; i++)
            {
                printf("%s ", echo_args[i]);
            }
            printf("\n");

            /* Restore original file descriptors */
            dup2(saved_stdout, STDOUT_FILENO);
            dup2(saved_stderr, STDERR_FILENO);
            close(saved_stdout);
            close(saved_stderr);
        }
        /* exit */
        else if (strcmp(passed_args[0], "exit") == 0)
        {
            printf("Good Bye :)\n");
            break;
        }

        /* pwd */
        else if (strcmp(passed_args[0], "pwd") == 0)
        {
            printf("%s\n", getcwd(CWD_buf, sizeof(CWD_buf) - 1));
        }

        /* cd */
        else if (strcmp(passed_args[0], "cd") == 0)
        {
            /* Check if argument is a variable reference (starts with '$') */
            if (passed_args[1][0] == '$')
            {
                /* Get the variable's value (path) */
                get_env(passed_args[1] + 1); // +1 skips the '$' character

                /* Try changing to directory stored in variable */
                if (chdir(g_val) == -1)          // chdir returns -1 on error
                    printf("Usage: cd /path\n"); // Print error on failure
            }
            else
            {
                /* Try changing to explicitly specified directory */
                if (chdir(passed_args[1]) == -1) // chdir returns -1 on error
                    printf("Usage: cd /path\n"); // Print error on failure
            }
        }

        /* Handle 'export' command for environment variables */
        else if (strcmp(passed_args[0], "export") == 0)
        {
            /* Validate argument syntax */
            bool valid_export = true;
            int equals_count = 0;

            /* Scan argument for proper format */
            for (char *ch = passed_args[1]; *ch != '\0'; ch++)
            {
                if (*ch == '=')
                    equals_count++;
                if (*ch == ' ' || equals_count > 1)
                { // Reject spaces or multiple '='
                    valid_export = false;
                    break;
                }
            }

            /* Case 1: export var=value (explicit assignment) */
            if (valid_export && equals_count == 1)
            {
                var_keyword = strtok(passed_args[1], delimeter);
                var_value = strtok(NULL, delimeter);

                if (!var_keyword || !var_value)
                {
                    fprintf(stderr, "Error: Invalid export syntax. Use: export NAME=value\n");
                }
                else
                {
                    setenv(var_keyword, var_value, 1); // Overwrite if exists
                }
            }
            /* Case 2: export var (export existing local variable) */
            else if (equals_count == 0)
            {
                get_env(passed_args[1]); // Get local variable value
                setenv(passed_args[1], g_val, 1);
            }
            /* Case 3: export printenv (display all variables) */
            else if (strcmp(passed_args[1], "printenv") == 0)
            {
                print_env();
                printf("\n");
            }
            /* Invalid format */
            else
            {
                fprintf(stderr, "Error: Invalid export syntax\n");
            }
        }
        // ----------------------------------
        //         External Commands
        // ----------------------------------
        else // Not a built-in command - execute external program
        {

            // Create clean argument array without redirections
            char *exec_args[ECHO_MAX_WORDS];
            int exec_arg_count = 0;

            for (int i = 0; i < echo_words_counter; i++)
            {
                if (strcmp(passed_args[i], "<") == 0 && i + 1 < echo_words_counter)
                {
                    input_file = passed_args[i + 1];
                    i++; // Skip filename
                }
                else if (strcmp(passed_args[i], ">") == 0 && i + 1 < echo_words_counter)
                {
                    output_file = passed_args[i + 1];
                    i++; // Skip filename
                }
                else if (strcmp(passed_args[i], "2>") == 0 && i + 1 < echo_words_counter)
                {
                    error_file = passed_args[i + 1];
                    i++; // Skip filename
                }
                else
                {
                    exec_args[exec_arg_count++] = passed_args[i];
                }
            }
            exec_args[exec_arg_count] = NULL; // Null-terminate

            pid_t pid = fork();

            if (pid > 0)
            { // Parent process
                wait(NULL);
            }
            else if (pid == 0)
            { // Child process
                // Handle input redirection
                if (input_file)
                {
                    int fd = open(input_file, O_RDONLY);
                    if (fd < 0)
                    {
                        perror("open input file");
                        exit(1);
                    }
                    dup2(fd, STDIN_FILENO);
                    close(fd);
                }

                // Handle output redirection
                if (output_file)
                {
                    int fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        perror("open output file");
                        exit(1);
                    }
                    dup2(fd, STDOUT_FILENO);
                    close(fd);
                }

                // Handle error redirection
                if (error_file)
                {
                    int fd = open(error_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0)
                    {
                        perror("open error file");
                        exit(1);
                    }
                    dup2(fd, STDERR_FILENO);
                    close(fd);
                }

                // Process variable substitution
                for (int i = 0; exec_args[i] != NULL; i++)
                {
                    if (exec_args[i][0] == '$')
                    {
                        get_env(exec_args[i] + 1);
                        exec_args[i] = g_val;
                    }
                }

                // Execute command
                execvp(exec_args[0], exec_args);

                // If execvp returns, there was an error
                perror("execvp failed");
                exit(1);
            }
            else
            {
                perror("fork failed");
            }
        }
    }

    return 0;
}
