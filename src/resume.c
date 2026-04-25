#include "resume.h"

//保存断点续传信息
void save_resume(const char* file_name ,size_t received_size)
{
    char resume_file[512];
    snprintf(resume_file, sizeof(resume_file), "%s.resume", file_name);
    int fd = open(resume_file, O_WRONLY | O_CREAT, 0644);
    if(fd == -1)
    {
        perror("open resume file fall");
        return;
    }
    write(fd, &received_size, sizeof(received_size));
    close(fd);
}

//读取断点续传信息
size_t read_resume(const char* file_name)
{
    char resume_file[512];
    snprintf(resume_file, sizeof(resume_file), "%s.resume", file_name);
    int fd = open(resume_file, O_RDONLY);
    if(fd == -1)
    {
        perror("open resume file fall");
        return 0;
    }
    size_t received_size = 0;
    read(fd, &received_size, sizeof(received_size));
    close(fd);
    return received_size;
}


//删除断点续传信息
void delete_resume(const char* file_name)
{
    char resume_file[512];
    snprintf(resume_file, sizeof(resume_file), "%s.resume", file_name);
    if(remove(resume_file) == -1)
    {
        perror("remove resume file fail");
    }
}

