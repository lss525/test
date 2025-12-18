#include <stdio.h>         // printf(), perror()
#include <stdlib.h>        // exit(), malloc(), free()
#include <string.h>        // strlen(), strcpy(), strcat(), strcmp()
#include <unistd.h>        // read(), write(), close(), lseek()
#include <fcntl.h>         // open(), O_RDONLY, O_WRONLY, O_CREAT
#include <sys/types.h>     // off_t, mode_t, uid_t, etc.
#include <sys/stat.h>      // stat(), lstat(), chmod(), S_IFDIR, etc.
#include <dirent.h>        // opendir(), readdir(), closedir()
#include <time.h>          // localtime(), strftime(), ctime()
#include <pwd.h>           // getpwuid()
#include <grp.h>           // getgrgid()
#include <errno.h>         // errno, perror
#include <linux/limits.h>  // PATH_MAX
#include<libgen.h>         // basename()
#include <sys/ioctl.h>      //获取终端长度
#include <getopt.h>

// 颜色定义
#define COLOR_RESET   "\033[0m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_CYAN    "\033[36m"

//文件信息结构：
typedef struct 
{  
   //文件名：
   char name[256];
   //stat结构体：  
   struct stat lie_buf;
   //符号链接目标：
   char link_count[1024];
   //文件类型标识：
   char file_type;
} File;


typedef struct {
   int _all;
   int _long;
   int _re;
   int _reverse;
   int _time;
   int _one;
   int _human;
   int _inode;
   int _color;
} Options;


// -a: 显示所有文件，包括以点（.）开头的隐藏文件。
// -l: 使用长格式列出文件的详细信息，包括权限、所有者、文件大小和最后修改时间等。
// -R: 递归列出所有子目录中的内容。
// -t: 按时间顺序排序，默认是按修改时间排序，从最新到最旧。
// -r: 反向排序，通常与其他排序选项一起使用（如 -t）。
// -1 每行一个
// -h 人类可读大小
// -i: 显示文件的 inode 号码。


//函数声明：
int lie_compare_name(const void *a,const void *b);
int lie_compare_time(const void *a,const void *b);
void compare_sort(File *file,int count,Options opts);
void lie_ls(const char *path, Options opts,int deep);
Options lie_options(int argc,char *argv[]);
char get_file_sort(mode_t mode);
char* get_file_power(mode_t mode);
const char *human_size(off_t size);
void lie_long(File *file, Options opts);
void Re_ls(const char *path,Options opts,int deep);
const char* get_color_by_type(mode_t mode, const char* name);

// 根据文件类型返回颜色
const char* get_color_by_type(mode_t mode, const char* name) {
    if (S_ISDIR(mode)) {
        return COLOR_BLUE;        // 目录用蓝色
    }
    if (S_ISLNK(mode)) {
        return COLOR_CYAN;        // 符号链接用青色
    }
    if (mode & (S_IXUSR | S_IXGRP | S_IXOTH)) {
        return COLOR_GREEN;       // 可执行文件用绿色
    }
    // 特殊文件类型可以用黄色
    if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode)) {
        return COLOR_YELLOW;      // 特殊文件用黄色
    }
    return "";                    // 普通文件不用颜色
}

void lie_ls(const char *path, Options opts,int deep) {
   if(opts._re){
      Re_ls(path,opts,deep);
   }
   else{
   DIR *dir = opendir(path);
   if (!dir) {
        perror("opendir");
        return;
   }
    
   struct dirent *entry;
    
    // 先收集所有文件信息
   File *files = NULL;
   int count = 0;
   int capacity = 64;
    
   files = malloc(capacity * sizeof(File));
   if (!files) {
        perror("malloc");
        closedir(dir);
        return;
   }
    
   while ((entry = readdir(dir)) != NULL) {
        // 处理 -a 选项：是否显示隐藏文件
        if (!opts._all) {
            // 跳过隐藏文件（以.开头）
            if (entry->d_name[0] == '.') {
                continue;
            }
        }
        
        // 动态扩容
        if (count >= capacity) {
            capacity *= 2;
            File *new_files = realloc(files, capacity * sizeof(File));
            if (!new_files) {
                perror("realloc");
                free(files);
                closedir(dir);
                return;
            }
            files = new_files;
        }
        
        // 填充文件信息
        File *file = &files[count];
        strncpy(file->name, entry->d_name, sizeof(file->name) - 1);
        file->name[sizeof(file->name) - 1] = '\0'; 
        // 构建完整路径
        char full_path[PATH_MAX];
        if (strcmp(path, ".") == 0) {
            snprintf(full_path, sizeof(full_path), "%s", entry->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        }
        
        // 获取文件状态信息
        if (lstat(full_path, &file->lie_buf) == -1) {
            perror("lstat");
            continue;
        }
        
        // 如果是符号链接，获取链接目标
        if (S_ISLNK(file->lie_buf.st_mode)) {
            ssize_t len = readlink(full_path, file->link_count, 
                                  sizeof(file->link_count) - 1);
            if (len != -1) {
                file->link_count[len] = '\0';
            } 
            else {
                strcpy(file->link_count, "???");
            }
        } else {
            file->link_count[0] = '\0';
        }
        
        count++;
   }
    
   closedir(dir);
    
   if (count == 0) {
        free(files);
        return;
   }
   
   compare_sort(files,count,opts);
    // 处理 -1 选项：每行一个
   if (opts._one && !opts._long) {
        for (int i = 0; i < count; i++) {
            const char* color = get_color_by_type(files[i].lie_buf.st_mode, files[i].name);
            printf("%s%s%s\n", color, files[i].name, COLOR_RESET);
        }
        free(files);
        return;
   }
    
   if (opts._long) {
        for (int i = 0; i < count; i++) {
            lie_long(&files[i], opts);
        }
        free(files);
        return;
   }
    
    // 先找出最长文件名
   int max_len = 0;
   for (int i = 0; i < count; i++) {
        int len = strlen(files[i].name);
        if (len > max_len) max_len = len;
   }
    
   struct winsize w;
   int term_width = 80;
   if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        term_width = w.ws_col;
   }
    
   int col_width = max_len + 2;
   int cols = term_width / col_width;
   if (cols == 0) cols = 1;
   int rows = (count + cols - 1) / cols;
    
    // 打印多列
   for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            int idx = row + col * rows;
            if (idx < count) {
                const char* color = get_color_by_type(files[idx].lie_buf.st_mode, files[idx].name);
                printf("%s%-*s%s", color, col_width, files[idx].name, COLOR_RESET);
            }
        }
        printf("\n");
   }
    
   free(files);
}
}

Options lie_options(int argc,char *argv[]){
   Options opts={0};
   opts._color = 1;  // 默认启用颜色显示
   
   int opt;
   optind = 1;
   while((opt=getopt(argc,argv,"alRrt1hi"))!=-1){
      switch(opt){
      case 'a':
      opts._all=1;
      break;
      case 'l':
      opts._long=1;
      break;
      case 'R':
      opts._re=1;
      break;
      case 'r':
      opts._reverse=1;
      break;
      case 't':
      opts._time=1;
      break;
      case '1':
      opts._one=1;
      break;
      case 'h':
      opts._human=1;
      break;
      case 'i':
      opts._inode=1;
      break;
      default:
      fprintf(stderr, "用法: %s [-alRrt1hi] [目录]\n", argv[0]);
      exit(EXIT_FAILURE);
   }
   }
   return opts;
} 


//获取文件类型：
char get_file_sort(mode_t mode){
   if(S_ISREG(mode)){
      return '-';
   }
   if(S_ISDIR(mode)){
      return 'd';
   }
   if(S_ISLNK(mode)){
      return 'l';
   }
   if(S_ISCHR(mode)){
      return 'c';
   }
   if(S_ISBLK(mode)){
      return 'b';
   }
   if(S_ISFIFO(mode)){
      return 'p';
   }
   if(S_ISSOCK(mode)){
      return 's';
   }
   return '?';
} 

//获取文件权限：
char* get_file_power(mode_t mode){

   static char lie_power[10];
   lie_power[0]=(mode & S_IRUSR) ? 'r' : '-';
   lie_power[1]=(mode & S_IWUSR) ? 'w' : '-';
   lie_power[2]=(mode & S_IXUSR) ? 'x' : '-';
   lie_power[3]=(mode & S_IRGRP) ? 'r' : '-';
   lie_power[4]=(mode & S_IWGRP) ? 'w' : '-';
   lie_power[5]=(mode & S_IXGRP) ? 'x' : '-';
   lie_power[6]=(mode & S_IROTH) ? 'r' : '-';
   lie_power[7]=(mode & S_IWOTH) ? 'w' : '-';
   lie_power[8]=(mode & S_IXOTH) ? 'x' : '-';

   lie_power[9]='\0';
   
   return lie_power;
} 

const char *human_size(off_t size){

   static char buf[16];
   const char *units[]={"B","k","M","G","T","P"}; 

   int num1=0;
   double human_size=size;

   //循环判断并且进一：
   while(human_size>=1024&&num1<5){
      human_size/=1024.00;
      num1++;
   }

   if(num1==0){
      snprintf(buf,sizeof(buf),"%ld%s",
                 (long)human_size,units[num1]);
   }

   //若小于10表示为小数：
   else if(human_size<10){
      snprintf(buf, sizeof(buf), "%.1f%s", 
                 human_size, units[num1]);
   }

   //其余则显示为整数：
   else{
      snprintf(buf, sizeof(buf), "%.0f%s", 
                 human_size, units[num1]);
   }

   return buf;
}

void lie_long(File *file, Options opts){
   
   struct stat *st = &file->lie_buf;

   if (opts._inode) {
       printf("%8lu ", (unsigned long)st->st_ino);
   }
   
   // 1. 文件类型和权限:
   printf("%c%s ", get_file_sort(st->st_mode), get_file_power(st->st_mode));

   // 2. 链接数:
   printf("%3ld ", (long)st->st_nlink);

   // 3. 用户和组:
   struct passwd *pw = getpwuid(st->st_uid);
   struct group *gr = getgrgid(st->st_gid);
   printf("%-8s %-8s ", 
          pw ? pw->pw_name : "?", 
          gr ? gr->gr_name : "?");

   // 4. 文件大小:
   if (opts._human) {
       printf("%6s ", human_size(st->st_size));
   } else {
       printf("%8ld ", (long)st->st_size);
   }

   // 5. 修改时间:
   char time_buf[64];
   struct tm *tm = localtime(&st->st_mtime);
   strftime(time_buf, sizeof(time_buf), "%b %d %H:%M", tm);
   printf("%s ", time_buf);

   // 6. 文件名（带颜色）:
   const char* color = get_color_by_type(st->st_mode, file->name);
   printf("%s%s%s", color, file->name, COLOR_RESET);
   
   // 7. 如果是目录，添加 /
   if (S_ISDIR(st->st_mode)) {
       printf("/");
   }

   // 8. 符号链接目标:
   if (S_ISLNK(st->st_mode) && file->link_count[0] != '\0') {
       printf(" -> %s", file->link_count);
   }
    
   printf("\n");
}

int lie_compare_name(const void *a,const void *b){
   const File *fa=(const File *)a;
   const File *fb=(const File *)b;
   return strcmp(fa->name,fb->name);
}

int lie_compare_time(const void *a,const void *b){
   const File *fa=(const File *)a;
   const File *fb=(const File *)b;
   if(fb->lie_buf.st_mtime>fa->lie_buf.st_mtime){
      return 1;
   }

   if(fb->lie_buf.st_mtime<fa->lie_buf.st_mtime){
      return -1;
   }
   return strcmp(fa->name,fb->name);
}

void compare_sort(File *file,int count,Options opts){
   if(count<=1){
      return;
   }
   if(opts._time){
      qsort(file,count,sizeof(File),lie_compare_time);
   }
   else{
      qsort(file,count,sizeof(File),lie_compare_name);
   }
   if(opts._reverse){
      for(int i=0;i<count/2;i++){
         File temp=file[i];
         file[i]=file[count-1-i];
         file[count-1-i]=temp;
      }
   }
}
 
//递归实现
void Re_ls(const char *path,Options opts,int deep){
   if(deep>10){
      fprintf(stderr, "递归深度超过限制: %s\n", path);
      return ;
   }
   
   DIR *dir=opendir(path);
   if(!dir){
      perror("opendir");
      return ;
   }
   
   struct dirent *entry;
    
    // 先收集所有文件信息
   File *files = NULL;
   int count = 0;
   int capacity = 64;
    
   files = malloc(capacity * sizeof(File));
   if (!files) {
        perror("malloc");
        closedir(dir);
        return;
   }
   
   while((entry=readdir(dir))!=NULL){
      if (!opts._all) {
         if (entry->d_name[0] == '.') {
            continue;
         }
      }
      
      if(count>=capacity){
         capacity *=2;
         File *new_files=realloc(files,capacity * sizeof(File));
         if(!new_files){
            perror("realloc");
            free(files);
            closedir(dir);
            return ;
         }
         files=new_files;
      }
      
      File *file = &files[count];
      strncpy(file->name, entry->d_name, sizeof(file->name) - 1);
      file->name[sizeof(file->name) - 1] = '\0';
      
      char full_path[PATH_MAX];
      if(strcmp(path,".")==0){
         snprintf(full_path,sizeof(full_path),"%s",entry->d_name);
      }
      else{
         snprintf(full_path,sizeof(full_path),"%s/%s",path,entry->d_name);
      }
      
      if (lstat(full_path, &file->lie_buf) == -1) {
         // 如果是文件不存在的错误，跳过不显示
         if (errno == ENOENT) {
            continue;
         }
         perror("lstat");
         continue;
      }
      
      if (S_ISLNK(file->lie_buf.st_mode)) {
         ssize_t len = readlink(full_path, file->link_count, 
                               sizeof(file->link_count) - 1);
         if (len != -1) {
            file->link_count[len] = '\0';
         } 
         else {
            strcpy(file->link_count, "???");
         }
      } 
      else {
         file->link_count[0] = '\0';
      }
      
      count++;
   }
   
   closedir(dir);
   
   if (count == 0) {
      free(files);
      return;
   }
   
   compare_sort(files,count,opts);

   if(deep>0){
      printf("\n%s:", path); 
   }
   
   if(opts._long){
      for(int i=0;i<count;i++){
         lie_long(&files[i],opts);
      }
   }
   else if(opts._one){ 
      for(int i=0;i<count;i++){
         const char* color = get_color_by_type(files[i].lie_buf.st_mode, files[i].name);
         printf("%s%s%s\n", color, files[i].name, COLOR_RESET);
      }
   }
   else{
      int max_len = 0;
      for (int i = 0; i < count; i++) {
         int len = strlen(files[i].name);
         if (len > max_len) max_len = len;
      }
      
      struct winsize w;
      int term_width = 80;
      if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
         term_width = w.ws_col;
      }
      
      int col_width = max_len + 2;
      int cols = term_width / col_width;
      if (cols == 0) cols = 1;
      int rows = (count + cols - 1) / cols;
      
      // 打印多列
      for (int row = 0; row < rows; row++) {
         for (int col = 0; col < cols; col++) {
            int idx = row + col * rows;
            if (idx < count) {
               const char* color = get_color_by_type(files[idx].lie_buf.st_mode, files[idx].name);
               printf("%s%-*s%s", color, col_width, files[idx].name, COLOR_RESET);
            }
         }
      }
   }
   
   // 递归处理子目录
   for(int i=0;i<count;i++){
      if(strcmp(files[i].name,".")==0||strcmp(files[i].name,"..")==0){
         continue;
      }
      
      if (S_ISDIR(files[i].lie_buf.st_mode)) {
         char new_path[PATH_MAX];
         if(strcmp(path,".")==0){
            snprintf(new_path,sizeof(new_path),"%s",files[i].name);
         }
         else{
            snprintf(new_path,sizeof(new_path),"%s/%s",path,files[i].name);
         }
         Re_ls(new_path, opts, deep+1);
      }
   }
   
   free(files);
}

int main(int argc, char *argv[]) {
   Options opts = lie_options(argc, argv);
   
   if (optind >= argc) {
      lie_ls(".", opts, 0);
   } 
   else {
      for (int i = optind; i < argc; i++) {
         if (argc - optind > 1) {
            printf("%s:\n", argv[i]);
         }
         lie_ls(argv[i], opts, 0);
         if (argc - optind > 1 && i < argc - 1) {
         }
      }
   }
   
   return 0;
}