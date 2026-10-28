#include "../source/control/APISenderSource.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        using Source = control::APISenderSource;
        auto source = std::make_shared<Source>();
        source->SetText(Source::Identity, "api-test");
        source->SetText(Source::ActiveCommand, R"([{"action":"set","parameter":"/composition/master","value":0.5},{"action":"trigger","parameter":"/composition/columns/2/connect"}])");
        source->SetText(Source::InactiveCommand, R"({"action":"set","parameter":"/composition/master","value":1})");
        // Validation must be atomic: reject invalid edits and retain the old text.
        for (const std::string text : {"[{}]", "[1]", "null", "{", "{\"action\":0}"}) {
            bool rejected = false;
            try { source->SetText(Source::ActiveCommand, text); } catch (...) { rejected = true; }
            if (!rejected) throw std::runtime_error("invalid command accepted");
        }
        if (control::ParseAPICommands(" \n\t").size() || control::ParseAPICommands("[]").size())
            throw std::runtime_error("empty commands");
        bool limited = false;
        try { control::ParseAPICommands(std::string(65537, ' ')); } catch (...) { limited = true; }
        if (!limited) throw std::runtime_error("text limit");
        limited = false;
        try { control::ParseAPICommands(control::Json(std::vector<control::Json>(257, {{"action", "get"}})).dump()); } catch (...) { limited = true; }
        if (!limited) throw std::runtime_error("batch limit");
        auto runtime = control::Runtime::ForTest(argv[1]); runtime->Add(source);
        std::cout << "ready" << std::endl;
        std::string line;
        while (std::getline(std::cin, line) && line != "quit") {
            if (line == "disable") source->SetEnabled(0);
            else if (line == "enable") source->SetEnabled(1);
            else if (line == "empty") source->SetText(Source::ActiveCommand, "");
            else if (line == "edit") source->SetText(Source::ActiveCommand, R"({"action":"get","parameter":"/composition"})");
            std::cout << "ok" << std::endl;
        }
        runtime.reset();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
