#include "../source/control/APISenderSource.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    try {
        using Source = control::APISenderSource;
        auto source = std::make_shared<Source>();
        source->SetText(Source::Identity, "api-test");
        const std::string bodyInput = R"([{"action":"post","body":["/composition/layers/1/clips/1","/composition/layers/1/clips/3"]},{"action":"post","body":{"name":"quote\" and newline\n","nested":[1,true]}},{"action":"post","body":"[1, 2]"},{"action":"post"},{"action":"post","body":null},{"action":"post","body":3},{"action":"set","value":"One Below","body":{"unchanged":true}},{"action":"remove","body":[]}])";
        source->SetText(Source::ActiveCommand, bodyInput);
        source->SetText(Source::InactiveCommand, bodyInput);
        if (source->GetText(Source::ActiveCommand) != bodyInput || source->GetText(Source::InactiveCommand) != bodyInput)
            throw std::runtime_error("body normalization changed saved text");
        const auto original = control::Json::parse(bodyInput);
        const auto normalized = control::ParseAPICommands(bodyInput);
        for (std::size_t i = 0; i < normalized.size(); ++i) {
            if (i < 2) {
                if (!normalized[i]["body"].is_string() ||
                    control::Json::parse(normalized[i]["body"].get<std::string>()) != original[i]["body"])
                    throw std::runtime_error("structured post body not serialized losslessly");
            } else if (normalized[i] != original[i]) throw std::runtime_error("unrelated command changed");
        }
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
            else if (line == "bodies") {
                source->SetText(Source::ActiveCommand, bodyInput);
                source->SetText(Source::InactiveCommand, bodyInput);
            }
            std::cout << "ok" << std::endl;
        }
        runtime.reset();
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
