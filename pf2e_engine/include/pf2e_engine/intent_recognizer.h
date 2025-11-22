#pragma once

#include <llama.h>
#include <filesystem>

class TUserIntentRecognizer {
public:
    explicit TUserIntentRecognizer(const std::filesystem::path& model_path);
    ~TUserIntentRecognizer();

    int Classify(const std::string& str_variants_list);

private:
    llama_model* model_ = nullptr;
    llama_context* ctx_ = nullptr;
    llama_sampler * sampler_ = nullptr;
    const llama_vocab* vocab_ = nullptr;
};
