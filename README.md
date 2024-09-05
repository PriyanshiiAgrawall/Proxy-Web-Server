# Proxy Web Server with LRU Cache

## Overview

This project implements a high-performance proxy web server with an LRU (Least Recently Used) caching mechanism. The server handles HTTP GET requests and utilizes semaphores and mutexes for efficient multi-threading. The proxy server improves web request handling by caching frequently accessed web pages, reducing latency and bandwidth usage.

## Features

- **HTTP GET Request Handling**: Supports HTTP GET requests, fetching requested web pages from the internet or the local cache.
- **LRU Caching Mechanism**: Stores frequently accessed web pages to improve load times and reduce bandwidth usage.
- **Multi-threading**: Utilizes multiple threads to handle simultaneous client connections.
- **Thread Synchronization**: Ensures thread-safe operations using semaphores and mutexes.

## Technologies Used

- **Programming Language**: C
- **Libraries**: POSIX threads (pthreads), Standard Template Library (STL)
- **Concurrency**: Semaphores, mutexes, condition variables
- **Network Programming**: Socket programming


## Usage

- The proxy server listens for incoming client connections and handles HTTP GET requests.
- If the requested web page is in the cache, it is served directly from the cache.
- If the requested web page is not in the cache, the server fetches it from the internet, stores it in the cache, and then serves it to the client.
- The cache follows the LRU eviction policy to manage the storage of web pages.


This README provides an overview of your project, instructions for installation and usage, a brief explanation of the code structure, and examples of key functionalities.


