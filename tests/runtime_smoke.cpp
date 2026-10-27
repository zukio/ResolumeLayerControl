#include "../source/control/Runtime.h"
#include <iostream>

class TestSource : public control::ControlSource {
public:
    explicit TestSource(bool second = false) : secondary(second) { key = second ? "other-runtime" : "test-runtime"; }
    bool secondary;
    control::Evaluation Evaluate(const control::Json& message) override {
        if (!secondary && message.value("pause_evaluation", false)) {
            std::cout << "evaluating" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
        }
        control::Evaluation e; e.revision = revision.load(); e.status = "test";
        if (secondary && message.value("fault", false)) throw std::runtime_error("test source failure");
        if (enabled.load() && message.value(secondary ? "active2" : "active", false))
            e.intents.push_back({key, secondary && !message.value("shared", false) ? "/parameter/by-id/456" : "/parameter/by-id/123",
                message["layers"][secondary && !message.value("shared", false) ? 1 : 0]["value"], message.value("desired", 2)});
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
        auto other = std::make_shared<TestSource>(true); runtime->Add(other);
        std::cout << "ready" << std::endl;
        std::string command;
        while (std::getline(std::cin, command)) {
            if (command == "quit") break;
            if (command == "capacity") {
                std::vector<std::shared_ptr<TestSource>> many;
                for (std::size_t i = 2; i < control::Runtime::SourceCapacity; ++i) {
                    auto extra = std::make_shared<TestSource>(); extra->enabled = false;
                    runtime->Add(extra); many.push_back(extra);
                }
                bool rejected = false;
                try { runtime->Add(std::make_shared<TestSource>()); } catch (const std::exception&) { rejected = true; }
                if (!rejected) throw std::runtime_error("capacity not bounded");
                many.clear();
                auto extra = std::make_shared<TestSource>(); runtime->Add(extra);
            }
            if (command == "enable") { source->enabled = true; ++source->revision; }
            if (command == "disable") { source->enabled = false; ++source->revision; }
            std::cout << "ok" << std::endl;
        }
        runtime.reset();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
