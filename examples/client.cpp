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

#define SUBSCRIBE_PERIOD 500 // ms


class client_t {
private:
    std::shared_ptr<vsomeip::application> vsomeip_app;
    std::thread client_worker;
    std::atomic<bool> running;

    void on_message(const std::shared_ptr<vsomeip::message> &message) {
        (void) message;
        std::cout << "client: <notify " << (int)message->get_payload()->get_data()[0] 
                << " from 0x" << std::setw(4) << std::setfill('0') << std::hex 
                << message->get_service() << "\n";
    }

    void on_availability(vsomeip::service_t _service, vsomeip::instance_t _instance, bool _is_available) {
        std::cout << "Service ["
                << std::setw(4) << std::setfill('0') << std::hex << _service << "." << _instance
                << "] is "
                << (_is_available ? "available." : "NOT available.")
                << std::endl;
    }

    void subscribe(vsomeip_v3::service_t service, 
                vsomeip_v3::instance_t instance, 
                vsomeip_v3::eventgroup_t eventgroup, 
                vsomeip_v3::event_t event) {
        (void)eventgroup;

        vsomeip_app->request_service(service, instance,0,0);

        std::set<vsomeip::eventgroup_t> group;
        group.insert(eventgroup);
        vsomeip_app->request_event(
                service,
                instance,
                event,
                group,
                vsomeip::event_type_e::ET_FIELD);

        vsomeip_app->subscribe(service, instance, eventgroup);
    }

    void unsubscribe(vsomeip_v3::service_t service, 
                vsomeip_v3::instance_t instance, 
                vsomeip_v3::eventgroup_t eventgroup, 
                vsomeip_v3::event_t event) {
        (void)eventgroup;
        vsomeip_app->unsubscribe(service, instance, eventgroup);

        vsomeip_app->release_event(
                service,
                instance,
                event);

        vsomeip_app->release_service(service, instance);
    }

    void run() {
        std::this_thread::sleep_for(std::chrono::seconds(1)); // lazy way to wait for app to be ready
        bool subscribed = true;
        while(running) {
            if(subscribed) {
                unsubscribe(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID, SAMPLE_EVENT_ID);
                unsubscribe(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID, SAMPLE_EVENT_ID);
            } else {
                subscribe(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID, SAMPLE_EVENT_ID);
                subscribe(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID, SAMPLE_EVENTGROUP_ID, SAMPLE_EVENT_ID);
            }
            subscribed = !subscribed;
            std::this_thread::sleep_for(std::chrono::milliseconds(SUBSCRIBE_PERIOD));
        }
    }

public: 
    bool init() {
        if(!vsomeip_app->init()) {
            std::cerr << "Couldn't initialize application" << std::endl;
            return false;
        }

        // vsomeip_app->register_message_handler(SAMPLE_SERVICE_ID,SAMPLE_INSTANCE_ID,vsomeip::ANY_METHOD,
        vsomeip_app->register_message_handler(vsomeip::ANY_SERVICE,vsomeip::ANY_INSTANCE,vsomeip::ANY_METHOD,
                                            std::bind(&client_t::on_message, this, std::placeholders::_1));
        // vsomeip_app->register_message_handler(OTHER_SAMPLE_SERVICE_ID,OTHER_SAMPLE_INSTANCE_ID,vsomeip::ANY_METHOD,
        vsomeip_app->register_message_handler(vsomeip::ANY_SERVICE,vsomeip::ANY_INSTANCE,vsomeip::ANY_METHOD,
                                            std::bind(&client_t::on_message, this, std::placeholders::_1));

        vsomeip_app->register_availability_handler(SAMPLE_SERVICE_ID, SAMPLE_INSTANCE_ID,
                std::bind(&client_t::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        vsomeip_app->register_availability_handler(OTHER_SAMPLE_SERVICE_ID, OTHER_SAMPLE_INSTANCE_ID,
                std::bind(&client_t::on_availability, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        
        return true;
    }

    void start() {
        running = true;
        client_worker = std::thread(std::bind(&client_t::run, this));
        vsomeip_app->start();
    }

    void stop() {
        running = false;
        vsomeip_app->stop();
        if(client_worker.joinable()) {
            client_worker.join();
        }
    }

    client_t() : vsomeip_app(vsomeip::runtime::get()->create_application("client-sample")) {

    }

    ~client_t() {
        stop();
    }
};

int main() {
    client_t client;

    if(client.init()) 
        client.start();

    return 0;
}
