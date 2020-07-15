#ifndef INPUT_OUTPUT_H_
#define INPUT_OUTPUT_H_

/* Tool function */
unsigned long get_time_microseconds();
void print_error(char error_message[]);
void print_string(char string[]);
void print_string_output_fd(char string[],int output_fd);

#endif //INPUT_OUTPUT_H_
