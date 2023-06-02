#define __FILENAME__                                                       \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 \
                                    : __FILE__)

#define LOG_DEBUG(...)                                                         \
  do {                                                                         \
    fprintf(stderr, "\033[0;32;34mDEBUG @%s/%s():%d(pid = %d) ", __FILENAME__, \
            __func__, __LINE__, getpid());                                     \
    fprintf(stderr, __VA_ARGS__);                                              \
    fprintf(stderr, "\033[0m\n");                                              \
  } while (0)

#define LOG_ERROR(...)                                            \
  do {                                                            \
    fprintf(stderr, "\033[0;32;31mERROR @%s/%s():%d(pid = %d)  ", \
            __FILENAME__, __func__, __LINE__, getpid());          \
    fprintf(stderr, __VA_ARGS__);                                 \
    fprintf(stderr, "\033[0m\n");                                 \
  } while (0)

/*
如果你在main()函數中使用printf()等輸出函數，
那麼這些輸出可能會影響execvp()或execlp()函數的執行。
這是因為這些輸出函數會向標準輸出流(stdout)寫入數據，
而execvp()或execlp()函數會使用標準輸出流作為文件描述符之一。

如果你在execvp()或execlp()函數之前向標準輸出流寫入了數據，
那麼這些數據可能會被當作命令行參數或環境變量傳遞給要執行的程序，
進而導致程序執行失敗。

為了避免這種情況，
你可以在main()函數中使用fprintf(stderr,...)等輸出函數，
將數據寫入標準錯誤流(stderr)而不是標準輸出流(stdout)。
這樣做可以確保execvp()或execlp()函數不會受到輸出數據的影響。
*/