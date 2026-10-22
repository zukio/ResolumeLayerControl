#include "../source/plugins/LayerControlProbe/ProbeWorker.h"
int main() {
    ProbeWorker worker;
    worker.Record("connect", true);
    char text[] = "host,version";
    worker.Record("snapshot", false, 1.25, 2.5, 3.75, text);
    text[0] = 'X'; // queued text must retain the value at callback time
    worker.Record("set_time", false, 42.125);
    worker.Record("set_time", false, 42.125); // repeated values are observations too
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
