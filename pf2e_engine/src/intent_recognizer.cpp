#include <intent_recognizer.h>
#include <iostream>
#include <vector>

constexpr int kMaxTokens = 100;

TUserIntentRecognizer::TUserIntentRecognizer(const std::filesystem::path& model_path)
{
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 0;

    model_ = llama_model_load_from_file(model_path.c_str(), model_params);
    if (model_ == nullptr) {
        throw std::runtime_error("Failed to load LLAMA model: " + model_path.string());
    }

    sampler_ = llama_sampler_init_greedy();
    vocab_ = llama_model_get_vocab(model_);

    llama_context_params ctx_params = llama_context_default_params();
    //ctx_params.seed = 1234;
    ctx_params.n_ctx = 2048;

    ctx_ = llama_init_from_model(model_, ctx_params);
    if (ctx_ == nullptr) {
        llama_model_free(model_);
        throw std::runtime_error("Failed to create LLAMA context");
    }
}

TUserIntentRecognizer::~TUserIntentRecognizer()
{
    if (ctx_ != nullptr) {
        llama_free(ctx_);
    }
    if (model_ != nullptr) {
        llama_model_free(model_);
    }
    llama_backend_free();
}

int TUserIntentRecognizer::Classify(const std::string& str_variants_list)
{
    std::vector<llama_token> tokens(2048);
    int n_tokens = llama_tokenize(vocab_, str_variants_list.c_str(), str_variants_list.size(),
        tokens.data(), tokens.size(), true, false);
    
    if (n_tokens < 0) {
        throw std::runtime_error("Tokenization error");
    }

    if (llama_decode(ctx_, llama_batch_get_one(tokens.data(), n_tokens)) != 0) {
        throw std::runtime_error("llama_decode(prompt) failed");
    }

    std::string result;
    int cur = n_tokens - 1;
    for (int i = 0; i < kMaxTokens; i++) {
        llama_token new_token = llama_sampler_sample(sampler_, ctx_, cur);

        const char * token_str = llama_vocab_get_text(vocab_, new_token);
        if (token_str != nullptr) {
            result += token_str;
            std::cout << "TOKEN: " << "\"" << token_str << "\"" << std::endl;
        }

        if (llama_decode(ctx_,llama_batch_get_one(&new_token, 1)) != 0) {
            break;
        }
        cur = 0;
    }

    std::cout << result << std::endl;
    return 0;
}
