#include <iostream>
#include <queue>
#include <random>
#include <chrono>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <climits>
using namespace std;
#define SIMULATION_TIME 10.0

// ANSI escape codes for text colors
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

int user_exit = 0;
int passenger_number = 1;
double gt = 0.0;
std::mutex log_mtx;

double generateExponentialTime(double lambda)
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::exponential_distribution<double> exponential(lambda);
    return exponential(gen);
}

class Passenger
{
public:
    int _id;
    double _interArrivalTime;
    double _globalArrivalTime; // Time it takes for the passenger to arrive at the checkpoint (in milliseconds).
    double _processing_time;   // Time it takes for the security scanner to process the passenger (in milliseconds).
    Passenger(double arrival_rate, double service_rate)
    {
        std::default_random_engine generator(std::random_device{}());
        std::exponential_distribution<double> arrival_distribution(arrival_rate);
        std::exponential_distribution<double> processing_distribution(service_rate);

        _id = passenger_number++;
        _interArrivalTime = arrival_distribution(generator);
        // _interArrivalTime = (generateExponentialTime(arrival_rate) );
        // _interArrivalTime = static_cast<int>(arrival_distribution(generator) * 1000); // Convert to milliseconds.
        gt += _interArrivalTime;
        _globalArrivalTime = gt;
        _processing_time = processing_distribution(generator);
        // _processing_time = (generateExponentialTime(service_rate) );
        // _processing_time = static_cast<int>(processing_distribution(generator) * 1000); // Convert to milliseconds.
        log_mtx.lock();
        std::cout << "\n\nPassenger Number : " << _id << "\nArrival Time : " << _interArrivalTime << ", Service Time : " << _processing_time;
        log_mtx.unlock();
    }
};

int passenger_count = 0;
int queue_length = 0;
double previous_time = 0.0;
double total_service = 0.0;
int single_buffer_size;
double arrival_rate = 5.0; // λ
double total_waiting_time = 0.0;
double service_rate = 6.0; // μ
double start_processing_time = 0.0;
double total_queue_length = 0.0;
int total_passengers = 0;
int K = 100;
int m; // No. of scanners
vector<double> startProcessingTimeForMultipleServers(1000, 0.0);

std::queue<Passenger> securityLineForSingleServer;
vector<queue<Passenger>> securityLineForMultipleServers(1000);

vector<mutex> mtx_vec(1000);

void initializeVariables()
{
    passenger_number = 1;
    gt = 0.0;
    passenger_count = 0;
    queue_length = 0;
    previous_time = 0.0;
    total_service = 0.0;
    total_waiting_time = 0.0;
    start_processing_time = 0.0;
    total_queue_length = 0.0;
    total_passengers = 0;
    startProcessingTimeForMultipleServers.resize(m);
    securityLineForMultipleServers.resize(m);
}

bool flag = false;
std::mutex mtx;

// Function to simulate passenger arrivals.
void simulateArrivals_singleServer_infiniteBuffers()
{
    while (true)
    {
        Passenger passenger(arrival_rate, service_rate);

        if (gt > SIMULATION_TIME)
        {
            flag = true;
            break;
        }

        // std::this_thread::sleep_for(std::chrono::seconds(passenger._interArrivalTime));
        std::this_thread::sleep_for(std::chrono::duration<double>(passenger._interArrivalTime));

        mtx.lock();

        securityLineForSingleServer.push(passenger);
        total_queue_length += (passenger._globalArrivalTime - previous_time) * queue_length;
        queue_length++;
        previous_time = passenger._globalArrivalTime;
        start_processing_time = std::max(start_processing_time, gt);

        mtx.unlock();
    }
}

void simulateProcessing_singleServer_infiniteBuffers()
{
    while (true)
    {
        if (flag)
            break;

        mtx.lock();

        double processing_time = 0;

        if (!securityLineForSingleServer.empty())
        {
            Passenger passenger = securityLineForSingleServer.front();
            processing_time = passenger._processing_time;
            securityLineForSingleServer.pop();
            total_queue_length += queue_length * (start_processing_time + passenger._processing_time - previous_time);
            queue_length--;
            previous_time = start_processing_time + passenger._processing_time;
            total_waiting_time += start_processing_time - passenger._globalArrivalTime;

            log_mtx.lock();
            std::cout << "\nStart Processing Time for passenger " << passenger._id << " : " << start_processing_time;
            std::cout << "\nWaiting time for passenger " << passenger._id << " : " << start_processing_time - passenger._globalArrivalTime;
            log_mtx.unlock();

            start_processing_time += processing_time;
            passenger_count++;
            total_service += processing_time;
        }

        mtx.unlock();
        // std::this_thread::sleep_for(std::chrono::milliseconds(processing_time));
        std::this_thread::sleep_for(std::chrono::duration<double>(processing_time));
    }
}

void simulateArrivals_singleServer_finiteBuffer()
{
    while (true)
    {
        Passenger passenger(arrival_rate, service_rate);

        if (gt > SIMULATION_TIME)
        {
            flag = true;
            break;
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(passenger._interArrivalTime));
        std::this_thread::sleep_for(std::chrono::duration<double>(passenger._interArrivalTime));

        if (queue_length < single_buffer_size)
        {
            mtx.lock();

            securityLineForSingleServer.push(passenger);
            total_queue_length += (passenger._globalArrivalTime - previous_time) * queue_length;
            queue_length++;
            previous_time = passenger._globalArrivalTime;
            start_processing_time = std::max(start_processing_time, gt);

            mtx.unlock();
        }
    }
}

void simulateProcessing_singleServer_finiteBuffer()
{
    while (true)
    {
        if (flag)
            break;

        mtx.lock();
        int processing_time = 0;

        if (!securityLineForSingleServer.empty())
        {
            Passenger passenger = securityLineForSingleServer.front();
            processing_time = passenger._processing_time;
            securityLineForSingleServer.pop();
            total_queue_length += queue_length * (start_processing_time + passenger._processing_time - previous_time);
            queue_length--;
            previous_time = start_processing_time + passenger._processing_time;
            total_waiting_time += start_processing_time - passenger._globalArrivalTime;

            log_mtx.lock();
            std::cout << "\nStart Processing Time for passenger : " << start_processing_time;
            log_mtx.unlock();

            start_processing_time += processing_time;
            passenger_count++;
            total_service += processing_time;
        }

        mtx.unlock();
        // std::this_thread::sleep_for(std::chrono::milliseconds(processing_time));
        std::this_thread::sleep_for(std::chrono::duration<double>(processing_time));
    }
}

// CASE 3
void simulateArrivals_multiServer_infiniteBuffer()
{
    while (true)
    {
        Passenger passenger(arrival_rate, service_rate);
        if (gt > SIMULATION_TIME)
        {
            flag = true;
            break;
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(passenger._interArrivalTime));
        std::this_thread::sleep_for(std::chrono::duration<double>(passenger._interArrivalTime));
        for (int i = 0; i < m; i++)
            mtx_vec[i].lock();

        int minq = -1;
        int minlen = INT_MAX;

        for (int i = 0; i < m; i++)
        {
            if (securityLineForMultipleServers[i].size() < minlen)
            {
                minlen = securityLineForMultipleServers[i].size();
                minq = i;
            }
        }

        for (int i = 0; i < m; i++)
        {
            if (i == minq)
                continue;
            mtx_vec[i].unlock();
        }

        securityLineForMultipleServers[minq].push(passenger);
        previous_time = passenger._globalArrivalTime;
        startProcessingTimeForMultipleServers[minq] = max(startProcessingTimeForMultipleServers[minq], gt);

        mtx_vec[minq].unlock();
    }
}

void simulateProcessing_multiServer_infiniteBuffers(int index)
{
    while (true)
    {
        if (flag)
            break;

        mtx_vec[index].lock();
        int processing_time = 0;
        if (!securityLineForMultipleServers[index].empty())
        {
            Passenger passenger = securityLineForMultipleServers[index].front();
            processing_time = passenger._processing_time;
            log_mtx.lock();
            std::cout << "\nStart Processing Time for passenger : " << startProcessingTimeForMultipleServers[index];
            std::cout << "\nWaiting time for passenger : " << startProcessingTimeForMultipleServers[index] - passenger._globalArrivalTime;
            log_mtx.unlock();
            securityLineForMultipleServers[index].pop();
            total_waiting_time += startProcessingTimeForMultipleServers[index] - passenger._globalArrivalTime;
            startProcessingTimeForMultipleServers[index] += processing_time;
            passenger_count++;
            total_service += processing_time;
        }

        mtx_vec[index].unlock();
        // std::this_thread::sleep_for(std::chrono::milliseconds(processing_time));
        std::this_thread::sleep_for(std::chrono::duration<double>(processing_time));
        // Process the passenger.
    }
}

// CASE 4
void simulateArrivals_multiServer_finiteBuffer()
{
    while (true)
    {
        Passenger passenger(arrival_rate, service_rate);
        if (gt > SIMULATION_TIME)
        {
            flag = true;
            break;
        }

        // std::this_thread::sleep_for(std::chrono::milliseconds(passenger._interArrivalTime));
        std::this_thread::sleep_for(std::chrono::duration<double>(passenger._interArrivalTime));
        for (int i = 0; i < m; i++)
        {
            mtx_vec[i].lock();
        }
        int minq = -1;
        int minlen = INT_MAX;
        for (int i = 0; i < m; i++)
        {
            if (securityLineForMultipleServers[i].size() < minlen)
            {
                minlen = securityLineForMultipleServers[i].size();
                minq = i;
            }
        }
        for (int i = 0; i < m; i++)
        {
            if (i == minq)
                continue;
            mtx_vec[i].unlock();
        }

        if (securityLineForMultipleServers[minq].size() < K)
        {
            securityLineForMultipleServers[minq].push(passenger);
            previous_time = passenger._globalArrivalTime;
            startProcessingTimeForMultipleServers[minq] = max(startProcessingTimeForMultipleServers[minq], gt);
        }
        mtx_vec[minq].unlock();
    }
}

void printSimulation_singleServer()
{
    cout << YELLOW;
    std::cout << "\n\nAverage waiting time (theoretical) : " << (arrival_rate) / (service_rate * (service_rate - arrival_rate)) << " seconds.";
    std::cout << "\nAverage waiting time (simulation) : " << total_waiting_time / passenger_count << " seconds.";

    std::cout << "\nAverage queue length (theoretical) : " << (arrival_rate * arrival_rate) / (service_rate * (service_rate - arrival_rate));
    std::cout << "\nAverage queue length (simulation) : " << total_waiting_time / SIMULATION_TIME;

    std::cout << "\nService Utilization (theoretical) : " << arrival_rate / service_rate;
    std::cout << "\nService Utilization (simulation) : " << total_service / SIMULATION_TIME;
    cout << "\n\n";
    cout << RESET;
}

void printSimulation_multiServer()
{
    cout << YELLOW;
    // std::cout << "\n\nTheoretical average waiting time " << 1000 * arrival_rate / (m * service_rate * (m * service_rate - arrival_rate));
    std::cout << "\n\nAverage waiting time (simulation) : " << total_waiting_time / passenger_count << " \n";

    // std::cout << "Theoretical average queue length " << (arrival_rate * arrival_rate) / (m * service_rate * (m * service_rate - arrival_rate));
    std::cout << "Average queue length (simulation) : " << total_waiting_time / SIMULATION_TIME << "\n";

    // std::cout << "Theoretical Service Utilization " << arrival_rate / (m * service_rate) << "\n";
    std::cout << "Service Utilization (simulation) : " << total_service / SIMULATION_TIME << "\n"
              << RESET;
}

void simulate_singleServer_infiniteBuffers()
{
    initializeVariables();

    // Create arrival and processing threads.
    std::thread arrivals(simulateArrivals_singleServer_infiniteBuffers);
    std::thread processing_thread(simulateProcessing_singleServer_infiniteBuffers);

    // Wait for the threads to finish.
    arrivals.join();
    processing_thread.join();

    printSimulation_singleServer();
}

void simulate_singleServer_finiteBuffer()
{
    initializeVariables();

    std::thread arrivals(simulateArrivals_singleServer_finiteBuffer);
    std::thread processing_thread(simulateProcessing_singleServer_finiteBuffer);

    // Wait for the threads to finish.
    arrivals.join();
    processing_thread.join();

    printSimulation_multiServer();
}

void simulate_multiServer_infiniteBuffer()
{
    initializeVariables();
    std::thread arrivals(simulateArrivals_multiServer_infiniteBuffer);
    vector<thread> scanners;
    for (int i = 0; i < m; i++)
    {
        thread t1(simulateProcessing_multiServer_infiniteBuffers, i);
        scanners.push_back(move(t1));
    }

    arrivals.join();
    for (int i = 0; i < m; i++)
        scanners[i].join();

    printSimulation_multiServer();
}

void simulate_multipleServer_finiteBuffer()
{
    initializeVariables();
    total_waiting_time = 0.0;
    std::thread arrivals(simulateArrivals_multiServer_finiteBuffer);
    vector<thread> scanners;
    for (int i = 0; i < m; i++)
    {
        thread t1(simulateProcessing_multiServer_infiniteBuffers, i);
        scanners.push_back(move(t1));
    }

    arrivals.join();
    for (int i = 0; i < m; i++)
        scanners[i].join();

    printSimulation_multiServer();
}

int main()
{

    cout << "Enter to simulate \n1 - single server with infinite buffers.\n2 - single server with finite buffers.\n";
    cout << "3 - multiple servers with infinte buffers.\n4 - multiple servers with finite buffers.";
    cout << "\n-1 to exit.\n";
    int x;
    cin >> x;
    if (x == 1)
    {
        cout << "Enter arrival rate : ";
        cin >> arrival_rate;
        cout << "Enter service rate : ";
        cin >> service_rate;
        m = 1;
        simulate_singleServer_infiniteBuffers();
    }
    else if (x == 2)
    {
        cout << "Enter arrival rate : ";
        cin >> arrival_rate;
        cout << "Enter service rate : ";
        cin >> service_rate;
        cout << "Enter buffer size : ";
        cin >> single_buffer_size;
        m = 1;
        simulate_singleServer_finiteBuffer();
    }
    else if (x == 3)
    {
        cout << "Enter arrival rate : ";
        cin >> arrival_rate;
        cout << "Enter service rate : ";
        cin >> service_rate;
        cout << "Enter number of servers : ";
        cin >> m;
        simulate_multiServer_infiniteBuffer();
    }
    else if (x == 4)
    {
        cout << "Enter arrival rate : ";
        cin >> arrival_rate;
        cout << "Enter service rate : ";
        cin >> service_rate;
        cout << "Enter number of servers : ";
        cin >> m;
        cout << "Enter buffer size : ";
        cin >> K;
        simulate_multipleServer_finiteBuffer();
    }

    return 0;
}