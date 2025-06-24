#include "std_lib.h"
#include "kernel.h"

void handle_command(char* buf);
void intToString(int n, char* str);
void reverse(char* str);
char* strstr(char* haystack, char* needle);
void trim(char* str);
void strcat(char* dest, char* src);
void split_string(char* input, char delimiter, char** result, int* count);
void split_command_and_arg(char *str, char **parts, int *count);


void printString(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        int ax = 0x0E00 | str[i];
        int bx = 0x0007; 
        interrupt(0x10, ax, bx, 0, 0);
        i++;
    }
}

void readString(char* buf) {
    int i = 0;
    while (1) {
        int key_press = interrupt(0x16, 0x0000, 0, 0, 0);
        char c = key_press & 0xFF;

        if (c == '\r') { 
            buf[i] = '\0';
            return;
        } else if (c == '\b') { 
            if (i > 0) {
                i--;
                interrupt(0x10, 0x0E00 | '\b', 0x0007, 0, 0);
                interrupt(0x10, 0x0E00 | ' ', 0x0007, 0, 0);
                interrupt(0x10, 0x0E00 | '\b', 0x0007, 0, 0);
            }
        } else {
            if (i < 127) {
                buf[i] = c;
                interrupt(0x10, 0x0E00 | c, 0x0007, 0, 0);
                i++;
            }
        }
    }
}

void clearScreen() {
    interrupt(0x10, 0x0600, 0x0700, 0x0000, 0x184F);
    interrupt(0x10, 0x0200, 0x0000, 0x0000, 0);
}

int main() {
    char buf[128];

    clearScreen();
    printString("LilHabOS - U02\n"); 

    while (true) {
        printString("$> ");
        readString(buf);
        printString("\n");

        if (strlen(buf) > 0) {
            handle_command(buf);
        }
    }
}


void handle_command(char* buf) {
    char* commands[10];
    int cmd_count = 0;
    char pipe_buffer[128];
    char* cmd_parts[2];
    int parts_count;
    int i;

    split_string(buf, '|', commands, &cmd_count);
    if (cmd_count == 0) return;

    clear(pipe_buffer, 128);

    split_command_and_arg(commands[0], cmd_parts, &parts_count);
    if (parts_count > 0 && strcmp(cmd_parts[0], "echo")) {
        if (parts_count > 1) {
            strcpy(pipe_buffer, cmd_parts[1]); 
        }
    } else {
        printString("Error: First command must be 'echo'.\n");
        return;
    }
    
    for (i = 1; i < cmd_count; i++) {
        char temp_buffer[128];
        clear(temp_buffer, 128);
        
        strcpy(temp_buffer, pipe_buffer);
        
        clear(pipe_buffer, 128); 
        split_command_and_arg(commands[i], cmd_parts, &parts_count);
        
        if (parts_count > 0 && strcmp(cmd_parts[0], "grep")) {
            if (parts_count > 1) {
                if (strstr(temp_buffer, cmd_parts[1])) {
                    strcpy(pipe_buffer, temp_buffer);
                }
            }
        } else if (parts_count > 0 && strcmp(cmd_parts[0], "wc")) {
            int lines = 0, words = 0, chars = 0;
            char num_str[10];
            bool in_word = false;
            int j = 0;

            chars = strlen(temp_buffer);
            if (chars > 0) lines = 1;
            
            while(temp_buffer[j] != '\0') {
                if (temp_buffer[j] != ' ' && !in_word) {
                    in_word = true;
                    words++;
                } else if (temp_buffer[j] == ' ') {
                    in_word = false;
                }
                j++;
            }
            strcat(pipe_buffer, " "); 
            intToString(lines, num_str); strcat(pipe_buffer, num_str);
            strcat(pipe_buffer, " ");
            intToString(words, num_str); strcat(pipe_buffer, num_str);
            strcat(pipe_buffer, " ");
            intToString(chars, num_str); strcat(pipe_buffer, num_str);
        } else {
            printString("Unknown or invalid command in pipe.\n");
            return;
        }
    }

    printString(pipe_buffer);
    printString("\n");
}



void reverse(char *str) {
    int i = 0;
    int j = strlen(str) - 1;
    char temp;
    while (i < j) {
        temp = str[i];
        str[i] = str[j];
        str[j] = temp;
        i++;
        j--;
    }
}

void intToString(int n, char* str) {
    int i = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    while (n != 0) {
        str[i++] = mod(n, 10) + '0';
        n = div(n, 10);
    }
    str[i] = '\0';
    reverse(str);
}

char* strstr(char* haystack, char* needle) {
    int i, j;
    if (*needle == '\0') return haystack;
    for (i = 0; haystack[i] != '\0'; ++i) {
        bool match = true;
        for (j = 0; needle[j] != '\0'; ++j) {
            if (haystack[i + j] == '\0' || haystack[i + j] != needle[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            return (haystack + i);
        }
    }
    return NULL;
}

void trim(char* str) {
    char *start = str;
    char *end;
    
    while (*start == ' ') start++;
    
    end = str + strlen(str) - 1;
    while (end >= start && *end == ' ') end--;
    *(end + 1) = '\0';
    
    if (str != start) {
        strcpy(start, str);
    }
}

void strcat(char* dest, char* src) {
    int i = strlen(dest), j = 0;
    while (src[j] != '\0') {
        dest[i++] = src[j++];
    }
    dest[i] = '\0';
}

void split_string(char* input, char delimiter, char** result, int* count) {
    char* token_start = input;
    int i;
    *count = 0;

    while (*input != '\0') {
        if (*input == delimiter) {
            result[*count] = token_start;
            (*count)++;
            *input = '\0';
            token_start = input + 1;
        }
        input++;
    }
    result[*count] = token_start;
    (*count)++;
    
    for (i = 0; i < *count; i++) {
        trim(result[i]);
    }
}

void split_command_and_arg(char *str, char **parts, int *count) {
    char *arg_start;
    trim(str);
    if (strlen(str) == 0) {
        *count = 0;
        return;
    }
    arg_start = str;
    while(*arg_start != ' ' && *arg_start != '\0') {
        arg_start++;
    }
    
    parts[0] = str;
    if (*arg_start == ' ') {
        *arg_start = '\0';
        arg_start++;
        while(*arg_start == ' ') arg_start++;
        parts[1] = arg_start;
        if (strlen(parts[1]) > 0) {
            *count = 2;
        } else {
            *count = 1;
        }
    } else {
        *count = 1;
    }
}
