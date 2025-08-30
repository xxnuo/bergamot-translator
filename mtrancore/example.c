/**
 * @file example.c
 * @brief mtrancore 库使用示例
 * 
 * 本示例程序演示了如何使用 mtrancore 库进行文本翻译。
 * 包括创建翻译器、加载模型、执行翻译和资源清理等操作。
 */

#include "mtrancore.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief 打印库的使用方法
 */
void printUsage(const char *programName) {
    printf("用法:\n");
    printf("  %s <models_directory>\n", programName);
    printf("  %s <from_lang> <to_lang> <text> <config_string>\n", programName);
    printf("\n");
    printf("示例:\n");
    printf("  # 从目录加载模型并交互式翻译\n");
    printf("  %s ./models\n", programName);
    printf("\n");
    printf("  # 直接配置并翻译\n");
    printf("  %s en zh \"Hello world\" \"models: [model.bin]\\nvocabs: [vocab.spm, vocab.spm]\"\n", programName);
    printf("\n");
}

/**
 * @brief 交互式翻译模式
 * 
 * 允许用户输入不同的语言对和文本进行翻译测试
 */
void interactiveMode(TranslatorWrapper *translator) {
    char fromLang[16], toLang[16];
    char input[1024];
    
    printf("\n=== 交互式翻译模式 ===\n");
    printf("输入格式: <源语言> <目标语言> <要翻译的文本>\n");
    printf("例如: en zh Hello world\n");
    printf("输入 'quit' 退出\n\n");
    
    while (1) {
        printf("翻译> ");
        fflush(stdout);
        
        if (!fgets(input, sizeof(input), stdin)) {
            break;
        }
        
        // 移除换行符
        input[strcspn(input, "\n")] = 0;
        
        // 检查退出命令
        if (strcmp(input, "quit") == 0 || strcmp(input, "exit") == 0) {
            break;
        }
        
        // 解析输入
        char *token = strtok(input, " ");
        if (!token) continue;
        strcpy(fromLang, token);
        
        token = strtok(NULL, " ");
        if (!token) {
            printf("错误: 请提供目标语言\n");
            continue;
        }
        strcpy(toLang, token);
        
        token = strtok(NULL, "");
        if (!token) {
            printf("错误: 请提供要翻译的文本\n");
            continue;
        }
        
        // 检查语言对支持
        if (!isSupported(translator, fromLang, toLang)) {
            printf("错误: 不支持语言对 %s -> %s\n", fromLang, toLang);
            continue;
        }
        
        // 执行翻译
        const char *result = translate(translator, fromLang, toLang, token);
        if (result) {
            printf("翻译结果: %s\n", result);
            freeTranslation(result);
        } else {
            printf("翻译失败\n");
        }
        printf("\n");
    }
}

/**
 * @brief 显示加载的模型信息
 */
void showModelInfo(TranslatorWrapper *translator) {
    size_t count = getSupportedLanguagePairsCount(translator);
    printf("\n=== 已加载的翻译模型 ===\n");
    printf("总共加载 %zu 个语言对:\n", count);
    
    for (size_t i = 0; i < count; i++) {
        const char *langPair = getSupportedLanguagePair(translator, i);
        if (langPair && strlen(langPair) >= 4) {
            // 假设语言对格式为 "fromto"，每个语言代码2个字符
            printf("  %zu. %c%c -> %c%c (%s)\n", 
                   i + 1,
                   langPair[0], langPair[1],
                   langPair[2], langPair[3],
                   langPair);
        }
    }
    printf("\n");
}

/**
 * @brief 从目录加载模型的模式
 */
int directoryMode(const char *modelsDir) {
    printf("正在创建翻译器实例 (4个工作线程)...\n");
    TranslatorWrapper *translator = create(4);
    if (!translator) {
        fprintf(stderr, "错误: 无法创建翻译器实例\n");
        return 1;
    }
    
    printf("正在从目录加载翻译模型: %s\n", modelsDir);
    loadModelsFromDirectory(translator, modelsDir);
    
    // 检查是否加载了任何模型
    size_t modelCount = getSupportedLanguagePairsCount(translator);
    if (modelCount == 0) {
        fprintf(stderr, "错误: 未找到任何有效的翻译模型\n");
        destroy(translator);
        return 1;
    }
    
    // 显示模型信息
    showModelInfo(translator);
    
    // 进入交互模式
    interactiveMode(translator);
    
    printf("正在清理资源...\n");
    destroy(translator);
    printf("完成\n");
    
    return 0;
}

/**
 * @brief 直接配置模式
 */
int directConfigMode(const char *fromLang, const char *toLang, 
                    const char *text, const char *config) {
    printf("正在创建翻译器实例 (2个工作线程)...\n");
    TranslatorWrapper *translator = create(2);
    if (!translator) {
        fprintf(stderr, "错误: 无法创建翻译器实例\n");
        return 1;
    }
    
    // 构造语言对标识符
    char langPair[32];
    snprintf(langPair, sizeof(langPair), "%s%s", fromLang, toLang);
    
    printf("正在加载翻译模型配置...\n");
    loadConfig(translator, langPair, config);
    
    // 检查是否支持此语言对
    if (!isSupported(translator, fromLang, toLang)) {
        fprintf(stderr, "错误: 不支持语言对 %s -> %s\n", fromLang, toLang);
        destroy(translator);
        return 1;
    }
    
    printf("正在翻译: \"%s\"\n", text);
    printf("语言对: %s -> %s\n", fromLang, toLang);
    
    const char *result = translate(translator, fromLang, toLang, text);
    if (result) {
        printf("翻译结果: %s\n", result);
        freeTranslation(result);
    } else {
        fprintf(stderr, "翻译失败\n");
        destroy(translator);
        return 1;
    }
    
    printf("正在清理资源...\n");
    destroy(translator);
    printf("完成\n");
    
    return 0;
}

/**
 * @brief 主函数
 */
int main(int argc, char *argv[]) {
    printf("=== mtrancore 库使用示例 ===\n\n");
    
    if (argc != 2 && argc != 5) {
        printUsage(argv[0]);
        return 1;
    }
    
    if (argc == 2) {
        // 目录加载模式
        return directoryMode(argv[1]);
    } else {
        // 直接配置模式
        return directConfigMode(argv[1], argv[2], argv[3], argv[4]);
    }
}

/**
 * @brief 编译和运行说明
 * 
 * 编译命令:
 *   gcc -o example example.c -lmtrancore -L. -I.
 * 
 * 运行示例:
 *   # 方式1: 从目录加载模型
 *   ./example ./models
 * 
 *   # 方式2: 直接配置
 *   ./example en zh "Hello world" "models: [model.bin]\nvocabs: [vocab.spm, vocab.spm]"
 * 
 * 目录结构示例:
 *   models/
 *   ├── enen/
 *   │   ├── model.intgemm8.bin
 *   │   ├── vocab.spm
 *   │   └── shortlist.s2t.bin
 *   ├── fren/
 *   │   ├── model.intgemm8.bin
 *   │   ├── srcvocab.spm
 *   │   ├── trgvocab.spm
 *   │   └── shortlist.s2t.bin
 *   └── zhen/
 *       ├── model.intgemm8.bin
 *       └── vocab.spm
 */
