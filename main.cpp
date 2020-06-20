#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <vector>
#include <exception>
#include <cstdarg>
#include <assert.h>

using namespace std::chrono_literals;


#define DISABLE_COPY(T) \
    T(const T&) = delete; \
    T& operator=(const T&) = delete;

#define DISABLE_MOVE(T) \
    T(T&&) = delete; \
    T& operator=(T&&) = delete;

#define DISABLE_COPY_AND_MOVE(T) \
    DISABLE_COPY(T) \
    DISABLE_MOVE(T)

#if defined(LOG)
#   define DEBUG_LOG(fmt, ...)  safeOut(fmt, ##__VA_ARGS__)
#else
#   define DEBUG_LOG(fmt, ...) /* No logging */
#endif


#define SEND_MESSAGES   30  /* The number of messages to send during the demo. */

class Mutex
{
public:
    Mutex()
    {

    };

    ~Mutex()
    {

    };

    void lock()
    {
        m_mutex.lock();
    };

    bool tryLock()
    {
        return m_mutex.try_lock();
    }

    void unlock()
    {
        m_mutex.unlock();
    };

private:
    std::mutex m_mutex;
};

static std::shared_ptr<Mutex> g_messageMutex;

class MutexLock
{
public:
    MutexLock(std::mutex *mutex) :
        m_mutex(mutex)
    {
        if (m_mutex)
        {
            m_mutex->lock();
        }
    };

    MutexLock(std::shared_ptr<Mutex> mutex) :
        m_mutex(nullptr),
        m_sharedMutex(mutex)
    {
        if (m_sharedMutex)
        {
            m_sharedMutex->lock();
        }
    };

    ~MutexLock()
    {
        if (m_mutex)
        {
            m_mutex->unlock();
        }

        if (m_sharedMutex)
        {
            m_sharedMutex->unlock();
        }
    };

private:
    std::mutex              *m_mutex;
    std::shared_ptr<Mutex>  m_sharedMutex;
};

class File
{
public:
    File(std::string filepath = "output.log") :
        m_stream(filepath, std::ios::in | std::ios::ate | std::ios::binary)
    {
    
    };

    ~File()
    {
        if (m_stream.is_open())
        {
            m_stream.flush();
            m_stream.close();
        }
    }

    void write(const char *message)
    {
        if (m_stream.is_open())
        {
            m_stream << std::string(message) << std::endl << std::flush;
        }
    };

private:
    std::ofstream m_stream;
};

static void safeOut(const char *format, ...)
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
        static std::shared_ptr<Mutex> s_outMutex = std::make_shared<Mutex>();
        MutexLock lock(s_outMutex);

        File file;
        file.write(buffer);

        //std::cout << buffer << std::endl << std::flush;
    }
}

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
    ThreadMessage() :
        m_type(ThreadDataType::Invalid),
        m_buffer(nullptr)
    {
        DEBUG_LOG("ThreadMessage - constructor");
    };

    ~ThreadMessage()
    {
        DEBUG_LOG("ThreadMessage - destructor");

        if (m_buffer)
        {
            DEBUG_LOG("Deleting NOT-NULL");
        }
        else
        {
            DEBUG_LOG("Deleting NULL");
        }

        if (static_cast<int>(this->m_type) & static_cast<int>(ThreadDataType::Array))
        {
            delete[] m_buffer;
        }
        else
        {
            delete m_buffer;
        }
    };

    ThreadMessage(const ThreadMessage &other) noexcept
    {
        DEBUG_LOG("FATAL ERROR - Copy constructor called!");

        this->m_buffer = other.m_buffer;
        this->m_type = other.m_type;
    };

    ThreadMessage& operator=(const ThreadMessage &other) noexcept
    {
        DEBUG_LOG("FATAL ERROR - Copy assign operator called!");

        this->m_buffer = other.m_buffer;
        this->m_type = other.m_type;
        return *this;
    };

    ThreadMessage(ThreadMessage &&other) noexcept
    {
        DEBUG_LOG("ThreadMessage - move constructor");

        this->m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        this->m_type = other.m_type;
        other.m_type = ThreadDataType::Invalid;
    };

    ThreadMessage& operator=(ThreadMessage &&other) noexcept
    {
        DEBUG_LOG("ThreadMessage - move assign operator");

        this->m_buffer = other.m_buffer;
        other.m_buffer = nullptr;
        this->m_type = other.m_type;
        other.m_type = ThreadDataType::Invalid;
        return *this;
    };

    ThreadDataType type() const
    {
        return m_type;
    };

    void setType(ThreadDataType type)
    {
        this->m_type = type;
    };

    void* data() const
    {
        return m_buffer;
    };

    void setData(void *data)
    {
        this->m_buffer = data;
    }

private:
    ThreadDataType m_type;
    void *m_buffer;
};

class ThreadMessageQueue
{
public:
    ThreadMessageQueue()
    {

    };

    ~ThreadMessageQueue()
    {

    };

    DISABLE_COPY_AND_MOVE(ThreadMessageQueue);

    void push(ThreadMessage *messages, size_t count)
    {
        MutexLock lock(g_messageMutex);
        
        for (size_t i = 0; i < count; i++)
        {
            DEBUG_LOG("push (%zu)", i);

            m_queue.emplace_back(std::move(messages[i]));
        }
    };

    void pop(std::vector<ThreadMessage> &messages)
    {
        MutexLock lock(g_messageMutex);
        DEBUG_LOG("pop");

        for (auto it = m_queue.begin(); it != m_queue.end(); it++)
        {
            messages.emplace_back(std::move(*it));
        }

        m_queue.clear();
    };

private:
    std::mutex m_mutex;
    std::vector<ThreadMessage> m_queue;
};

static ThreadMessageQueue g_messageQueue;

class Thread
{
public:
    Thread() :
        m_thread(nullptr)
    {
        // Nothing specific
    };

    ~Thread()
    {
        if (m_thread && m_thread->joinable())
        {
            m_thread->join();
        }

        delete m_thread;
    };

    DISABLE_COPY_AND_MOVE(Thread);

    void start(void (*func)(void))
    {
        m_thread = new std::thread(func);
        assert(m_thread->joinable());
        //m_thread->detach();
    };

    bool isRunning() const
    {
        return (m_thread && m_thread->joinable());
    };

    void await()
    {
        if (m_thread->joinable())
        {
            m_thread->join();
        }
    }

private:
    std::thread *m_thread;
};

static Thread g_threadProducer;
static Thread g_threadConsumer;

void func_producer()
{
    try
    {
        const size_t BUFFER_SIZE = 64;

        for (int i = 0; i < SEND_MESSAGES; i++)
        {
            ThreadMessage message;
            message.setType(ThreadDataType::String);
            message.setData(new char[BUFFER_SIZE]);

            char *buffer = static_cast<char*>(message.data());
            std::snprintf(buffer, BUFFER_SIZE, "Hello World! %i", i);
            buffer[BUFFER_SIZE - 1] = '\0';
            //safeOut(buffer);

            g_messageQueue.push(&message, 1);

            DEBUG_LOG("producer tick (%i)", i);
        }

        safeOut("Terminating producer");
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
    }
}

void func_consumer()
{
    try
    {
        size_t received = 0;

        while (g_threadProducer.isRunning())
        {
            std::vector<ThreadMessage> messages;
            g_messageQueue.pop(messages);

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

                    safeOut(out);
                }
                else DEBUG_LOG("NO string");
            }

            received += messages.size();

            safeOut("total: %2zu of %2zu", received, SEND_MESSAGES);
        }
        
        safeOut("Terminating consumer");
    }
    catch (const std::exception &ex)
    {
        std::cout << ex.what() << std::endl;
    }
}

int main(int argc, char **argv)
{
    g_messageMutex = std::make_shared<Mutex>();

    g_threadProducer.start(&func_producer);
    g_threadConsumer.start(&func_consumer);

    safeOut("Awaiting producer...");
    g_threadProducer.await();

    safeOut("Awaiting consumer...");
    g_threadConsumer.await();

    safeOut("Terminating main");

    return 0;
}
