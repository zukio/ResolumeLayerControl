#include "../source/plugins/LayerControlProbe/ProbeWorker.h"
#include <vector>
int main() {
    ProbeWorker worker;
    std::vector<std::thread> producers;
    for (int producer = 0; producer < 4; ++producer)
        producers.emplace_back([&worker, producer] {
            for (int i = 0; i < 800; ++i)
                worker.Record("concurrent", false, producer, i);
        });
    for (auto& producer : producers) producer.join();
    // Destructor must drain log events, even without an arbitrary sleep.
}
