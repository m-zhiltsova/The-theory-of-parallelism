#include <iostream>
#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <future>
#include <functional>
#include <random>
#include <fstream>
#include <sstream>
#include <string>
#include <cmath>
#include <stdexcept>
#include <iomanip>

using namespace std;

template <typename T>
class Server {
public:
    Server() = default;
    ~Server() { stop(); }

    void start() {
        stop_source_ = std::stop_source{};
        worker_ = std::jthread(&Server::server_loop, this);
    }

    void stop() {
        if (worker_.joinable()) {
            stop_source_.request_stop();
            cv_.notify_all();
            worker_.join();
        }
    }

    size_t add_task(std::function<T()> func) {
        std::packaged_task<T()> task(std::move(func));
        auto fut = task.get_future();
        std::shared_future<T> shared_fut = fut.share();

        size_t id;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            id = next_id_++;
            results_.emplace(id, shared_fut);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
        return id;
    }

    T request_result(size_t id) {
        std::shared_future<T> fut;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = results_.find(id);
            if (it == results_.end()) {
                throw std::runtime_error("Invalid task id");
            }
            fut = it->second;
            results_.erase(it);
        }
        return fut.get();
    }

private:
    void server_loop() {
        while (!stop_source_.stop_requested()) {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this] {
                return !tasks_.empty() || stop_source_.stop_requested();
            });
            if (stop_source_.stop_requested()) break;
            if (!tasks_.empty()) {
                auto task = std::move(tasks_.front());
                tasks_.pop();
                lock.unlock();
                task();
            }
        }
    }

    std::jthread worker_;
    std::stop_source stop_source_;
    std::queue<std::packaged_task<T()>> tasks_;
    std::unordered_map<size_t, std::shared_future<T>> results_;
    std::mutex mtx_;
    std::condition_variable cv_;
    size_t next_id_ = 1;
};

template<typename T>
T fun_sin(T arg) {
    return std::sin(arg);
}

template<typename T>
T fun_sqrt(T arg) {
    return std::sqrt(arg);
}

template<typename T>
T fun_pow(T x, T y) {
    return std::pow(x, y);
}

void client_thread(Server<double>& server,
                   int client_id,
                   int num_tasks,
                   const std::string& filename) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dist_arg1(-10.0, 10.0);
    std::uniform_real_distribution<double> dist_arg2(1.0, 10.0);
    std::uniform_real_distribution<double> dist_pow(-2.0, 5.0);

    enum class TaskType { SIN, SQRT, POW };
    TaskType type;
    switch (client_id) {
        case 1: type = TaskType::SIN; break;
        case 2: type = TaskType::SQRT; break;
        case 3: type = TaskType::POW; break;
        default: throw std::runtime_error("Invalid client id");
    }

    std::vector<size_t> ids;
    std::vector<double> args1, args2;

    for (int i = 0; i < num_tasks; ++i) {
        std::function<double()> task;
        double arg1, arg2;
        switch (type) {
            case TaskType::SIN:
                arg1 = dist_arg1(gen);
                task = [arg1] { return fun_sin(arg1); };
                args1.push_back(arg1);
                break;
            case TaskType::SQRT:
                arg1 = std::abs(dist_arg1(gen));
                task = [arg1] { return fun_sqrt(arg1); };
                args1.push_back(arg1);
                break;
            case TaskType::POW:
                arg1 = dist_arg2(gen);
                arg2 = dist_pow(gen);
                task = [arg1, arg2] { return fun_pow(arg1, arg2); };
                args1.push_back(arg1);
                args2.push_back(arg2);
                break;
        }
        size_t id = server.add_task(task);
        ids.push_back(id);
    }

    std::ofstream out(filename);
    if (!out) {
        std::cerr << "Cannot open output file " << filename << std::endl;
        return;
    }
    out << std::fixed << std::setprecision(15);
    for (size_t i = 0; i < ids.size(); ++i) {
        double result = server.request_result(ids[i]);
        out << "id: " << ids[i] << ", result: " << result;
        switch (type) {
            case TaskType::SIN:
                out << ", function: sin, arg: " << args1[i];
                break;
            case TaskType::SQRT:
                out << ", function: sqrt, arg: " << args1[i];
                break;
            case TaskType::POW:
                out << ", function: pow, x: " << args1[i] << ", y: " << args2[i];
                break;
        }
        out << "\n";
    }
    out.close();
    std::cout << "Client " << client_id << " finished. Results in " << filename << std::endl;
}

void test_results(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        std::cerr << "Cannot open file for testing: " << filename << std::endl;
        return;
    }
    std::string line;
    int line_num = 0;
    const double eps = 1e-9;

    while (std::getline(in, line)) {
        ++line_num;

        std::string clean_line;
        for (char c : line)
            if (c != ',') clean_line += c;

        std::istringstream iss(clean_line);
        std::string token;
        size_t id;
        double result;
        std::string func;
        double arg1, arg2;
        bool has_arg2 = false;

        if (!(iss >> token >> id)) {
            std::cerr << "Parse error line " << line_num << std::endl;
            continue;
        }
        if (!(iss >> token >> result)) {
            std::cerr << "Parse error line " << line_num << std::endl;
            continue;
        }
        if (!(iss >> token >> func)) {
            std::cerr << "Parse error line " << line_num << std::endl;
            continue;
        }

        if (func == "sin" || func == "sqrt") {
            if (!(iss >> token >> arg1)) {
                std::cerr << "Parse error line " << line_num << std::endl;
                continue;
            }
        } else if (func == "pow") {
            if (!(iss >> token >> arg1)) {
                std::cerr << "Parse error line " << line_num << std::endl;
                continue;
            }
            if (!(iss >> token >> arg2)) {
                std::cerr << "Parse error line " << line_num << std::endl;
                continue;
            }
            has_arg2 = true;
        } else {
            std::cerr << "Unknown function at line " << line_num << std::endl;
            continue;
        }

        double expected;
        if (func == "sin")
            expected = std::sin(arg1);
        else if (func == "sqrt")
            expected = std::sqrt(arg1);
        else // pow
            expected = std::pow(arg1, arg2);

        if (std::abs(result - expected) > eps) {
            std::cerr << "Test FAILED at line " << line_num
                      << ": expected " << expected << ", got " << result << std::endl;
        } else {
            std::cout << "Line " << line_num << " OK" << std::endl;
        }
    }
}

int main() {
    const int N = 100;
    Server<double> server;

    server.start();

    std::thread client1(client_thread, std::ref(server), 1, N, "results_client1.txt");
    std::thread client2(client_thread, std::ref(server), 2, N, "results_client2.txt");
    std::thread client3(client_thread, std::ref(server), 3, N, "results_client3.txt");

    client1.join();
    client2.join();
    client3.join();

    server.stop();

    std::cout << "\nAll clients finished. Starting verification...\n";
    test_results("results_client1.txt");
    test_results("results_client2.txt");
    test_results("results_client3.txt");

    return 0;
}
