#include "tools/feedback_parser_tool.hpp"

#include <string>
#include <vector>

namespace tools {

namespace {

bool contains(const std::string& text, const std::string& keyword) {
    return text.find(keyword) != std::string::npos;
}

} // namespace

std::string FeedbackParserTool::name() const {
    return "feedback_parser";
}

agent::ToolResult FeedbackParserTool::execute(
    const nlohmann::json& input,
    agent::ToolContext& context
) {
    (void) context;

    if (!input.contains("feedback") || !input["feedback"].is_string()) {
        return agent::ToolResult::fail("feedback_parser input.feedback must be a string");
    }

    const std::string feedback = input["feedback"].get<std::string>();

    std::vector<std::string> positive_tags;
    std::vector<std::string> negative_tags;
    std::vector<std::string> preferred_categories;
    std::vector<std::string> rejected_categories;

    // 阶段 1：识别负反馈
    if (contains(feedback, "不要") || contains(feedback, "不想") || contains(feedback, "不喜欢")) {
        if (contains(feedback, "课程")) {
            negative_tags.push_back("课程");
            rejected_categories.push_back("course");
        }

        if (contains(feedback, "基础")) {
            negative_tags.push_back("基础");
            rejected_categories.push_back("beginner");
        }

        if (contains(feedback, "入门")) {
            negative_tags.push_back("入门");
            rejected_categories.push_back("beginner");
        }
    }

    // 阶段 2：识别用户进一步想要的方向
    if (contains(feedback, "真实工程") || contains(feedback, "工程")) {
        positive_tags.push_back("真实工程");
        positive_tags.push_back("工程实践");
        preferred_categories.push_back("engineering_practice");
    }

    if (contains(feedback, "系统设计")) {
        positive_tags.push_back("系统设计");
    }

    if (contains(feedback, "后端")) {
        positive_tags.push_back("后端");
    }

    nlohmann::json output = {
        {"feedback", feedback},
        {"feedback_type", "preference_update"},
        {"positive_tags", positive_tags},
        {"negative_tags", negative_tags},
        {"preferred_categories", preferred_categories},
        {"rejected_categories", rejected_categories}
    };

    return agent::ToolResult::ok(output);
}

} // namespace tools