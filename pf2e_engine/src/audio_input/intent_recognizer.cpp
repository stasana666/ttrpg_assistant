#include <intent_recognizer.h>
#include <iostream>
#include <vector>

constexpr int kMaxTokens = 5;

TUserIntentRecognizer::TUserIntentRecognizer(const std::filesystem::path& model_path)
{
    llama_backend_init();
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 100;

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

TUserIntentRecognizer::TState TUserIntentRecognizer::SaveState()
{
    TState result(llama_state_get_size(ctx_));
    llama_state_get_data(ctx_, result.data(), result.size());
    return result;
}

void TUserIntentRecognizer::LoadState(TState state)
{
    std::cout << "load state" << std::endl;
    llama_state_set_data(ctx_, state.data(), state.size());
}

int TUserIntentRecognizer::Classify(const std::string& str_prompt)
{
    TState state = SaveState();
    std::cerr << "Start Classify" << std::endl;
    std::vector<llama_token> tokens(4096);
    std::cerr << "str_prompt = \"" << str_prompt << "\"" << std::endl;
    int n_tokens = llama_tokenize(vocab_, str_prompt.c_str(), str_prompt.size(),
        tokens.data(), tokens.size(), true, false);
    std::cerr << "end tokenize" << std::endl;
    if (n_tokens < 0) {
        throw std::runtime_error("Tokenization error");
    }

    tokens.resize(n_tokens);

    auto batch = llama_batch_get_one(tokens.data(), n_tokens);
    std::string result;
    for (int i = 0; i < kMaxTokens; i++) {
        if (llama_decode(ctx_, batch) != 0) {
            throw std::runtime_error("llama_decode(prompt) failed");
        }

        llama_token new_token_id = llama_sampler_sample(sampler_, ctx_, -1);

        if (llama_vocab_is_eog(vocab_, new_token_id)) {
            break;
        }

        const char* new_token = llama_vocab_get_text(vocab_, new_token_id);
        if (new_token != nullptr) {
            if (std::string(new_token) == "<0x0A>") {
                break;
            }
            result += new_token;
        }

        batch = llama_batch_get_one(&new_token_id, 1);
    }

    std::string trimmed;
    for (char c : result) {
        if ((c >= '0' && c <= '9') || c == '?') {
            trimmed += c;
        }
    }

    std::cerr << "End Classify with \"" << trimmed << "\"" << std::endl;

    LoadState(state);

    if (trimmed == "?") {
        return -1;
    }

    bool is_int = !trimmed.empty() && std::all_of(trimmed.begin(), trimmed.end(), ::isdigit);

    if (is_int) {
        return std::stoi(trimmed);
    }

    return -1;
}
