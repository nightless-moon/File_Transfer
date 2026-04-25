#ifndef RESUME_H
#define RESUME_H
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

void save_resume(const char* file_name, size_t received_size);
size_t read_resume(const char* file_name);
void delete_resume(const char* file_name);

#endif