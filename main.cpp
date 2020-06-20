#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <exception>
#include <cstdarg>
#include <assert.h>
#include "main.h"

using namespace std::chrono_literals;

#define SEND_MESSAGES   30  /* The number of messages to send during the demo. */



// === COMMON ==================================================================
static void unsafeOut(const char *format, ...)
{
    const size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];

    std::va_list argptr;
    va_start(argptr, format);
    int written = std::vsnprintf(buffer, BUFFER_SIZE, format, argptr);
    va_end(argptr);

    buffer[BUFFER_SIZE - 1] = '\0';

    if (written > 0)
    {
        std::cout << buffer << std::endl << std::flush;
    }
}

#if defined(LOG)
#   define DEBUG_LOG_SAFE(info, fmt, ...)   info->writer->write(fmt, ##__VA_ARGS__)
#   define DEBUG_LOG_UNSAFE(fmt, ...)       unsafeOut(fmt, ##__VA_ARGS__)
#else
#   define DEBUG_LOG_SAFE(info, fmt, ...)   /* No logging */
#   define DEBUG_LOG_UNSAFE(fmt, ...)       /* No logging */
#endif




// === THREADINFO ==============================================================
ThreadInfo::ThreadInfo() :
    producer(nullptr),
    consumer(nullptr),
    queue(nullptr),
    writer(nullptr)
{

}

void ThreadInfo::initialize(std::shared_ptr<ThreadInfo> info)
{
    producer = new Thread();
    consumer = new Thread();
    queue = new ThreadMessageQueue(info);
    writer = new FileWriter(std::make_shared<File>());
}

ThreadInfo::~ThreadInfo()
{
    delete producer;
    delete consumer;
    delete queue;
    delete writer;
}



// === TIMER ===================================================================
Timer::Timer(std::shared_ptr<ThreadInfo> info, std::string name) :
    info(info),
    name(name)
{
    start = std::chrono::high_resolution_clock::now();
}

Timer::~Timer()
{
    end = std::chrono::high_resolution_clock::now();

    std::chrono::duration<double, std::milli> elapsed = end - start;
    DEBUG_LOG_SAFE(info, "[%s] Elapsed time: %g ms", name.c_str(), elapsed.count());
}



// === MUTEXLOCK ===============================================================
MutexLock::MutexLock(std::mutex *mutex) :
    m_mutex(mutex)
{
    if (m_mutex)
    {
        m_mutex->lock();
    }
}

MutexLock::~MutexLock()
{
    if (m_mutex)
    {
        m_mutex->unlock();
    }
}



// === FILE ====================================================================
File::File(std::string filepath) :
    m_stream(filepath, std::ios::in | std::ios::ate | std::ios::binary)
{

}

File::~File()
{
    if (m_stream.is_open())
    {
        m_stream.flush();
        m_stream.close();
    }
}

void File::write(const char *message)
{
    MutexLock lock(&m_mutex);

    if (m_stream.is_open())
    {
        m_stream << message << std::endl << std::flush;
    }

    //std::cout << message << std::endl << std::flush;
}



// === FILEWRITER ==============================================================
FileWriter::FileWriter(std::shared_ptr<File> file) :
    m_file(file)
{
    
}

FileWriter::~FileWriter()
{

}

void FileWriter::write(const char *format, ...)
{
    const size_t BUFFER_SIZE = 2048;
    char buffer[BUFFER_SIZE];

    std::va_list argptr;
    va_start(argptr, format);
    int written = std::vsnprintf(buffer, BUFFER_SIZE, format, argptr);
    va_end(argptr);

    buffer[BUFFER_SIZE - 1] = '\0';

    if (written > 0)
    {      
        m_file->write(buffer);
    }
}



// === THREADMESSAGE ===========================================================
ThreadMessage::ThreadMessage(std::shared_ptr<ThreadInfo> info) :
    m_info(info),
    m_type(ThreadDataType::Invalid),
    m_buffer(nullptr)
{
    DEBUG_LOG_SAFE(m_info, "ThreadMessage - constructor");
}

ThreadMessage::~ThreadMessage()
{
    DEBUG_LOG_SAFE(m_info, "ThreadMessage - destructor");

    if (m_buffer)
    {
        DEBUG_LOG_SAFE(m_info, "Deleting NOT-NULL");
    }
    else
    {
        DEBUG_LOG_SAFE(m_info, "Deleting NULL");
    }

    if (static_cast<int>(this->m_type) & static_cast<int>(ThreadDataType::Array))
    {
        delete[] m_buffer;
    }
    else
    {
        delete m_buffer;
    }
}

ThreadMessage::ThreadMessage(const ThreadMessage &other) noexcept
{
    DEBUG_LOG_SAFE(m_info, "FATAL ERROR - Copy constructor called!");

    this->m_info = other.m_info;
    this->m_buffer = other.m_buffer;
    this->m_type = other.m_type;
}

ThreadMessage& ThreadMessage::operator=(const ThreadMessage &other) noexcept
{
    DEBUG_LOG_SAFE(m_info, "FATAL ERROR - Copy assign operator called!");

    this->m_info = other.m_info;
    this->m_buffer = other.m_buffer;
    this->m_type = other.m_type;
    return *this;
}

ThreadMessage::ThreadMessage(ThreadMessage &&other) noexcept
{
    DEBUG_LOG_SAFE(m_info, "ThreadMessage - move constructor");

    this->m_info = other.m_info;
    //other.m_info = <null>;
    this->m_buffer = other.m_buffer;
    other.m_buffer = nullptr;
    this->m_type = other.m_type;
    other.m_type = ThreadDataType::Invalid;
}

ThreadMessage& ThreadMessage::operator=(ThreadMessage &&other) noexcept
{
    DEBUG_LOG_SAFE(m_info, "ThreadMessage - move assign operator");

    this->m_info = other.m_info;
    //other.m_info = <null>;
    this->m_buffer = other.m_buffer;
    other.m_buffer = nullptr;
    this->m_type = other.m_type;
    other.m_type = ThreadDataType::Invalid;
    return *this;
}

ThreadDataType ThreadMessage::type() const
{
    return m_type;
}

void ThreadMessage::setType(ThreadDataType type)
{
    this->m_type = type;
}

void* ThreadMessage::data() const
{
    return m_buffer;
}

void ThreadMessage::setData(void *data)
{
    this->m_buffer = data;
}



// === THREADMESSAGEQUEUE ======================================================
ThreadMessageQueue::ThreadMessageQueue(std::shared_ptr<ThreadInfo> info) :
    m_info(info)
{

}

ThreadMessageQueue::~ThreadMessageQueue()
{

}

void ThreadMessageQueue::push(ThreadMessage *messages, size_t count)
{
    MutexLock lock(&m_mutex);
    
    for (size_t i = 0; i < count; i++)
    {
        DEBUG_LOG_SAFE(m_info, "push (%zu)", i);

        m_queue.emplace_back(std::move(messages[i]));
    }
}

void ThreadMessageQueue::pop(std::vector<ThreadMessage> &messages)
{
    MutexLock lock(&m_mutex);
    DEBUG_LOG_SAFE(m_info, "pop");

    for (auto it = m_queue.begin(); it != m_queue.end(); it++)
    {
        messages.emplace_back(std::move(*it));
    }

    m_queue.clear();
}



// === THREAD ==================================================================
Thread::Thread() :
    m_thread(nullptr)
{
    // Nothing specific
}

Thread::~Thread()
{
    if (m_thread && m_thread->joinable())
    {
        m_thread->join();
    }

    delete m_thread;
}

void Thread::start(void (*func)(std::shared_ptr<ThreadInfo>), std::shared_ptr<ThreadInfo> info)
{
    m_thread = new std::thread(func, info);
    assert(m_thread->joinable());
    //m_thread->detach();
}

bool Thread::isRunning() const
{
    //throw std::exception(); // Can't be used with detach
    return (m_thread && m_thread->joinable());
}

void Thread::await()
{
    if (m_thread->joinable())
    {
        m_thread->join();
    }
}



void func_producer(std::shared_ptr<ThreadInfo> info)
{
    Timer timer(info, "producer");

    const size_t BUFFER_SIZE = 64;

    for (int i = 0; i < SEND_MESSAGES; i++)
    {
        ThreadMessage message(info);
        message.setType(ThreadDataType::String);
        message.setData(new char[BUFFER_SIZE]);

        char *buffer = static_cast<char*>(message.data());
        std::snprintf(buffer, BUFFER_SIZE, "Hello World! %i", i);
        buffer[BUFFER_SIZE - 1] = '\0';
        //safeOut(buffer);

        info->queue->push(&message, 1);

        DEBUG_LOG_SAFE(info, "producer tick (%i)", i);
    }

    DEBUG_LOG_SAFE(info, "Terminating producer");
}

void func_consumer(std::shared_ptr<ThreadInfo> info)
{
    Timer timer(info, "consumer");

    size_t received = 0;

    //while (info->producer->isRunning())
    while (received < SEND_MESSAGES)
    {
        std::vector<ThreadMessage> messages;
        info->queue->pop(messages);

        for (auto it = messages.begin(); it != messages.end(); it++)
        {
            ThreadMessage &message = *it;

            if (message.type() == ThreadDataType::String)
            {
                char *out = static_cast<char*>(message.data());

                if (out == nullptr)
                {
                    out = "NULL message";
                }

                DEBUG_LOG_SAFE(info, out);
            }
            else DEBUG_LOG_SAFE(info, "NO string");
        }

        received += messages.size();

        DEBUG_LOG_SAFE(info, "total: %2zu of %2zu", received, SEND_MESSAGES);
    }
    
    DEBUG_LOG_SAFE(info, "Terminating consumer");
}

int main(int argc, char **argv)
{
    try
    {
        std::shared_ptr<ThreadInfo> info = std::make_shared<ThreadInfo>();
        info->initialize(info);

        Timer timer(info, "main");

        info->producer->start(&func_producer, info);
        info->consumer->start(&func_consumer, info);

        DEBUG_LOG_SAFE(info, "Awaiting producer...");
        info->producer->await();

        DEBUG_LOG_SAFE(info, "Awaiting consumer...");
        info->consumer->await();

        // NOTE: if it does not wait for 5s an exception has been thrown
        std::this_thread::sleep_for(5s);
        DEBUG_LOG_SAFE(info, "Terminating main");
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
        std::cout << "Terminating main" << std::endl << std::flush;
    }

    return 0;
}
