#include "prompt.h"

#include <cassert>
#include <sstream>

std::string TPrompt::ToString() const
{
    std::stringstream ss;

    ss << "Ты — система, задача которой — выбрать одно действие из предложенного списка вариантов, исходя из того, что сказал игрок.\n"
       << "Если заявка игрока содержит достаточно информации для однозначного выбора, выбери один вариант, который максимально соответствует её содержанию.\n"
       << "Если заявка игрока не позволяет определить выбор однозначно, ответь специальным значением \"unknown\"\n"
       << "Используй только предоставленные варианты.\n"
       << "НЕ ДОДУМЫВАЙ за пользователя, если информации мало ответь \"unknown\"\n"
       << "НЕ ПРИДУМЫВАЙ новые варианты.\n"
       << "Не оценивай последствия, не добавляй комментариев, не интерпретируй.\n"
       << "Отвечай только номером выбранного варианта или \"unknown\"\n";

    ss << "\n";

    if (!variants_.empty()) {
        ss << "<VARIANTS_LIST_BEGIN>\n";
        for (const auto& [index, name] : variants_) {
            ss << index << ": " << name << "\n";
        }
        ss << "<VARIANTS_LIST_END>\n";
    }

    ss << "\n";

    if (!user_speech_.empty()) {
        ss << "<USER_INPUT_START>\n";
        ss << user_speech_ << "\n";
        ss << "<USER_INPUT_END>\n";
    }

    ss << "\n";

    ss << "Если заявка явно указывает на какой-то вариант - написи ТОЛЬКО его номер\n";
    ss << "Если никакой вариант не упоминается - НЕ НУЖНО ПИСАТЬ НИКАКОЙ НОМЕР, НАПИШИ \"unknown\"\n";

    ss << "CORRECT_ANSWER: ";

    return ss.str();
}

void TPrompt::AddVariant(int index, std::string variant)
{
    auto [_, inserted] = variants_.emplace(index, variant);
    assert(inserted);
}

void TPrompt::AddUserSpeech(std::string speech)
{
    user_speech_ = speech;
}

void TPrompt::AddGameContext(std::string game_context)
{
    game_context_ = game_context;
}
