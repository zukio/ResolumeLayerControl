#include "../source/control/Runtime.h"
#include <iostream>

class TestSource : public control::ControlSource {
public:
    TestSource() { key = "test-runtime"; }
    control::Evaluation Evaluate(const control::Json& message) override {
        control::Evaluation e; e.revision = revision.load(); e.status = "test";
        if (enabled.load() && message.value("active", false))
            e.intents.push_back({key, "/parameter/by-id/123", message["layers"][0]["value"], 2});
        return e;
    }
    std::uint64_t Revision() const override { return revision.load(); }
    std::atomic<bool> enabled{true};
    std::atomic<std::uint64_t> revision{0};
};
int main(int argc, char** argv) {
    try {
        auto source = std::make_shared<TestSource>();
        auto runtime = argc == 2 ? control::Runtime::ForTest(std::filesystem::u8path(argv[1])) : control::Runtime::Acquire(); runtime->Add(source);
        std::cout << "ready" << std::endl;
        std::string command;
        while (std::getline(std::cin, command)) {
            if (command == "quit") break;
            if (command == "disable") { source->enabled = false; ++source->revision; }
            std::cout << "ok" << std::endl;
        }
        runtime.reset();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
