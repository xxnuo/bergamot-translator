#pragma once

#ifdef _WIN32
#define MTRANCORE_API __declspec(dllexport)
#else
#define MTRANCORE_API __attribute__((visibility("default")))
#endif

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 翻译器包装器类型
 * 
 * 不透明指针类型，用于在 C 接口中表示翻译器实例。
 * 实际的实现细节在 C++ 端处理。
 */
typedef struct TranslatorWrapper TranslatorWrapper;

/**
 * @brief 创建翻译器实例
 * 
 * 创建一个新的翻译器实例，支持多线程并发翻译。
 * 
 * @param numWorkers 工作线程数量，建议根据 CPU 核心数设置
 * @return 成功返回翻译器指针，失败返回 nullptr
 */
MTRANCORE_API TranslatorWrapper *create(size_t numWorkers);

/**
 * @brief 销毁翻译器实例
 * 
 * 释放翻译器实例及其相关资源，包括翻译模型和工作线程。
 * 
 * @param translator 要销毁的翻译器实例
 */
MTRANCORE_API void destroy(TranslatorWrapper *translator);

/**
 * @brief 加载翻译模型配置
 * 
 * 为指定语言对加载翻译模型。配置字符串应为 YAML 格式，
 * 包含模型文件路径、词汇表路径等信息。
 * 
 * @param translator 翻译器实例
 * @param languagePair 语言对标识符 (例如: "enen", "fren", "zhen")
 * @param config YAML 格式的配置字符串
 */
MTRANCORE_API void loadConfig(TranslatorWrapper *translator, 
                             const char *languagePair, 
                             const char *config);

/**
 * @brief 从目录批量加载翻译模型
 * 
 * 扫描指定目录，自动检测并加载所有可用的翻译模型。
 * 期望每个子目录包含一个语言对的完整模型文件。
 * 
 * @param translator 翻译器实例
 * @param modelsDir 包含翻译模型的目录路径
 */
MTRANCORE_API void loadModelsFromDirectory(TranslatorWrapper *translator, 
                                          const char *modelsDir);

/**
 * @brief 检查语言对支持情况
 * 
 * 检查是否支持指定语言对的翻译，包括直接翻译和透视翻译。
 * 
 * @param translator 翻译器实例
 * @param from 源语言代码 (例如: "zh", "en", "fr")
 * @param to 目标语言代码 (例如: "en", "zh", "fr")
 * @return 支持返回 true，否则返回 false
 */
MTRANCORE_API bool isSupported(TranslatorWrapper *translator, 
                              const char *from, 
                              const char *to);

/**
 * @brief 获取支持的语言对数量
 * 
 * 返回当前加载的翻译模型支持的语言对总数。
 * 
 * @param translator 翻译器实例
 * @return 支持的语言对数量
 */
MTRANCORE_API size_t getSupportedLanguagePairsCount(TranslatorWrapper *translator);

/**
 * @brief 获取指定索引的语言对标识符
 * 
 * 与 getSupportedLanguagePairsCount 配合使用可遍历所有支持的语言对。
 * 
 * @param translator 翻译器实例
 * @param index 语言对索引 (0 到 count-1)
 * @return 语言对标识符字符串，失败返回 nullptr
 */
MTRANCORE_API const char *getSupportedLanguagePair(TranslatorWrapper *translator, 
                                                   size_t index);

/**
 * @brief 翻译文本
 * 
 * 将输入文本从源语言翻译为目标语言。支持直接翻译和透视翻译。
 * 返回的字符串必须使用 freeTranslation 函数释放。
 * 
 * @param translator 翻译器实例
 * @param from 源语言代码
 * @param to 目标语言代码
 * @param input 要翻译的文本
 * @return 翻译结果字符串 (需要释放)，失败返回 nullptr
 */
MTRANCORE_API const char *translate(TranslatorWrapper *translator, 
                                   const char *from, 
                                   const char *to, 
                                   const char *input);

/**
 * @brief 释放翻译结果内存
 * 
 * 释放由 translate 函数分配的内存。必须为每个成功的翻译
 * 调用此函数以避免内存泄漏。
 * 
 * @param translation 要释放的翻译结果字符串
 */
MTRANCORE_API void freeTranslation(const char *translation);

#ifdef __cplusplus
}
#endif
