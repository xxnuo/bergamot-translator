#include "mtrancore.h"
#include "translator/definitions.h"
#include "translator/service.h"
#include "translator/translation_model.h"
#include "translator/parser.h"
#include <future>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <chrono>
#include <filesystem>

// 内部辅助函数的前向声明
static bool isSupportedInternal(const TranslatorWrapper *translator,
                                const std::string &from, const std::string &to);
static std::string translateInternal(TranslatorWrapper *translator,
                                     const std::string &from,
                                     const std::string &to,
                                     const std::string &input);

/**
 * @brief 内部包装器结构，持有 Bergamot 翻译器状态
 * 
 * 该结构封装了 Bergamot 翻译模型和异步服务，提供 C 兼容接口，
 * 同时保持 C++ 实现。支持多种语言对的翻译模型管理。
 */
struct TranslatorWrapper {
  /// 语言对到对应翻译模型的映射表
  /// 键格式: "fromto" (例如: "enen", "fren", "zhen" 等)
  std::map<std::string, marian::Ptr<marian::bergamot::TranslationModel>> models;
  
  /// 处理翻译请求的异步服务
  marian::bergamot::AsyncService service;
  
  /// 翻译使用的工作线程数量
  size_t numWorkers;

  /**
   * @brief TranslatorWrapper 构造函数
   * @param workers 用于翻译的工作线程数量
   */
  explicit TranslatorWrapper(size_t workers);

  /// 默认析构函数
  ~TranslatorWrapper() = default;
};

// 构造函数的实现
TranslatorWrapper::TranslatorWrapper(size_t workers)
    : service(marian::bergamot::AsyncService::Config{
        .numWorkers = workers,  // 设置工作线程数量
        .cacheSize = 0,         // 不使用缓存
        .logger = {}            // 使用默认日志配置
      }), 
      numWorkers(workers) {}

/**
 * @brief 创建新的翻译器实例
 * 
 * 该函数创建一个新的翻译器包装器，初始化异步服务和必要的资源。
 * 支持多线程并发翻译，线程数量可配置。
 * 
 * @param numWorkers 用于翻译的工作线程数量，建议根据 CPU 核心数设置
 * @return 创建成功返回翻译器包装器指针，失败返回 nullptr
 */
extern "C" TranslatorWrapper *create(size_t numWorkers) {
  try {
    // 确保至少有一个工作线程
    if (numWorkers == 0) {
      numWorkers = 1;
    }
    
    TranslatorWrapper *ptr = new TranslatorWrapper(numWorkers);
    return ptr;
  } catch (const std::exception &e) {
    std::cerr << "Error creating translator: " << e.what() << std::endl;
    return nullptr;
  } catch (...) {
    std::cerr << "Unknown error creating translator" << std::endl;
    return nullptr;
  }
}

/**
 * @brief 销毁翻译器实例并释放内存
 * 
 * 该函数安全地销毁翻译器实例，清理所有相关资源，
 * 包括翻译模型、异步服务和工作线程。
 * 
 * @param translator 要销毁的翻译器包装器指针
 */
extern "C" void destroy(TranslatorWrapper *translator) {
  if (translator) {
    delete translator;
  }
}

/**
 * @brief 加载指定语言对的翻译模型配置
 * 
 * 该函数为特定语言对加载翻译模型。配置字符串应包含模型路径、
 * 词汇表路径等必要信息。支持动态加载多个语言对的模型。
 * 
 * @param translator 翻译器包装器指针
 * @param languagePair 语言对标识符 (例如: "enen", "fren", "zhen")
 * @param config 翻译模型的配置字符串 (YAML 格式)
 */
extern "C" void loadConfig(TranslatorWrapper *translator,
                          const char *languagePair,
                          const char *config) {
  try {
    if (!translator || !languagePair || !config) {
      std::cerr << "Error: Invalid parameters for model loading" << std::endl;
      return;
    }

    std::string langPair = languagePair;
    
    // 解析配置字符串为 marian 选项
    auto options = marian::bergamot::parseOptionsFromString(config);
    
    // 创建内存束以传递给翻译模型
    marian::bergamot::MemoryBundle memoryBundle;
    
    // 创建翻译模型实例
    translator->models[langPair] = marian::New<marian::bergamot::TranslationModel>(
        options, std::move(memoryBundle), translator->numWorkers);
        
    std::cout << "Successfully loaded model for language pair: " << langPair << std::endl;
  } catch (const std::exception &e) {
    std::cerr << "Error loading model: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown error loading model" << std::endl;
  }
}

/**
 * @brief 从目录批量加载翻译模型
 * 
 * 该函数扫描指定目录，自动检测并加载所有可用的翻译模型。
 * 期望的目录结构：每个子目录包含一个语言对的完整模型文件。
 * 
 * @param translator 翻译器包装器指针
 * @param modelsDir 包含翻译模型的目录路径
 */
extern "C" void loadModelsFromDirectory(TranslatorWrapper *translator,
                                       const char *modelsDir) {
  try {
    if (!translator || !modelsDir) {
      std::cerr << "Error: Invalid parameters for models directory loading" << std::endl;
      return;
    }

    if (!std::filesystem::exists(modelsDir)) {
      std::cerr << "Error: Models directory does not exist: " << modelsDir << std::endl;
      return;
    }

    // 遍历模型目录
    for (const auto &entry : std::filesystem::directory_iterator(modelsDir)) {
      if (!entry.is_directory()) continue;
      
      auto basePath = entry.path();
      auto langPair = entry.path().filename().string();
      
      std::string srcVocabPath = "";
      std::string trgVocabPath = "";
      std::string modelPath = "";
      std::string shortlistPath = "";

      std::cout << "Scanning for models in " << basePath << std::endl;

      // 扫描模型文件
      for (const auto &fileEntry : std::filesystem::directory_iterator(basePath)) {
        auto fileName = fileEntry.path().filename().string();
        auto filePath = fileEntry.path().string();
        
        if (fileName.size() >= 4 && fileName.substr(fileName.size() - 4) == ".spm") {
          // SentencePiece 词汇表文件
          if (fileName.size() >= 8 && fileName.substr(0, 8) == "srcvocab") {
            srcVocabPath = filePath;
          } else if (fileName.size() >= 8 && fileName.substr(0, 8) == "trgvocab") {
            trgVocabPath = filePath;
          } else {
            // 通用词汇表，源语言和目标语言共用
            srcVocabPath = filePath;
            trgVocabPath = filePath;
          }
        }
        else if ((fileName.size() >= 19 && fileName.substr(fileName.size() - 19) == ".intgemm.alphas.bin") || 
                 (fileName.size() >= 12 && fileName.substr(fileName.size() - 12) == ".intgemm8.bin")) {
          // 量化模型文件
          modelPath = filePath;
        }
        else if (fileName.size() >= 7 && fileName.substr(fileName.size() - 7) == ".s2t.bin") {
          // 快速搜索列表文件
          shortlistPath = filePath;
        }
      }

      // 验证必要文件存在
      if (modelPath.empty() || srcVocabPath.empty() || trgVocabPath.empty()) {
        std::cerr << "Warning: Incomplete model files for " << langPair << std::endl;
        continue;
      }

      // 构建配置字符串
      std::string config = 
        "beam-size: 1\n"
        "normalize: 1.0\n"
        "word-penalty: 0\n"
        "max-length-break: 128\n"
        "mini-batch-words: 1024\n"
        "workspace: 128\n"
        "max-length-factor: 2.0\n"
        "skip-cost: true\n"
        "quiet: true\n"
        "quiet-translation: true\n"
        "gemm-precision: int8shiftAll\n"
        "models: [" + modelPath + "]\n"
        "vocabs: [" + srcVocabPath + ", " + trgVocabPath + "]\n";
      
      if (!shortlistPath.empty()) {
        config += "shortlist: [" + shortlistPath + ", false]\n";
      }

      // 加载模型
      loadConfig(translator, langPair.c_str(), config.c_str());
    }

    if (translator->models.empty()) {
      std::cerr << "Error: No valid models found in directory: " << modelsDir << std::endl;
    } else {
      std::cout << "Successfully loaded " << translator->models.size() 
                << " translation models" << std::endl;
    }
  } catch (const std::exception &e) {
    std::cerr << "Error loading models from directory: " << e.what() << std::endl;
  } catch (...) {
    std::cerr << "Unknown error loading models from directory" << std::endl;
  }
}

/**
 * @brief 检查是否支持给定语言对的翻译
 * 
 * 该函数检查是否可以进行指定语言对的翻译。支持直接翻译和
 * 通过英语中转的间接翻译（透视翻译）。
 * 
 * @param translator 翻译器包装器指针
 * @param from 源语言代码 (例如: "zh", "en", "fr")
 * @param to 目标语言代码 (例如: "en", "zh", "fr")
 * @return 支持翻译返回 true，否则返回 false
 */
extern "C" bool isSupported(TranslatorWrapper *translator,
                           const char *from, const char *to) {
  if (!translator || !from || !to) {
    return false;
  }

  try {
    // 检查直接翻译支持或通过英语中转的支持
    return isSupportedInternal(translator, from, to) ||
           (isSupportedInternal(translator, from, "en") &&
            isSupportedInternal(translator, "en", to));
  } catch (const std::exception &e) {
    std::cerr << "Error checking supported languages: " << e.what()
              << std::endl;
    return false;
  } catch (...) {
    std::cerr << "Unknown error checking supported languages" << std::endl;
    return false;
  }
}

/**
 * @brief 获取支持的语言对列表
 * 
 * 该函数返回当前翻译器支持的所有语言对的数量。
 * 可用于遍历所有可用的翻译模型。
 * 
 * @param translator 翻译器包装器指针
 * @return 支持的语言对数量
 */
extern "C" size_t getSupportedLanguagePairsCount(TranslatorWrapper *translator) {
  if (!translator) {
    return 0;
  }
  return translator->models.size();
}

/**
 * @brief 获取指定索引的语言对标识符
 * 
 * 该函数返回指定索引位置的语言对标识符。
 * 与 getSupportedLanguagePairsCount 配合使用可遍历所有语言对。
 * 
 * @param translator 翻译器包装器指针
 * @param index 语言对索引 (0 到 count-1)
 * @return 语言对标识符，失败返回 nullptr
 */
extern "C" const char *getSupportedLanguagePair(TranslatorWrapper *translator, 
                                                size_t index) {
  if (!translator || index >= translator->models.size()) {
    return nullptr;
  }
  
  auto it = translator->models.begin();
  std::advance(it, index);
  
  // 注意：这里返回的是内部字符串的指针，调用者不应修改或释放
  return it->first.c_str();
}

/**
 * @brief 翻译文本
 * 
 * 该函数将输入文本从源语言翻译为目标语言。支持直接翻译和
 * 通过英语中转的透视翻译。返回的字符串需要使用 freeTranslation 释放。
 * 
 * @param translator 翻译器包装器指针
 * @param from 源语言代码
 * @param to 目标语言代码  
 * @param input 要翻译的文本
 * @return 翻译结果字符串 (需要用 freeTranslation 释放)，失败返回 nullptr
 */
extern "C" const char *translate(TranslatorWrapper *translator,
                                const char *from, const char *to,
                                const char *input) {
  if (!translator || !from || !to || !input) {
    std::cerr << "Error: Invalid parameters for translation" << std::endl;
    return nullptr;
  }

  char *c_result = nullptr;
  try {
    std::string result;
    
    // 优先尝试直接翻译
    if (isSupportedInternal(translator, from, to)) {
      result = translateInternal(translator, from, to, input);
    } 
    // 回退到通过英语的透视翻译
    else if (isSupportedInternal(translator, from, "en") &&
             isSupportedInternal(translator, "en", to)) {
      // 第一步：源语言 -> 英语
      std::string intermediateRes =
          translateInternal(translator, from, "en", input);
      if (intermediateRes.empty()) {
        std::cerr << "Error: Intermediate translation produced empty result"
                  << std::endl;
        return nullptr;
      }
      // 第二步：英语 -> 目标语言
      result = translateInternal(translator, "en", to, intermediateRes);
    } else {
      std::cerr << "Error: Unsupported language pair for translation: " << from
                << " -> " << to << std::endl;
      return nullptr;
    }

    if (result.empty()) {
      std::cerr << "Error: Translation produced empty result" << std::endl;
      return nullptr;
    }

    // 为结果分配 C 字符串内存
    c_result = new char[result.size() + 1];
    std::copy(result.begin(), result.end(), c_result);
    c_result[result.size()] = '\0';

    return c_result;
  } catch (const std::exception &e) {
    delete[] c_result;
    std::cerr << "Error during translation: " << e.what() << std::endl;
    return nullptr;
  } catch (...) {
    delete[] c_result;
    std::cerr << "Unknown error during translation" << std::endl;
    return nullptr;
  }
}

/**
 * @brief 释放翻译结果的内存
 * 
 * 该函数释放由 translate 函数分配的内存。
 * 必须为每个成功的翻译调用此函数以避免内存泄漏。
 * 
 * @param translation 要释放的翻译字符串指针
 */
extern "C" void freeTranslation(const char *translation) {
  delete[] translation;
}

/**
 * @brief 内部函数：执行特定语言对的实际翻译
 * 
 * 该函数执行底层的翻译操作，使用异步服务和同步等待机制。
 * 包含超时处理以防止长时间阻塞。
 * 
 * @param translator 翻译器包装器指针
 * @param from 源语言代码
 * @param to 目标语言代码
 * @param input 要翻译的文本
 * @return 翻译结果字符串，失败返回空字符串
 */
static std::string translateInternal(TranslatorWrapper *translator,
                                     const std::string &from,
                                     const std::string &to,
                                     const std::string &input) {
  std::string langPair = from + to;

  auto modelIt = translator->models.find(langPair);
  if (modelIt == translator->models.end()) {
    std::cerr << "Error: Model not found for language pair: " << from << " -> "
              << to << std::endl;
    return "";
  }

  auto model = modelIt->second;
  
  // 设置响应选项
  marian::bergamot::ResponseOptions responseOptions;
  
  // 创建 promise/future 对来等待异步翻译完成
  std::promise<marian::bergamot::Response> responsePromise;
  std::future<marian::bergamot::Response> responseFuture = responsePromise.get_future();

  // 翻译完成时的回调函数
  auto callback = [&responsePromise](marian::bergamot::Response &&response) {
    responsePromise.set_value(std::move(response));
  };

  try {
    // 提交翻译请求到异步服务
    translator->service.translate(model, std::string(input),
                                  std::move(callback), responseOptions);
    
    if (!responseFuture.valid()) {
      std::cerr << "Error: responseFuture is invalid!" << std::endl;
      return "";
    }
    
    // 等待翻译完成 (30秒超时)
    auto status = responseFuture.wait_for(std::chrono::seconds(30));
    if (status != std::future_status::ready) {
      throw std::runtime_error("Translation timeout");
    }
    
    marian::bergamot::Response response = responseFuture.get();
    return response.target.text;
  } catch (const std::exception &e) {
    std::cerr << "Error in translation service: " << e.what() << std::endl;
    return "";
  } catch (...) {
    std::cerr << "Unknown error in translation service" << std::endl;
    return "";
  }
}

/**
 * @brief 内部函数：检查特定语言对是否受支持
 * 
 * 该函数检查是否存在指定语言对的已加载翻译模型。
 * 
 * @param translator 翻译器包装器指针
 * @param from 源语言代码
 * @param to 目标语言代码
 * @return 支持返回 true，否则返回 false
 */
static bool isSupportedInternal(const TranslatorWrapper *translator,
                                const std::string &from,
                                const std::string &to) {
  if (!translator || from.empty() || to.empty()) {
    return false;
  }

  std::string langPair = from + to;
  return translator->models.find(langPair) != translator->models.end();
}
