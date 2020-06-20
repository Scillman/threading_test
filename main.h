#ifndef MAIN_H
#define MAIN_H

#include <mutex>
#include <iostream>
#include <fstream>



#define DISABLE_COPY(T) \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;

#define DISABLE_MOVE(T) \
    T(T&&) = delete; \
    T& operator=(T&&) = delete;

#define DISABLE_COPY_AND_MOVE(T) \
    DISABLE_COPY(T) \
    DISABLE_MOVE(T)



struct ThreadInfo
{
    class Thread                *producer;
    class Thread                *consumer;
    class ThreadMessageQueue    *queue;
    class FileWriter            *writer;
    std::mutex                  outMutex;

    ThreadInfo();
    ~ThreadInfo();

    void initialize(std::shared_ptr<ThreadInfo> info);
};



class Timer
{
public:
    Timer(std::shared_ptr<ThreadInfo> info, std::string name);
    ~Timer();

private:
    std::shared_ptr<ThreadInfo> info;
    std::string name;
    std::chrono::steady_clock::time_point start, end;
};



class MutexLock
{
public:
    MutexLock(std::mutex *mutex);
    ~MutexLock();

private:
    std::mutex *m_mutex;
};



class File
{
public:
    File(std::string filepath = "output.log");
    ~File();

    void write(const char *message);

private:
    std::mutex      m_mutex;
    std::ofstream   m_stream;
};



class FileWriter
{
public:
    FileWriter(std::shared_ptr<File> file);
    ~FileWriter();

    void write(const char *format, ...);

private:
    std::shared_ptr<File> m_file;
};



enum class ThreadDataType : int
{
    Invalid = 0x7FFFFFFF,
    UserDefined = 0,

    Char = 1,
    Byte = 2,
    Short = 4,
    Int = 8,
    Long = 0x10,
    
    Array = 0x10000,

    String = 0x10001 // Char | Array
};



class ThreadMessage
{
public:
    ThreadMessage(std::shared_ptr<ThreadInfo> info);
    ~ThreadMessage();

    ThreadMessage(const ThreadMessage &other) noexcept;
    ThreadMessage& operator=(const ThreadMessage &other) noexcept;

    ThreadMessage(ThreadMessage &&other) noexcept;
    ThreadMessage& operator=(ThreadMessage &&other) noexcept;

    ThreadDataType type() const;
    void setType(ThreadDataType type);

    void* data() const;
    void setData(void *data);

private:
    std::shared_ptr<ThreadInfo> m_info;
    ThreadDataType m_type;
    void *m_buffer;
};



class ThreadMessageQueue
{
public:
    ThreadMessageQueue(std::shared_ptr<ThreadInfo> info);
    ~ThreadMessageQueue();

    DISABLE_COPY_AND_MOVE(ThreadMessageQueue);

    void push(ThreadMessage *messages, size_t count);
    void pop(std::vector<ThreadMessage> &messages);

private:
    std::shared_ptr<ThreadInfo> m_info;
    std::mutex                  m_mutex;
    std::vector<ThreadMessage>  m_queue;
};



class Thread
{
public:
    Thread();
    ~Thread();

    DISABLE_COPY_AND_MOVE(Thread);

    void start(void (__cdecl *func)(std::shared_ptr<ThreadInfo>), std::shared_ptr<ThreadInfo> info);
    bool isRunning() const;
    void await();

private:
    std::thread *m_thread;
};






#endif // MAIN_H
