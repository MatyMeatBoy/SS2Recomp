#include "online/storage_worker.h"
#include <future>
#include <iostream>
#include <vector>

int main() {
    std::promise<void> release;
    auto gate=release.get_future().share();
    std::vector<int> written;
    {
        ss2::online::StorageWorker worker;
        worker.submit(52,[&]{gate.wait(); written.push_back(1);});
        // A blocked flush must not block admission on the simulation thread.
        worker.submit(52,[&]{written.push_back(2);});
        bool bounded=false;
        try { worker.submit(16*1024*1024+1,[]{}); }
        catch (const std::runtime_error&) { bounded=true; }
        if (!bounded) return 1;
        release.set_value(); worker.flush();
        if (written!=std::vector<int>{1,2}) return 2;
    }
    {
        ss2::online::StorageWorker worker;
        worker.submit(1,[]{throw std::runtime_error("injected persistence failure");});
        bool surfaced=false;
        try { worker.flush(); }
        catch (const std::runtime_error&) { surfaced=true; }
        if (!surfaced) return 3;
        surfaced=false;
        try { worker.submit(1,[]{}); }
        catch (const std::runtime_error&) { surfaced=true; }
        if (!surfaced) return 4;
    }
    std::cout << "PASS ordered persistence, nonblocking admission, bounded backlog, failure propagation\n";
}
