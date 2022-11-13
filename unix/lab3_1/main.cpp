#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <string_view>
#include <vector>
#include <iostream>
#include <fstream>
#include <array>
#include <cassert>
#include <optional>

using ll = int64_t;
using Matrix = std::vector<std::vector<ll>>;
using Vec = std::vector<ll>;

Matrix ReadMatrix(std::ifstream & fin, size_t rows, size_t columns)
{
  Matrix mat(rows, std::vector<ll>(columns, 0));

  for (size_t i = 0; i < rows; i++)
  {
    for (size_t j = 0; j < columns; j++)
    {
      fin >> mat[i][j];
    }
  }

  return mat;
}

Vec ReadVec(std::ifstream & fin, size_t columns)
{
  Vec vec;
  for (size_t i = 0; i < columns; i++)
  {
    ll val;
    fin >> val;
    vec.push_back(val);
  }

  return vec;
}

std::tuple<Matrix, Vec> ReadData(std::string_view filePath)
{
  std::ifstream fin(filePath);
  if (fin.fail())
  {
    std::cerr << "Couldn't find input file" << std::endl;
    exit(1);
  }
  
  size_t rows = 0;
  fin >> rows;
  size_t columns = 0;
  fin >> columns;

  auto mat = ReadMatrix(fin, rows, columns);
  auto vec = ReadVec(fin, columns);
  return {mat, vec};
}

void WriteMatrix(std::string_view filePath, Matrix const & mat)
{
  std::ofstream fout(filePath);
  if (fout.fail())
  {
    std::cerr << "Couldn't find output file" << std::endl;
    exit(1);
  }

  for (size_t i = 0; i < mat.size(); i++)
  {
    for (size_t j = 0; j < mat[i].size(); j++)
    {
      fout << mat[i][j] << " ";
    }
    fout << std::endl;
  }
}

void WriteVec(std::string_view filePath, Vec const & vec)
{
  std::ofstream fout(filePath);
  if (fout.fail())
  {
    std::cerr << "Couldn't find output file" << std::endl;
    exit(1);
  }

  for (auto const & val : vec)
  {
    fout << val << std::endl;
  }
}

class WorkerPool
{
private:
  constexpr static uint64_t WORKER_NUMBER = 8;
  constexpr static size_t MAX_PROCESS_PAYLOAD = 128;

private:
  struct Worker
  {
    int fd;
    pid_t pid;
  };

#pragma pack(push, 1)
  struct Work
  {
    enum {
      Value,
      EOS 
    } type;

    ll index;
    uint64_t size;
    ll row[MAX_PROCESS_PAYLOAD];
    ll vec[MAX_PROCESS_PAYLOAD];
  };

  struct Result
  {
    enum {
      Value,
      EOS
    } type;

    ll index;
    ll value;
  };
#pragma pack(pop)

public:
  WorkerPool()
  {
    SpawnWorkers();
  }

  ~WorkerPool()
  {
    StopWorkers();
  }

  WorkerPool(WorkerPool const &) = delete;
  WorkerPool& operator=(WorkerPool const &) = delete;
  WorkerPool(WorkerPool &&) = default;
  WorkerPool& operator=(WorkerPool &&) = default;

  Vec Multiply(Matrix const & mat, Vec const & vec)
  {
    assert(mat.size() > 0);
    assert(vec.size() > 0);
    assert(mat[0].size() == vec.size());

    PostWork(mat, vec);
    return CollectResults(mat.size());
  }

private:
  void PostWork(Worker const & worker, Vec const & row, Vec const & vec, size_t rowId)
  {
    Work work {
      .size = vec.size(),
      .index = ll(rowId)
    };
    memcpy(work.row, row.data(), row.size() * sizeof(ll));
    memcpy(work.vec, vec.data(), vec.size() * sizeof(ll));
    int res = send(worker.fd, &work, sizeof(Work), 0);
    assert(res > 0);
  }

  void PostWork(Matrix const & mat, Vec const & vec)
  {
    auto const rows = mat.size();
    auto const workload = rows / WORKER_NUMBER;
    auto const rem = rows % WORKER_NUMBER;

    for (size_t workerId = 0; workerId < WORKER_NUMBER; workerId++)
    {
      auto const start = workerId * workload;
      auto const end = (workerId + 1) * workload;

      assert(end <= rows);
      for (size_t rowId = start; rowId < end; rowId++)
      {
        PostWork(m_workers[workerId], mat[rowId], vec, rowId);
      }
    }

    for (size_t i = 0; i < rem; i++)
    {
      size_t rowId = rowId = workload * WORKER_NUMBER + i;
      PostWork(m_workers[i], mat[rowId], vec, rowId);
    }

    for (auto const & worker : m_workers)
    {
      Work work { .type = Work::EOS };
      send(worker.fd, &work, sizeof(Work), 0);
    }
  }

  Vec CollectResults(size_t rowsN)
  {
    Vec vec(rowsN, 0);

    for (auto const & worker : m_workers)
    {
      for (;;)
      {
        Result res;
        recv(worker.fd, &res, sizeof(Result), 0);
        if (res.type == Result::EOS)
        {
          break;
        }
        vec[res.index] = res.value;
      }
    }

    return vec;
  }

  void SpawnWorkers()
  {
    for (size_t i = 0; i < WORKER_NUMBER; i++)
    {
      int fd[2];
      constexpr int PARENT_SOCKET = 0;
      constexpr int CHILD_SOCKET = 1;
      socketpair(AF_UNIX, SOCK_STREAM, 0, fd);

      pid_t pid = fork();
      if (pid == 0)
      {
        close(fd[PARENT_SOCKET]);
        WorkerProc(fd[CHILD_SOCKET]);
      }
      else if (pid > 0)
      {
        close(fd[CHILD_SOCKET]);
        m_workers.push_back(Worker{fd[PARENT_SOCKET], pid});
      }
      else
      {
        close(fd[PARENT_SOCKET]);
        close(fd[CHILD_SOCKET]);
        KillWorkers();
        throw std::runtime_error("Couldn't start ProcessPool");
      }
    }
  }

  void StopWorkers()
  {
    for (auto & worker : m_workers)
    {
      shutdown(worker.fd, SHUT_RDWR);
      close(worker.fd);
      worker.fd = -1;

      int status;
      waitpid(worker.pid, &status, 0);
      if (status != EXIT_SUCCESS)
      {
        std::cerr << "Worker[" << worker.pid << "]" << " didn't exit successfully. The result might be incorrect" << std::endl;
      }
    }
  }

  void KillWorkers()
  {
    for (auto const & worker : m_workers)
    {
      kill(worker.pid, SIGTERM);
    }
  }
  
  std::optional<Work> ReadWork(int fd)
  {
    Work work;
    int res = recv(fd, &work, sizeof(Work), 0);
    if (res == -1)
    {
      std::cerr << strerror(errno) << std::endl;
      return {};
    }

    if (res == 0)
    {
      return {};
    }

    return work;
  }

  void WorkerProc(int fd)
  {
    for (;;)
    {
      auto work = ReadWork(fd);
      if (!work.has_value())
      {
        shutdown(fd, SHUT_RDWR);
        close(fd);
        exit(EXIT_SUCCESS);
      }

      Result result {
        .value = 0
      };
      switch (work->type)
      {
        case Work::EOS:
          result.type = Result::EOS;
          break;
        case Work::Value:
          result.type = Result::Value;
          result.index = work->index;
          for (size_t i = 0; i < work->size; i++)
          {
            result.value += work->row[i] * work->vec[i];
          }
          break;
      }
      send(fd, &result, sizeof(Result), 0);
    }
  }

  std::vector<Worker> m_workers;
};

void usage()
{
  printf("lab3_1 input_file output_file\n");
}

int main(int argc, char** argv)
{
  if (argc != 3)
  {
    usage();
    return 0;
  }

  std::string_view inputFile = argv[1];
  std::string_view outputFile = argv[2];
  try
  {
    auto [mat, vec] = ReadData(inputFile);
    WorkerPool pool;
    WriteVec(outputFile, pool.Multiply(mat, vec));
  }
  catch (std::exception & ex)
  {
    std::cerr << ex.what() << std::endl;
    return 1;
  }

  return 0;
}