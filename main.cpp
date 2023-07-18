#include <iostream>
#include <unistd.h>
#include <mutex>
#include <shared_mutex>
#include <thread>

using namespace std;

struct prices {
    float hydrogen;
    float helium;
    float lithium;
    float berilium;
    float boron;
};

struct portfolio {
    int hydrogen_stocks;
    int helium_stocks;
    int lithium_stocks;
    int berilium_stocks;
    int boron_stocks;
    float cash;
    float worth_peak;
};

prices scenario_prices;
portfolio scenario_portfolio;
float abort_threshold = 0.5;
shared_timed_mutex market_mutex;
mutex portfolio_mutex;

void initialise_scenario(prices* prices, portfolio* portfolio) {
    srand(time(NULL));
    prices->hydrogen = 1.0;
    prices->helium = 1.0;
    prices->lithium = 1.0;
    prices->berilium = 1.0;
    prices->boron = 1.0;
    
    portfolio->hydrogen_stocks = 0;
    portfolio->helium_stocks = 0;
    portfolio->lithium_stocks = 0;
    portfolio->berilium_stocks = 0;
    portfolio->boron_stocks = 0;
    portfolio->cash = 100.0;
    portfolio->worth_peak = 100.0;
};

float establish_boundaries(float original) {
    return original < 0.01 ? 0.01 : original > 100 ? 100 : original;
};

void volatility_agent(float* value) {
    struct timespec request, remaining = {0, 0};
    while (true) {
        request.tv_nsec, remaining.tv_nsec = rand() % 500000000;
        nanosleep(&request, &remaining);
        float multiplier = 0.5 + 1 * (float)((double)rand() / (double)RAND_MAX);
        market_mutex.lock_shared();
        *value = establish_boundaries(*value * multiplier);
        market_mutex.unlock_shared();
    };
};

void market_agent(float* value, int* quantity_owned) {
    struct timespec request, remaining = {0, 0};
    while (true) {
        request.tv_nsec, remaining.tv_nsec = rand() % 500000000;
        nanosleep(&request, &remaining);
        bool buy = rand() > RAND_MAX / 2;
        market_mutex.lock_shared();
        float recorded_value = *value;
        market_mutex.unlock_shared();
        portfolio_mutex.lock();
        if (buy && scenario_portfolio.cash > recorded_value) {
            *quantity_owned = *quantity_owned + 1;
            scenario_portfolio.cash -= recorded_value;
        } else if (!buy && *quantity_owned > 0) {
            *quantity_owned = *quantity_owned - 1;
            scenario_portfolio.cash += recorded_value;
        };
        portfolio_mutex.unlock();
    };
};

void monitor_agent(prices* prices_, portfolio* portfolio_) {
    struct timespec request, remaining = {0, 0};
    while (true) {
        request.tv_nsec, remaining.tv_nsec = rand() % 500000000;
        nanosleep(&request, &remaining);
        prices prices_snapshot;
        portfolio portfolio_snapshot;
        market_mutex.lock();
        prices_snapshot = *prices_; //Safe as struct does not include any pointers
        market_mutex.unlock();
        portfolio_mutex.lock();
        portfolio_snapshot = *portfolio_;
        portfolio_mutex.unlock();
        float worth = prices_snapshot.hydrogen * portfolio_snapshot.hydrogen_stocks +
            prices_snapshot.helium * portfolio_snapshot.helium_stocks + 
            prices_snapshot.lithium * portfolio_snapshot.lithium_stocks + 
            prices_snapshot.berilium * portfolio_snapshot.berilium_stocks + 
            prices_snapshot.boron * portfolio_snapshot.boron_stocks +
            portfolio_snapshot.cash;
        if (worth < portfolio_snapshot.worth_peak * abort_threshold) {
            portfolio_mutex.lock(); //Do not unlock, permanently disable more buying
            portfolio_->cash = worth;
            portfolio_->hydrogen_stocks = 0;
            portfolio_->helium_stocks = 0;
            portfolio_->lithium_stocks = 0;
            portfolio_->berilium_stocks = 0;
            portfolio_->boron_stocks = 0;
            return;
        } else if (worth > portfolio_snapshot.worth_peak) {
            portfolio_->worth_peak = worth;
        };
    };
};

int main() {
    initialise_scenario(&scenario_prices, &scenario_portfolio);
    thread monitor = thread(monitor_agent, &scenario_prices, &scenario_portfolio);
    thread volatility_agents[5] = {thread(volatility_agent, &scenario_prices.hydrogen), thread(volatility_agent, &scenario_prices.helium), 
    thread(volatility_agent, &scenario_prices.lithium), thread(volatility_agent, &scenario_prices.berilium), thread(volatility_agent, &scenario_prices.boron)};
    for (thread& i : volatility_agents) {
        i.detach();
    };
    thread market_agents[5] = {thread(market_agent, &scenario_prices.hydrogen, &scenario_portfolio.hydrogen_stocks), thread(market_agent, &scenario_prices.helium, &scenario_portfolio.helium_stocks), 
    thread(market_agent, &scenario_prices.lithium, &scenario_portfolio.lithium_stocks), thread(market_agent, &scenario_prices.berilium, &scenario_portfolio.berilium_stocks), thread(market_agent, &scenario_prices.boron, &scenario_portfolio.boron_stocks)};
    for (thread& i : market_agents) {
        i.detach();
    };
    monitor.join();
    printf("Final worth when abort occurred: %f\n", scenario_portfolio.cash);
    return 0;
};
