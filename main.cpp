#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

struct Task {
  std::function<void()> function;
};

class TaskScheduler {
private:
  std::queue<Task> taskQueue;
  std::mutex mutex;
  std::condition_variable cv;
  std::vector<std::thread> threads;
  bool stop;
  size_t tasks_count = 0;
  size_t tasks_completed = 0;

  void worker() {
    while (true) {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [this] { return !taskQueue.empty() || stop; });
      if (stop)
        return;

      Task task = std::move(taskQueue.front());
      taskQueue.pop();

      lock.unlock();

      task.function();

      incrementTaskCount();
    }
  }

  void incrementTaskCount() {
    std::lock_guard<std::mutex> lock(mutex);
    ++tasks_completed;
    if (tasks_completed == tasks_count) {
      cv.notify_one();
    }
  }

public:
  TaskScheduler(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
      threads.emplace_back(&TaskScheduler::worker, this);
    }
  }

  ~TaskScheduler() {
    {
      std::unique_lock<std::mutex> lock(mutex);
      stop = true;
    }
    cv.notify_all();

    for (std::thread &thread : threads) {
      thread.join();
    }
  }

  void addTask(Task task) {
    std::lock_guard<std::mutex> lock(mutex);
    taskQueue.push(std::move(task));
    ++tasks_count;
    cv.notify_one();
  }

  void waitForCompletion() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return tasks_completed == tasks_count; });
  }
};

int main() {
  TaskScheduler scheduler(3);
  std::atomic<size_t> counter(0);

  auto process_matrix = [&counter]() {
    for (size_t i = 0; i < 1000; i++) {
      for (size_t j = 0; j < 1000; j++) {
        counter.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  auto start_time = std::chrono::high_resolution_clock::now();
  
  for(size_t i = 0; i < 5; i++) {
    scheduler.addTask({process_matrix});
  }

  scheduler.waitForCompletion();

  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      end_time - start_time);

  std::cout << "Time taken for all tasks to complete: " << duration.count()
            << " milliseconds" << std::endl;
  std::cout << "Counter value: " << counter.load() << std::endl;

  return 0;
}
