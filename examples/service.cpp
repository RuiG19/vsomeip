#include <memory>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <atomic>
#include <set>
#include <iomanip>

#include <vsomeip/vsomeip.hpp>

#include "sample-ids.hpp"

#define NOTIFY_PERIOD 50 // ms

class service_t {
private:
    std::shared_ptr<vsomeip::application> vsomeip_app;
    std::mutex counter_mutex;
    uint8_t counter;
    std::thread service_worker;
    std::atomic<bool> running;

    void run() {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        while(running) {
            {
                std::lock_guard<std::mutex> lc(counter_mutex);
                std::vector<uint8_t> payload_data = {counter++};
                auto vsomeip_payload = vsomeip::runtime::get()->create_payload(payload_data);
                // std::cout << "service: >notify " << (int)counter << " from 0x1234" << "\n";
                vsomeip_app->notify(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENT_ID, vsomeip_payload);
                // std::cout << "service: >notify " << (int)counter << " from 0x1235" << "\n";
                vsomeip_app->notify(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID, SAMPLE_EVENT_ID, vsomeip_payload);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(NOTIFY_PERIOD));
        }
    }

public:
    bool init() {
        if(!vsomeip_app->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        vsomeip_app->offer_service(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,0,0);
        vsomeip_app->offer_service(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID,0,0);

        std::set<vsomeip::eventgroup_t> group;
        group.insert(SAMPLE_EVENTGROUP_ID);
        vsomeip_app->offer_event(
                SAMPLE_SERVICE_ID,
                SAMPLE_INSTANCE_ID,
                SAMPLE_EVENT_ID,
                group,
                vsomeip::event_type_e::ET_FIELD);       
        vsomeip_app->offer_event(
                OTHER_SAMPLE_SERVICE_ID,
                OTHER_SAMPLE_INSTANCE_ID,
                SAMPLE_EVENT_ID,
                group,
                vsomeip::event_type_e::ET_FIELD);

        return true;
    }

    void start() {
        running = true;
        service_worker = std::thread(std::bind(&service_t::run, this));
        vsomeip_app->start();
    }

    void stop() {
        running = false;
        vsomeip_app->stop();
        if(service_worker.joinable()) {
            service_worker.join();
        }
    }

    service_t() : vsomeip_app(vsomeip::runtime::get()->create_application("service-sample")), counter(0), running(false) {

    }

    ~service_t() {
        stop();
    }

}; 


int main() {
    service_t service;

    if(service.init()) 
        service.start();

    return 0;
}
