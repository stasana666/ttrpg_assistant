#pragma once

#include <map>
#include <string>

class TPrompt {
public:
    std::string ToString() const;

    void AddVariant(int index, std::string variant);
    void AddUserSpeech(std::string speech);
    void AddGameContext(std::string game_context);

private:
    std::string user_speech_;
    std::string game_context_;
    std::map<int, std::string> variants_;
};
