# C++ Threading Test
A quick C++ threading test where data is passed between different threads -
producer and consumer - while remaining thread-safe. This is achieved using the standard C++ library only and has to be fast. As such the move operator has been used as part of the demonstration as well.

## Notes
- The code has three threads: main, producer and consumer. The main thread waits till all the other threads are finished. The consumer waits for the producer to be finished. And the producer produces the SEND_MESSAGES number of messages.
- The code assumes the main thread keeps running while the other threads are active. If this is not the case an exception may be thrown in both the producer and consumer threads. The resulting behavior is undefined.
- Mutexes and file output is made thread-safe using RAII.
- The buffers in the messages are guaranteed to be freed. This is done by using RAII to delete the data on an exception and using move instead of copy when pushing message into and popping them out of the vector. Essentially transfering ownership not only of the object but the buffer too. (Keep in mind that if the main thread terminates before the producer or consumer this behavior becomes undefined.)
