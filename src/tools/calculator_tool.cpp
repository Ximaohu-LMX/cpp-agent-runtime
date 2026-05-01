#include "tools/calculator_tool.hpp"

#include <cctype>
#include <cmath>
#include <stdexcept>
#include <string>

namespace tools {

namespace {

// ==========================================
// 递归下降表达式解析器
// 成员：表达式字符串 expression_、当前扫描位置（游标） position_{0}
// 支持：加减乘除、括号、正负号、小数
// 优先级：括号/正负号 > 乘除 > 加减
// ==========================================
class ExpressionParser {
public:
    explicit ExpressionParser(std::string expression)
        : expression_(std::move(expression)) {}

    // 入口：解析整个表达式，解析完后检查是否还有多余字符
    double parse() {
        const double value = parseExpression();
        skipSpaces();

        if (position_ != expression_.size()) {
            throw std::runtime_error("unexpected token near position " + std::to_string(position_));
        }

        return value;
    }

private:
    // ==========================================
    // 最低优先级：处理加法和减法
    // 例如：3 + 4 - 2
    // ==========================================
    double parseExpression() {
        double value = parseTerm();  // 先解析左边更高优先级的项

        while (true) {
            skipSpaces();

            if (match('+')) {
                value += parseTerm();
            } else if (match('-')) {
                value -= parseTerm();
            } else {
                break;
            }
        }

        return value;
    }

    // ==========================================
    // 中间优先级：处理乘法和除法
    // 例如：2 * 3 / 4
    // ==========================================
    double parseTerm() {
        double value = parseFactor();  // 先解析左边更高优先级的因子

        while (true) {
            skipSpaces();

            if (match('*')) {
                value *= parseFactor();
            } else if (match('/')) {
                const double divisor = parseFactor();
                if (std::abs(divisor) < 1e-12) {  // 除数接近 0 就报错
                    throw std::runtime_error("division by zero");
                }
                value /= divisor;
            } else {
                break;
            }
        }

        return value;
    }

    // ==========================================
    // 最高优先级：处理正负号和括号
    // 例如：-5、+3、(1 + 2)
    // ==========================================
    double parseFactor() {
        skipSpaces();

        // 正号，直接忽略继续解析
        if (match('+')) {
            return parseFactor();
        }

        // 负号，取反
        if (match('-')) {
            return -parseFactor();
        }

        // 括号，递归回 parseExpression 处理括号内的完整表达式
        if (match('(')) {
            const double value = parseExpression();
            skipSpaces();

            if (!match(')')) {
                throw std::runtime_error("missing closing parenthesis");
            }

            return value;
        }

        // 都不是，那就应该是一个数字
        return parseNumber();
    }

    // ==========================================
    // 解析数字（整数或小数）
    // 例如：42、3.14
    // ==========================================
    double parseNumber() {
        skipSpaces();

        const std::size_t start = position_;

        // 向前扫描所有数字字符和小数点
        bool has_digit = false;
        while (
            position_ < expression_.size() &&
            (std::isdigit(static_cast<unsigned char>(expression_[position_])) ||
             expression_[position_] == '.')
        ) {
            if (std::isdigit(static_cast<unsigned char>(expression_[position_]))) {
                has_digit = true;
            }
            position_++;
        }

        // 如果一个数字都没扫到，说明表达式有语法错误
        if (!has_digit) {
            throw std::runtime_error("expected number near position " + std::to_string(start));
        }

        // 截取数字子串，转成 double
        return std::stod(expression_.substr(start, position_ - start));
    }

    // 跳过空格
    void skipSpaces() {
        while (
            position_ < expression_.size() &&
            std::isspace(static_cast<unsigned char>(expression_[position_]))
        ) {
            position_++;
        }
    }

    // 如果当前字符匹配预期，就消费掉并返回 true
    bool match(char expected) {
        if (position_ < expression_.size() && expression_[position_] == expected) {
            position_++;
            return true;
        }

        return false;
    }

private:
    std::string expression_;      // 待解析的表达式字符串
    std::size_t position_{0};     // 当前扫描位置（游标）
};

} // namespace

// ==========================================
// CalculatorTool 的接口实现
// ==========================================

// 返回工具名，用于 ToolRegistry 注册和查找
std::string CalculatorTool::name() const {
    return "calculator";
}

// 执行入口：从 input 中取出表达式字符串，解析并计算
agent::ToolResult CalculatorTool::execute(
    const nlohmann::json& input,
    agent::ToolContext& context
) {
    (void) context;  // 计算器不需要会话数据，显式标记未使用，避免编译器警告

    // 输入校验：必须有 expression 字段且为字符串
    if (!input.contains("expression") || !input["expression"].is_string()) {
        return agent::ToolResult::fail("calculator input.expression must be a string");
    }

    const std::string expression = input["expression"].get<std::string>();

    try {
        ExpressionParser parser(expression);
        const double result = parser.parse();

        // 成功：返回原始表达式和计算结果
        return agent::ToolResult::ok({
            {"expression", expression},
            {"result", result}
        });
    } catch (const std::exception& ex) {
        // 解析或计算出错（语法错误、除以零等）
        return agent::ToolResult::fail(ex.what());
    }
}

} // namespace tools