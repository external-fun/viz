#include <semaphore.h>
#include <errno.h>
#include <sys/mman.h>
#include <unistd.h>

#include <thread>
#include <iostream>
#include <stdexcept>
#include <atomic>
#include <string_view>
#include <csignal>

namespace
{
const char *SEM_CHAT  = "/chats";
const char *SEM_WRITE = "/chat_write";
const char *SEM_READ1 = "/chat_one";
const char *SEM_READ2 = "/chat_two";
const char *SHARED_MEM_NAME = "shared_file";

std::atomic<bool> gWasTerminated = false;
}

template <typename T>
constexpr auto Align(T val, T by)
{
  auto rem = val % by != 0;
  return (val / by + rem) * by;
}

void SigTerm(int sig)
{
  gWasTerminated = true;
}

class MemoryView
{
private:
  static constexpr size_t PAGE_SIZE = 4096;
  static constexpr size_t STARTING_SIZE = PAGE_SIZE * 4;

public:
  MemoryView(std::string_view fileName) : m_fileName(fileName), m_capacity(STARTING_SIZE)
  {
    m_fd = shm_open(fileName.data(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if (m_fd == -1)
    {
      throw std::runtime_error(strerror(errno));
    }
    m_data = Allocate(m_capacity);
  }
  
  ~MemoryView() 
  {
    munmap(m_data, m_capacity);
    shm_unlink(m_fileName.data());
  }

  std::string Read()
  {
    return std::string(m_data);
  }

  void Write(std::string_view view)
  {
    memcpy(m_data, view.data(), view.size());
    m_data[view.size()] = '\0';
  }
private:
  void Resize(size_t newSize)
  {
    if (m_capacity >= newSize)
    {
      return;
    }

    munmap(m_data, m_capacity);
    m_capacity = Align(newSize, PAGE_SIZE);
    m_data = Allocate(m_capacity);
  }

  char* Allocate(size_t newSize)
  {
    ftruncate(m_fd, newSize);
    auto mem = mmap(nullptr, newSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_fd, 0);
    if (mem == MAP_FAILED)
    {
      throw std::runtime_error(strerror(errno));
    }
    return reinterpret_cast<char *>(mem);
  }

private:
  std::string_view m_fileName;
  size_t m_capacity = 0;
  int m_fd = 0;
  char * m_data = nullptr;
};

class Chat
{
public:
  Chat() : m_view(SHARED_MEM_NAME)
  {
    InitLocks();
    m_listener = std::thread([this]() { ThreadProc(); });
  }

  ~Chat()
  {
    m_exit.store(true);
    sem_post(m_readSem);
    if (m_listener.joinable())
    {
      m_listener.join();
    }

    if (m_readSem)
    {
      sem_close(m_readSem);
    }

    if (m_readOtherSem)
    {
      sem_close(m_readOtherSem);
    }

    if (m_writeSem)
    {
      sem_close(m_writeSem);
    }

    sem_unlink(SEM_WRITE);
    sem_unlink(SEM_READ1);
    sem_unlink(SEM_READ2);
    sem_unlink(SEM_CHAT);
  }

  void Start()
  {
    std::string input;
    for (; !gWasTerminated.load() ;) 
    {
      getline(std::cin, input);
      if (input == "exit")
      {
        return;
      }

      sem_wait(m_writeSem);
      m_view.Write(input);

      sem_post(m_readOtherSem);
      sem_post(m_writeSem);
    }
  }

private:
  bool IsChatOpen()
  {
    auto sem = sem_open(SEM_CHAT, O_CREAT | O_EXCL, S_IRUSR | S_IWUSR, 1);
    auto res = sem == SEM_FAILED;
    sem_close(sem);
    return res;
  }

  void ThreadProc()
  {
    for (;;)
    {
      sem_wait(m_readSem);
      if (m_exit.load())
      {
        return;
      }

      std::cout << m_view.Read() << std::endl;
    }
  }

  void InitLocks()
  {
    bool chatIsOpen = IsChatOpen();

    m_writeSem = BinSem(SEM_WRITE, 1);
    if (chatIsOpen)
    {
      m_readSem = BinSem(SEM_READ1, 0);
      m_readOtherSem = BinSem(SEM_READ2, 0);
    }
    else
    {
      m_readSem = BinSem(SEM_READ2, 0);
      m_readOtherSem = BinSem(SEM_READ1, 0);
    }
  }

  sem_t *BinSem(const char *name, int val = 0)
  {
    auto sem = sem_open(name, O_CREAT, S_IRUSR | S_IWUSR, val);
    if (sem == SEM_FAILED)
    {
      throw std::runtime_error(strerror(errno));
    }

    return sem;
  }

private:
  std::atomic<bool> m_exit = false;
  std::thread m_listener;
  MemoryView m_view;
  sem_t* m_readSem = nullptr;
  sem_t* m_readOtherSem = nullptr;
  sem_t* m_writeSem = nullptr;
};

int main(int argc, char **argv)
{
  signal(SIGTERM, SigTerm);
  signal(SIGINT, SigTerm);

  try
  {
    Chat chat;
    chat.Start();
  } catch (std::runtime_error ex)
  {
    std::cerr << ex.what() << std::endl;
    return 1;
  }

  return 0;
}